/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


package executors


import java.io.{File, FileOutputStream, IOException}

import datasources.InternalRowConverter

import helpers.{Config, ScalaExpressionConverter}
import io.grpc.steamdrill.SDRelation
import io.grpc.steamdrill.SDQuery
import jni.ShmIter

//import scala.collection.{Iterator, Seq, Map}
import scala.collection.mutable.HashMap
import scala.collection.JavaConverters._
import org.apache.spark.{InterruptibleIterator, Partition, SparkContext, SparkException, TaskContext, broadcast}
import org.apache.spark.internal.Logging
import org.apache.spark.rdd.RDD
import org.apache.spark.sql.catalyst.InternalRow
import org.apache.spark.sql.catalyst.errors.attachTree
import org.apache.spark.sql.catalyst.expressions.{Alias, Attribute, AttributeReference, Expression, GenericInternalRow, Literal, NamedExpression}
import org.apache.spark.sql.catalyst.plans.physical.{BroadcastDistribution, Distribution, IdentityBroadcastMode}
import org.apache.spark.sql.execution.{SQLExecution, SparkPlan}
import org.apache.spark.sql.execution.metric.{SQLMetric, SQLMetrics}
import org.apache.spark.sql.types.{DataType, DataTypes}
import org.apache.spark.unsafe.types.UTF8String



// Class to implement an OmniTable Scan. DataSourceV2 doens't work b/c OT Scans are grouped
case class OmniTableScanExec(
  round : Int,
  replay : String,
  exitClock : Long,
  children : Seq[SparkPlan],
  filterExprs : Seq[Seq[Expression]],
  outputExprs : Seq[Seq[NamedExpression]],
  joinFilters : Seq[Seq[Expression]],
  //joinSchema  : Seq[Seq[Attribute]],
  joinSchema  : Seq[Seq[Attribute]],
  refNum : Attribute = AttributeReference("ScanIdx", DataTypes.IntegerType)())
  extends SparkPlan with Logging {

  override def requiredChildDistribution: Seq[Distribution] =
    Seq.fill(children.size)(BroadcastDistribution(IdentityBroadcastMode))

  override lazy val metrics = Map(
    "conversionTime" -> SQLMetrics.createTimingMetric(sparkContext, "conversion time"))


  private var cachedRDD: OmniTableRDD = null

  override protected def doExecute(): OmniTableRDD = attachTree(this, "execute") {
    val conversion = longMetric("conversionTime")

    if (cachedRDD == null) {
      logInfo(s" $this OmniTableScanExec: Creating a new RDD")


      val broadcasts = children.par.map(_.executeBroadcast[Array[InternalRow]]().value)
      cachedRDD = OmniTableRDD(sparkContext, round, replay, exitClock, output,
        outputExprs, filterExprs, joinFilters, joinSchema, broadcasts.seq,
        conversion)
    }
    else {
      logInfo(s" $this OmniTableScanExec: Reusing a known RDD")
    }
    cachedRDD
  }

  override def output: Seq[Attribute] = refNum :: Nil ++ outputExprs.flatten.distinct.map(_.toAttribute)
}

case class OmniTablePartition(index: Int) extends Partition with Serializable

trait OmniTableIterator extends Iterator[InternalRow]{
  def filter(scanIdx: Int) : Unit
}


case class OmniTableRDD (
  @transient sc : SparkContext,
  round : Int,
  replay : String,
  exitClock : Long,
  output: Seq[NamedExpression],
  scanOutputs : Seq[Seq[NamedExpression]],
  filters : Seq[Seq[Expression]],
  joins : Seq[Seq[Expression]],
  joinSchemas : Seq[Seq[Attribute]],
  rows: Seq[Array[InternalRow]],
  conversion: SQLMetric,
  numHosts: Int = Config.numCores) extends RDD[InternalRow](sc, Nil) {

  val outTypes: Seq[DataType] = output.map(_.dataType)
  var joinRows : Array[InternalRow] = Array.empty

  def buildRelations (
    output: Seq[NamedExpression],
    filters: Seq[Expression],
    joins: Seq[Expression],
    joinSchema: Seq[Attribute],
    rows : Array[InternalRow],
    index: Int,
    joinBytes : Int) : SDQuery = {

    val aliases = output.filter(_.isInstanceOf[Alias]).map(_.asInstanceOf[Alias])

    val query = SDQuery.newBuilder.setReplay(replay).setExitClock(exitClock)
    val converter = new ScalaExpressionConverter(aliases)
    val base = SDRelation.newBuilder

    for (f <- filters) {
      val res = converter.toExpression(f)
      if (res != null) {
        base.addFilters(res)
      }
    }

    for ((row, i) <- rows.zipWithIndex) {
      val nextRel = SDRelation.newBuilder.mergeFrom(base.build)
      converter.loadRow(new InternalRowConverter(joinSchema.asJava, row))

      for (c <- output) {
        // this is now creating something that is
        // I think this is just converting something into...
        nextRel.addCols(converter.toExpression(c))
      }

      // add in the join Column
      val ty = if (joinBytes == 2) {
        DataTypes.ShortType
      } else {
        assert (joinBytes == 4)
        DataTypes.IntegerType
      }
      nextRel.addCols(converter.toExpression(new Literal (i, ty)))


      for (join <- joins) {
        val res = converter.toExpression(join)
        if (res != null) {
          nextRel.addFilters(res)
        }
      }
      query.addRels(nextRel)
      //logError(s"nextRel ${nextRel.toString}")
    }
    for ((_, v) <- converter.getTracingFunctions) {
      query.addTracingCodeDefs(v);
    }


    query.build
  }


  def getJoinBytes(size : Int) =
    if (size == 0)
      0
    else if (size < 65535)
      2
    else
      4

  override def compute(split: Partition, context: TaskContext): OmniTableIterator = {

    // This is where we implement everything! This function is executed on the correct machine.
    val flatTuples = (scanOutputs zip filters zip joins zip joinSchemas zip rows) map {
      case ((((o, f), j), js), r) => (o, f, j, js, r)
    }

    var partialRowCount : Int = 0
    val shmIters = flatTuples.zipWithIndex.map(pair => {
      val desOutputs = pair._1._1
      val joinSchema = pair._1._4
      val rows = pair._1._5
      val index = pair._2

      //val scan = desOutputs.filter(out => !joinSchema.contains(out.toAttribute))
      val scan = desOutputs.filter(out => !joinSchema.exists(_.exprId == out.exprId))

      val scanTypes = scan.map(_.dataType).toArray
      val oMap : Seq[Int] = scan.map(o => output.indexWhere(_.semanticEquals(o.toAttribute)))
      val jMap = joinSchema.map(out => output.indexWhere(_.semanticEquals(out.toAttribute)))

			val shmName = Config.getShmName(replay, round, numHosts, split.index, index)
      val joinBytes = getJoinBytes(rows.size)
      val shmIter = new ShmIter(shmName, scanTypes.map(_.typeName), joinBytes, partialRowCount);

      joinRows = joinRows ++ rows
      partialRowCount += rows.size
      (shmIter, oMap, jMap)
    })


    val wasCreated = shmIters.map(_._1.getCreated).head

    if (wasCreated) {
      logInfo(s"${shmIters.map(_._1.getFd).mkString(", ")} created the shared memories")

      // index and all of that needs to be sensible.
      val commands = flatTuples.zipWithIndex.map (pair => {
        val group = pair._1
        val index = pair._2
        val myJoinOutput = group._1
        val myJoinSchema = group._4
        val rows = group._5
        val joinBytes = getJoinBytes(rows.size)

        // n^2, but probably okay given the size??
        val scan = myJoinOutput.filter(out => !myJoinSchema.exists(_.exprId == out.exprId))
        val relations = buildRelations(scan, group._2, group._3, myJoinSchema, rows, index, joinBytes)
        val commands = Config.getCommands(replay, round, numHosts, split.index, index)
        // write out the SDQuery to the file system.
        try {
          val fos = new FileOutputStream(commands)
          relations.writeTo(fos)
          fos.close()
        } catch {
          case e: IOException =>
            e.printStackTrace()
        }
        commands
      })

      logInfo("Starting execution!\n")

      val commandArgs : Seq[String] =
        Config.SteamdrillProcess :: Config.getRoundStats(replay, round, numHosts, split.index) ::
          s"$round" :: s"$numHosts" :: s"${split.index}" :: Config.getEpochBase(replay, round, numHosts, split.index) ::
          s"-c${Config.getJumpCounterType}" :: s"-l${Config.getLogLevel}" :: Nil ++ commands

      try {
        val pb = new ProcessBuilder(commandArgs.asJava)
          .redirectError(new File(Config.getRoundStdout(replay, round, numHosts, split.index)))
          .redirectOutput(new File(Config.getRoundStderr(replay, round, numHosts, split.index)))
        val env = pb.environment
        env.put("LD_LIBRARY_PATH", Config.LdLibraryPath())
        pb.start
      } catch {
        case e: IOException =>
          e.printStackTrace()
      }
    }

    // debug stuff
    //for (group <- shmIters.zipWithIndex) {
    //log.error(s"index ${group._2} has size ${group._1._1.getTypesSize()}")
  //}

    val iters = shmIters.zip(joinSchemas).zipWithIndex.map(set => {
      val rowIter = set._1._1._1
      val outputMap = set._1._1._2
      val joinMap = set._1._1._3
      val joinSchema = set._1._2
      val index = set._2

      (index, new Iterator[InternalRow] {
        var count : Int = 0
        var conversionStart : Long = 0;


        // the known and new values will always be contiguous, so can't we optimize with that and
        // elide a round of iteration?
        def createRow(joinIdx: Int) : Array[Any] = {
          val row = Array.fill[Any](outTypes.size)(null)
          val joinRow = joinRows(joinIdx)
          for ((dest, idx) <- joinMap.zipWithIndex) {

            if (dest > 0) {
              val value = joinRow.get(idx, joinSchema(idx).dataType)
              row(dest) = value
            }
          }
          row
        }

        val joinCache: HashMap[Int, Array[Any]] = HashMap[Int, Array[Any]]()
        def getJoinRow(joinIdx: Int) : Array[Any] = {
          //since we know that you're going to muck up everything in this row, no need to
          //recreate it if we already have it:
          try {
            joinCache.getOrElse(joinIdx, createRow(joinIdx))
          }
          catch {
            case e: ArrayIndexOutOfBoundsException =>
              {
                log.error(s"parsed ${count} rows")
                throw e
              }
          }
        }

        def utfWrapper(s : Any) = UTF8String.fromString(s.asInstanceOf[String])
        def identity(a : Any) = a

        lazy val outputFunctions : Array[(Any) => Any] =
          outTypes.map(t => {
            if (t.typeName.startsWith("st")) {
              utfWrapper _
            } else {
              identity _
            }
          }).toArray

        override def hasNext: Boolean = {
          val next: Boolean = rowIter.hasNext
          if (count == 0)
            conversionStart = System.nanoTime
          if (!next)
            conversion += (System.nanoTime - conversionStart);
          next
        }
        override def next(): InternalRow = {
          val scanObjects = rowIter.next
          val joinKey = rowIter.getJoinRowsOffset() + scanObjects.last.asInstanceOf[Integer]

          // important to figure out join key type:

          count += 1
          val row = getJoinRow(joinKey)
          row(0) = index

          for ((destIdx, index)  <- outputMap.zipWithIndex) {
            row(destIdx) =  outputFunctions(destIdx)(scanObjects(index))
          }
          //logDebug(s"""row ${row.mkString(", ")} for ${output.mkString(", ")}""");
          new GenericInternalRow(row)
        }
      })
    })

    context.addTaskCompletionListener[Unit](_ => for (elem <- shmIters) elem._1.close())


    new OmniTableIterator {
      var onlyScan : Int = -1

      lazy val iterator : InterruptibleIterator[InternalRow] = {
        val it = if (onlyScan >= 0) {
          logTrace(s"Filtering until $onlyScan!")
          iters.find(_._1 == onlyScan).get._2
        }
        else {
          iters.foldLeft(Iterator[InternalRow]()) {
            (first, second) => first ++ second._2
          }
        }
        new InterruptibleIterator(context, it)
      }

      def filter(index : Int) = { onlyScan = index}
      def hasNext : Boolean = iterator.hasNext
      def next = iterator.next()
    }
  }

  lazy val partitionMap : Map[Int, String] = {
    val sc = SparkContext.getOrCreate()
		val driver = sc.getConf.get("spark.driver.host")
		val plusDriver = sc.getExecutorMemoryStatus.keys

		val al : Iterable[String] =	if (plusDriver.size > 1) {
				plusDriver.filter(! _.split(":")(0).equals(driver))
		} else {
				plusDriver
		}

		val allExec = al.map(_.split(":")(0))

    // assumes that default parallelism includes across ALL execs
		val coresPerHost = sc.defaultParallelism / allExec.size
    val unrolledHosts = allExec.map(string => {
				List.fill(coresPerHost)(string)
		}).toList.flatten

		// logError(s"unrolledHosts size ${unrolledHosts.size} ${unrolledHosts.mkString(", ")}")
		((0 until Config.numCores) zip unrolledHosts.take(Config.numCores)).toMap
  }
  override def getPreferredLocations(split: Partition): Seq[String] = {
    partitionMap(split.index) :: Nil
  }
  override protected def getPartitions: Array[Partition] = {
		(0 until Config.numCores).map(OmniTablePartition).toArray
  }
}

