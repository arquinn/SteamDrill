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

package helpers

import java.io.InputStream

import datasources.{ProgramDataSource, Schema}
import logical.OmniTableLogicalPlan
import optimizers.{CleanupProj, OrderGenerators, ProjDown, PushFilterThroughOrderedJoin, SimpBCastRule, NestedGenPushdown}
import planners.{DataSourceV2StrategyExtended, OmniTableScanMerger, OmniTableStrategy, OrderedJoinStrategy, RoundifyStrategy, NestedGeneratorStrategy}
import org.apache.spark.sql.{DataFrame, Dataset, SparkSession}
import org.apache.spark.SparkConf
import org.apache.spark.sql.functions.struct

import collection.JavaConverters._



class OmniTable(val replay : String, val exit : Long = 0) {

  private val GENERATOR_NAME: String = "datasources.ProgramDataSource"

  val sparkSession: SparkSession = {
//    val conf = new SparkConf().set("spark.serializer", "org.apache.spark.serializer.KryoSerializer")

    val temp = SparkSession.builder().appName("SDQuery")
//      .config(conf)
      .withExtensions(e => {
        e.injectOptimizerRule(_ => PushFilterThroughOrderedJoin)
        e.injectOptimizerRule(_ => ProjDown)
        e.injectOptimizerRule(_ => OrderGenerators)
        e.injectOptimizerRule(_ => SimpBCastRule)
        e.injectOptimizerRule(_ => NestedGenPushdown)
        e.injectOptimizerRule(_ => CleanupProj)



        e.injectPlannerStrategy(_ => RoundifyStrategy)
        e.injectPlannerStrategy(_ => NestedGeneratorStrategy)
        e.injectPlannerStrategy(_ => OmniTableStrategy)
        e.injectPlannerStrategy(_ => DataSourceV2StrategyExtended)
        e.injectPlannerStrategy(_ => OrderedJoinStrategy)

        e.injectPrepareExectionRule(_ => OmniTableScanMerger())
      })
      .getOrCreate()

    // the old way to extend. places opts in a different order, might be useful later?
    // temp.experimental.extraOptimizations = SteamdrillOptimizer.optimizers
    // temp.experimental.extraStrategies = DataSourceV2StrategyExtended :: OrderedJoinStrategy :: Nil
    temp
  }

  import sparkSession.implicits._

  def apply(): DataFrame = {
    /*
    sparkSession.read.format(OT_NAME)
      .option(OmniTableDataSource.REPLAY_KEY, replay)
      .option(OmniTableDataSource.EXIT_CLOCK, exit)
      .load()
      .withColumn("order", struct($"REPLAY_CLOCK", $"LOGICAL_CLOCK", $"EIP"))
    */
    val cl = Thread.currentThread.getContextClassLoader
    val schemaStream = cl.getResourceAsStream("schema.txt")
    val attrs = Schema.fromStream(schemaStream).getStructType.toAttributes

    // I *guess* include this here? seems sloppy
    Dataset.ofRows(sparkSession, new OmniTableLogicalPlan(attrs, replay, exit))
    .withColumn("order", struct($"REPLAY_CLOCK", $"LOGICAL_CLOCK", $"EIP"))
  }

  def gen(exec: String, input: String = ""): DataFrame = {
    val temp = sparkSession.read.format(GENERATOR_NAME)
       .option(ProgramDataSource.EXECUTABLE_KEY, s"${Config.staticFile(exec)}")
       .option(ProgramDataSource.SCHEMA_KEY, s"${Config.staticFile(s"$exec.schema")}")

    if (!input.isEmpty) {
      temp.option(ProgramDataSource.INPUT_KEY, input)
    }

    temp.load()
  }

  def stop() {
    sparkSession.stop()
  }
}
