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


import logical.{LeftNextOJT, LeftStackOJT, NextOJT, OrderedJoinType, PrevOJT, StackLike}

import scala.collection.mutable.ListBuffer
import org.apache.spark.internal.Logging
import org.apache.spark.rdd.RDD
import org.apache.spark.sql.catalyst.InternalRow
import org.apache.spark.sql.catalyst.expressions.{Ascending, Attribute, Expression, GenericInternalRow, JoinedRow, Projection, SortOrder, UnsafeProjection}
import org.apache.spark.sql.catalyst.plans.physical.{Distribution, HashClusteredDistribution, Partitioning, PartitioningCollection}
import org.apache.spark.sql.execution.{BinaryExecNode, RowIterator, SparkPlan}
import org.apache.spark.sql.execution.metric.SQLMetrics
import org.apache.spark.sql.types.DataType

/**
 * Performs an ordered join of two child relations.
 */

case class OrderedJoinExec(
  leftEqiKeys: Seq[Expression],
  rightEqiKeys: Seq[Expression],
  leftOrderKeys: Seq[Expression],
  rightOrderKeys: Seq[Expression],
  joinType : OrderedJoinType,
  left: SparkPlan,
  right: SparkPlan) extends BinaryExecNode {

  override lazy val metrics = Map(
    "numOutputRows" -> SQLMetrics.createMetric(sparkContext, "number of output rows"))

  lazy val leftKeys: Seq[Expression] = leftEqiKeys ++ leftOrderKeys
  lazy val rightKeys: Seq[Expression] = rightEqiKeys ++ rightOrderKeys

  lazy val leftTypes : Seq[DataType] = left.output.map(_.dataType)
  lazy val rightTypes : Seq[DataType] = right.output.map(_.dataType)


  override def output: Seq[Attribute] = (left.output ++ right.output).map(_.withNullability(true))

  override def outputPartitioning: Partitioning =
    PartitioningCollection(Seq(left.outputPartitioning, right.outputPartitioning))

  override def requiredChildDistribution: Seq[Distribution] =
    HashClusteredDistribution(leftEqiKeys) :: HashClusteredDistribution(rightEqiKeys) :: Nil
  // this seems broken?


  override def outputOrdering: Seq[SortOrder] = {
    // For inner join, orders of both sides keys should be kept.

    val leftKeyOrdering = getKeyOrdering(leftKeys, left.outputOrdering)
    val rightKeyOrdering = getKeyOrdering(rightKeys, right.outputOrdering)
    leftKeyOrdering.zip(rightKeyOrdering).map { case (lKey, rKey) =>
      // Also add the right key and its `sameOrderExpressions`
      SortOrder(lKey.child, Ascending, lKey.sameOrderExpressions + rKey.child ++ rKey
        .sameOrderExpressions)
    }
  }

  /**
   * The utility method to get output ordering for left or right side of the join.
   *
   * Returns the required ordering for left or right child if childOutputOrdering does not
   * satisfy the required ordering; otherwise, which means the child does not need to be sorted
   * again, returns the required ordering for this child with extra "sameOrderExpressions" from
   * the child's outputOrdering.
   */
  private def getKeyOrdering(keys: Seq[Expression], childOutputOrdering: Seq[SortOrder])
  : Seq[SortOrder] = {
    val requiredOrdering = requiredOrders(keys)
    if (SortOrder.orderingSatisfies(childOutputOrdering, requiredOrdering)) {
      keys.zip(childOutputOrdering).map { case (key, childOrder) =>
        SortOrder(key, Ascending, childOrder.sameOrderExpressions + childOrder.child - key)
      }
    } else {
      requiredOrdering
    }
  }

  override def requiredChildOrdering: Seq[Seq[SortOrder]] =
    requiredOrders(leftKeys) :: requiredOrders(rightKeys) :: Nil

  private def requiredOrders(keys: Seq[Expression]): Seq[SortOrder] = {
    // This must be ascending in order to agree with the `keyOrdering` defined in `doExecute()`.

    keys.map(SortOrder(_, Ascending))
  }

  private lazy val createLeftKeyGenerator: Projection =
    UnsafeProjection.create(leftEqiKeys, left.output)

  private lazy val createRightKeyGenerator: Projection =
    UnsafeProjection.create(rightEqiKeys, right.output)

  private lazy val createLeftOrderKeyGenerator: Projection =
    UnsafeProjection.create(leftOrderKeys, left.output)

  private lazy val createRightOrderKeyGenerator: Projection =
    UnsafeProjection.create(rightOrderKeys, right.output)

  private lazy val equalOrdering : Ordering[InternalRow] =
    newNaturalAscendingOrdering(leftEqiKeys.map(_.dataType))
  private lazy val orderOrdering : Ordering[InternalRow] =
    newNaturalAscendingOrdering(leftOrderKeys.map(_.dataType))
  private lazy val resultProj : InternalRow => InternalRow =
    UnsafeProjection.create(output, output)

  protected override def doExecute(): RDD[InternalRow] = {

    logDebug(s"""orderedJoinExec DoExecute Starting! ${joinType} leftEquiKeys ${leftEqiKeys.mkString(", ")}""")
    val numOutputRows = longMetric("numOutputRows")

    // I don't think this works in the case that right is empty?
//    val leftyboi = left.execute()
//    val rightyboi = right.execute()
    left.execute().zipPartitions(right.execute()) { (leftIter, rightIter) =>

      new RowIterator {
        private[this] val scanner: OrderedJoinScanner = joinType match {
          case NextOJT => new NextJoinScanner(
            createLeftKeyGenerator,
            createRightKeyGenerator,
            equalOrdering,
            createLeftOrderKeyGenerator,
            createRightOrderKeyGenerator,
            orderOrdering,
            RowIterator.fromScala(leftIter),
            RowIterator.fromScala(rightIter),
            leftTypes,
            rightTypes)
          case LeftNextOJT => new LeftNextJoinScanner(
            createLeftKeyGenerator,
            createRightKeyGenerator,
            equalOrdering,
            createLeftOrderKeyGenerator,
            createRightOrderKeyGenerator,
            orderOrdering,
            RowIterator.fromScala(leftIter),
            RowIterator.fromScala(rightIter),
            leftTypes,
            rightTypes)
          case LeftStackOJT => new LeftStackJoinScanner(
            createLeftKeyGenerator,
            createRightKeyGenerator,
            equalOrdering,
            createLeftOrderKeyGenerator,
            createRightOrderKeyGenerator,
            orderOrdering,
            RowIterator.fromScala(leftIter),
            RowIterator.fromScala(rightIter),
            leftTypes,
            rightTypes
          )
          case _ => new StackJoinScanner(
            createLeftKeyGenerator,
            createRightKeyGenerator,
            equalOrdering,
            createLeftOrderKeyGenerator,
            createRightOrderKeyGenerator,
            orderOrdering,
            RowIterator.fromScala(leftIter),
            RowIterator.fromScala(rightIter),
            leftTypes,
            rightTypes)
        }

        private[this] val joinRow = new JoinedRow
        private[this] val nulls = new GenericInternalRow(right.output.size)

        override def advanceNext(): Boolean = {
          // logError("advanceNext called from OrderedJoinExec I guess?")
          val (currentLeftRow, currentRightRow) = scanner.findJoinRow()
          if (currentLeftRow != null) {
            numOutputRows += 1
            if (currentRightRow != null) {
              joinRow(currentLeftRow, currentRightRow)
            } else {
              joinRow(currentLeftRow, nulls)
            }
          }
          // logDebug(s"""advanceNext returning ${currentLeftRow != null}, row is ${joinRow}""")

          currentLeftRow != null;
        }
        override def getRow: InternalRow = joinType match {
          case _ => resultProj (joinRow)
        }
      }.toScala
    }
  }
}

/**
 * Helper class that is used to implement [[OrderedJoinExec]].
 *
 * To perform an join, users of this class call [[findJoinRow()]]
 * which returns `leftrow, rightrow` if a result has been produced and `(null, null)` otherwise.
 *
 * @param lKeyGen a projection that produces join keys from the streamed input.
 * @param rKeyGen a projection that produces join keys from the buffered input.
 * @param eqiKeyOrder an ordering which can be used to compare join keys.
 * @param lIter an input whose rows will be streamed.
 * @param rIter an input whose rows will be buffered to construct sequences of rows that
 *                     have the same join key.
 */
private[executors] abstract class OrderedJoinScanner(leftEqualsProj: Projection,
  rightEqualsProj: Projection,
  eqiKeyOrder: Ordering[InternalRow],
  leftOrderProj : Projection,
  rightOrderProj: Projection,
  oKeyOrder: Ordering[InternalRow],
  lIter: RowIterator,
  rIter: RowIterator,
  val leftTypes : Seq[DataType],
  val rightTypes : Seq[DataType]) extends Logging {

  protected[this] var left: InternalRow = _
  protected[this] var leftEquals: InternalRow = _
  protected[this] var leftOrder: InternalRow = _


  protected[this] var right: InternalRow = _
  protected[this] var rightEquals: InternalRow = _
  protected[this] var rightOrder: InternalRow = _


  var leftCount : Int = 0
  var rightCount : Int = 0

  // --- Public methods ---------------------------------------------------------------------------
  def findJoinRow() : (InternalRow, InternalRow)

  // --- Private methods --------------------------------------------------------------------------

  /**
   * Advance the streamed iterator and compute the new row's join key.
   * @return true if the streamed iterator returned a row and false otherwise.
   */
  protected def advancedLeft(): Boolean = {
    val hasData = lIter.advanceNext()
    if (hasData) {
      left = lIter.getRow
      leftEquals = leftEqualsProj(left)
      leftOrder = leftOrderProj(left)

      //dumpLeft("left")

      // leftCount += 1
      // logDebug(s"al count=${leftCount}, row=${left.toString}")

      true
    }
    else {
      left = null
      leftEquals = null
      leftOrder = null
      false
    }
  }

  protected def dumpLeft(msg: String): Unit = {
    val values = leftTypes.zipWithIndex.map(x => left.get(x._2, x._1))
    logError(s"$msg ${values.mkString(",")}")
  }

  protected def dumpRight(msg: String): Unit = {
    val values = rightTypes.zipWithIndex.map(x => right.get(x._2, x._1))
    logError(s"$msg ${values.mkString(",")}")
  }

  /**
   * Advance the buffered iterator until we find a row with join key that does not contain nulls.
   * @return true if the buffered iterator returned a row and false otherwise.
   */
  protected def advancedRight(): Boolean = {
    val hasData = rIter.advanceNext()

    if (hasData) {
      right = rIter.getRow
      rightEquals = rightEqualsProj(right)
      rightOrder = rightOrderProj(right)

      // rightCount += 1
      // logDebug(s"ar count=${rightCount}, row=${right.toString}")

      true
    }
    else {
      right = null
      rightEquals = null
      rightOrder = null
      false
    }
  }
}

private[executors] class PrevJoinScanner(
  leftEqualsProj: Projection,
  rightEqualsProj: Projection,
  equalsOrder: Ordering[InternalRow],
  leftOrderProj: Projection,
  rightOrderProj: Projection,
  orderOrder: Ordering[InternalRow],
  leftIter: RowIterator,
  rightIter: RowIterator,
  lTypes: Seq[DataType],
  rTypes: Seq[DataType])


  extends OrderedJoinScanner(leftEqualsProj, rightEqualsProj, equalsOrder,
                             leftOrderProj, rightOrderProj, orderOrder,
                             leftIter, rightIter, lTypes, rTypes) {

  var rightBack : InternalRow = null

  /**
   * Advances both input iterators, stopping when we have found rows with matching join keys.
   * @return left and right internal rows for a match, or (null, null) if no match exists
   */
  override  def findJoinRow() : (InternalRow, InternalRow) = {
    advancedLeft()

    // this behavior will depend upon left vs. right vs. inner vs. outter

    if (left == null || right == null) {
      left = null
      right = null
      (null, null)
    } else {
      var innerDone = false
      do {
        // left and right are ordered first by equality -- no match means no ordering match either
        val comp = equalsOrder.compare(leftEquals, rightEquals)
        val orderComp = orderOrder.compare(leftOrder, rightOrder)
        if (comp > 0) {
          rightBack = null
          advancedRight()
        }
        else if (comp < 0) {
          rightBack = null
          advancedLeft()
        }
        else if (orderComp < 0) {
          rightBack = right.copy()
          advancedRight()
        }
        else if (rightBack != null)  {
          innerDone = true;
        }
      } while (left != null && right != null && !innerDone)

      if (innerDone) {
        (left, rightBack)
      }
      else {
        left = null
        right = null
        (left, rightBack)
      }
    }
  }
}

private[executors] class NextJoinScanner(
  lKeyG: Projection,
  rKeyG: Projection,
  eqiKeyO: Ordering[InternalRow],
  lOKeyGen : Projection,
  rOKeyGen: Projection,
  oKeyOrder: Ordering[InternalRow],
  lIter: RowIterator,
  rIter: RowIterator,
  lTypes: Seq[DataType],
  rTypes: Seq[DataType])
  extends OrderedJoinScanner(lKeyG, rKeyG, eqiKeyO, lOKeyGen, rOKeyGen, oKeyOrder, lIter, rIter, lTypes, rTypes) {
  /**
   * Advances both input iterators, stopping when we have found rows with matching join keys.
   * @return left and right internal rows for a match, or (null, null) if no match exists
   */
  override  def findJoinRow() : (InternalRow, InternalRow) = {
    // is this vv correct? should I always advance? I don't think so??
    advancedLeft()
    advancedRight()

    // this behavior somewhat depends on left v. right v. inner v. outer,

    if (left == null) {
      // We have consumed the entire streamed iterator, so there can be no more matches.
      right = null
      (null, null)
    } else if (right == null) {
      // There are no more rows to read from the buffered iterator, so there can be no more matches.
      left = null
      (null, null)
    } else {
      // Advance both the streamed and buffered iterators to find the next pair of matching rows.
      var comp = 0
      do {
        // left and right are ordered first by equality -- no match means no ordering match either
        comp = eqiKeyO.compare(leftEquals, rightEquals)
        // logDebug(s"""comp between ${leftRow.toString} and ${rightRow.toString} is ${comp}""")
        if (comp > 0) advancedRight()
        else if (comp < 0) advancedLeft()

        // check to see that right is after the left
        else {
          val oComp = oKeyOrder.compare(leftOrder, rightOrder)
          // logDebug(s"""oComp is ${oComp}""")
          if (oComp >= 0) {
            comp = 1
            advancedRight()
          }
        }
      } while (left!= null && right!= null && comp != 0)

      if (left== null || right== null) {
        // We have hit the end of one of the iterators, so there can be no more matches.
        left = null
        right = null
        (null, null)
      } else {
        (left, right)
      }
    }
  }
}



private[executors] class LeftNextJoinScanner(
  lKeyG: Projection,
  rKeyG: Projection,
  eqiKeyO: Ordering[InternalRow],
  lOKeyGen : Projection,
  rOKeyGen: Projection,
  oKeyOrder: Ordering[InternalRow],
  lIter: RowIterator,
  rIter: RowIterator,
  lTypes: Seq[DataType],
  rTypes: Seq[DataType])
  extends OrderedJoinScanner(lKeyG, rKeyG, eqiKeyO, lOKeyGen, rOKeyGen, oKeyOrder, lIter, rIter, lTypes, rTypes) {
  /**
   * Advances both input iterators, stopping when we have found rows with matching join keys.
   * @return left and right internal rows for a match, or (null, null) if no match exists
   */

  // we need a status variable to tell us if we're draining left
  var drainLeft = false
  override  def findJoinRow() : (InternalRow, InternalRow) = {

    advancedLeft()

    if (!drainLeft) {
      advancedRight()
    }

    // logDebug(s"findJoinRow for ${if (leftRow != null) leftRow.toString else "none"}")

    if (left== null) {
      // We have consumed the entire streamed iterator, so there can be no more matches.
      right = null
      (null, null)
    } else if (right== null) {
      // There are no more rows to read from the buffered iterator, so return the left
      // logDebug("finished with rightRows!")
      drainLeft = true
      (left, null)
    } else {

      var comp = 0
      drainLeft = false
      do {
        // left and right are ordered first by equality -- no match means no ordering match either
        comp = eqiKeyO.compare(leftEquals, rightEquals)
        // logDebug(s"""comp between ${leftRow.toString} and ${rightRow.toString} is ${comp}""")
        if (comp > 0) {
          advancedRight()
        }
        else if (comp == 0) {
          val oComp = oKeyOrder.compare(leftOrder, rightOrder)
          // logDebug(s"""ocomp between ${leftRow.toString} and ${rightRow.toString} is ${oComp}""")

          if (oComp >= 0) {
            comp = 1
            advancedRight()
          }
        }
        else {
          // comp < 0 -> means that the right is done with matches for this section of lefts
          drainLeft = true
        }
      } while (left!= null && right!= null && comp > 0)
      if (drainLeft) {
        (left, null)
      } else {
        (left, right)
      }
    }
  }
}


private[executors] class StackJoinScanner(
  lKeyG: Projection,
  rKeyG: Projection,
  eqiKeyO: Ordering[InternalRow],
  lOKeyGen : Projection,
  rOKeyGen: Projection,
  oKeyO: Ordering[InternalRow],
  lIter: RowIterator,
  rIter: RowIterator,
  lTypes: Seq[DataType],
  rTypes: Seq[DataType])
  extends OrderedJoinScanner(lKeyG, rKeyG, eqiKeyO, lOKeyGen, rOKeyGen, oKeyO, lIter, rIter, lTypes, rTypes)
    with Logging {

  // in scala.. you can just, shove this here and it gets called on object construction (!)
  advancedLeft()

  // in this world, leftRow and all that jazz points to the row AFTER the current potential match

  protected[this] var leftStack: ListBuffer[InternalRow] = ListBuffer.empty[InternalRow]
  protected[this] var rightDone: Boolean = false;


  /**
   * Advances both input iterators, stopping when we have found rows with matching join keys.
   *
   * @return left and right internal rows for a match, or (null, null) if no match exists
   *
   *         I think this works for... inner joins?
   */
  def findJoinRow(): (InternalRow, InternalRow) = {
    // on a match I *have* to advance the right iter ?
    var leftMatch: InternalRow = null


    if (!rightDone) {
      do {
        rightDone = !advancedRight()
        if (right!= null) {
          logDebug(s"""next rightRow ${right.toString}""")
        }
        var innerDone = false;
        // push all equal and ordered less left rows onto the stack
        while (!innerDone && left!= null && right!= null) {

          // ordered first by equiKey, so non match means they'll never match the stack.
          // again, this depends on left v. right v. inner v. outter
          val comp = eqiKeyO.compare(leftEquals, rightEquals)

          logDebug(s"""equiCompare $leftEquals vs. $rightEquals is ${comp}""")
          if (comp == 0) {
            val oComp = oKeyO.compare(leftOrder, rightOrder)
            logDebug(s"""oComp is ${oComp}""")
            if (oComp < 0) {
              logDebug(s"""push ${left.toString} to stack ${leftStack.size}""")
              // bad things happen if you don't copy (leftis a var after all :()
              leftStack.prepend(left.copy())
              advancedLeft()
            } else {
              innerDone = true
            }
          } else if (comp < 0) {
            advancedLeft()
          } else {
            innerDone = true
            // don't I need to clear out the stack in this case...???
          }
        }
        // if there is something at the top of the stack, it's our potential match:

        leftStack.foreach((ir) => {
          val leqi = lKeyG(ir)
          logDebug(s"""stack has $leqi vs. $rightEquals""")
          assert(eqiKeyO.compare(leqi, rightEquals) == 0)
        })

        if (left!= null) {
          leftEquals = lKeyG(left)
        }
        if (!leftStack.isEmpty) {
          leftMatch = leftStack.head
          leftStack.trimStart(1)
        }
        if (left!= null) {
          logDebug(s"""leftRowKey $left""")
        }
        // there should be an else here depending on left v. right v. inner v. outer
      } while (leftMatch == null && right!= null)
    }

    // recompute in case right done b/c of a loop:
    if (rightDone) {
      // flush out the stack and then the left iter:
      if (!leftStack.isEmpty) {
        leftMatch = leftStack.head
        leftStack.trimStart(1)
      }
      else {
        leftMatch = left
        advancedLeft()
      }
    }

    if (leftMatch != null && right!= null) {
      logDebug(s"""return match ${leftMatch.toString} ${right.toString}""")
    }
    else {
      logDebug(s"""donzo with the findJoinRow""")
    }

    if (left!= null) {
      logDebug(s"""leftRowKey $left""")
    }
    (leftMatch, right)
  }
}

private[executors] class LeftStackJoinScanner(
  leftEqualsProj: Projection,
  rightEqualsProj: Projection,
  equalsOrder: Ordering[InternalRow],
  leftOrderProj : Projection,
  rightOrderProj: Projection,
  orderOrder : Ordering[InternalRow],
  leftIter: RowIterator,
  rightIter: RowIterator,
  lTypes: Seq[DataType],
  rTypes: Seq[DataType]) extends
    OrderedJoinScanner(leftEqualsProj, rightEqualsProj, equalsOrder,
      leftOrderProj, rightOrderProj, orderOrder,
      leftIter, rightIter,
      lTypes, rTypes) with Logging{

  // in scala.. you can just, shove this here and it gets called on object construction (!)
  advancedLeft()

  // in this world, leftRow and all that jazz points to the row AFTER the current potential match

  protected[this] var stack: ListBuffer[InternalRow] = ListBuffer.empty[InternalRow]

  protected[this] var topEquals: InternalRow = _
  protected[this] var topOrder: InternalRow = _

  protected[this] var rightPause: Boolean = false

  /**
   * Advances both input iterators, stopping when we have found rows with matching join keys.
   *
   * @return left and right internal rows for a match, or (null, null) if no match exists
   *
   */
  def findJoinRow(): (InternalRow, InternalRow) = {
    var leftMatch: InternalRow = null


    if (left == null) {
      // I think this basically means that we were an empty partition(?)
      // but, if there were right party people, let's print one of dem:
//      advancedRight()
//      if (right != null) {
//        val values = rightTypes.zipWithIndex.map(x => right.get(x._2, x._1))

//        logError(s"""unmatched right:
//                     |num   ${right.numFields}
//                     |row   ${right.toString}
//                     |equi  ${rightEquals.toString}
//                     |order ${rightOrder.toString}
//                     |right ${values.mkString(", ")}""")
        // it would be helpful to be able to see what right order and right equals
        // actually are. For these purposes, we will add in some additional variables!

      //      }
//      logError(s"left is null at findJoinRow")
      if (stack.nonEmpty) {
        leftMatch = stack.head
        stack.trimStart(1)
//        val values = leftTypes.zipWithIndex.map(x => leftMatch.get(x._2, x._1))
//        logError(s"null lefty ${values.mkString(",")}")
        (leftMatch, null)
      }
      else {
        (left, null)
      }
    }
    else if (rightPause) {
      if (stack.nonEmpty) {
        leftMatch = stack.head
        stack.trimStart(1)
        rightPause = stack.nonEmpty
//        if (leftMatch != null) {
//          val values = leftTypes.zipWithIndex.map(x => leftMatch.get(x._2, x._1))
//          logError(s"rightPause ${values.mkString(",")}")
//        }
        (leftMatch, null)
      }
      else {
        // this means that rightPause is *really* rightDone
        advancedLeft()
//        if (left != null)
//          dumpLeft("left under rightPause.. done")
        (left, null)
      }
    }
    else {
      do {
        advancedRight()

        var innerDone = false;
        // push all less than left rows (both eqi and ordered) onto the stack
        while (!innerDone && left != null && right != null) {

          // ordered first by equiKey, so non match means they'll never match the stack.
          val comp = equalsOrder.compare(leftEquals, rightEquals)
          val orderComp = orderOrder.compare(leftOrder, rightOrder)

          if (comp < 0 || (comp == 0 && orderComp <= 0)) {
            stack.prepend(left.copy())
            advancedLeft()
          }
          else {
            innerDone = true
          }
        }

        // always try to pop from the stack.
        if (stack.nonEmpty) {
          leftMatch = stack.head
          stack.trimStart(1)

//          val values = leftTypes.zipWithIndex.map(x => leftMatch.get(x._2, x._1))
//          logError(s"leftMatch ${values.mkString(",")}")
        }
      } while (leftMatch == null && right != null)


      if (right != null && equalsOrder.compare(leftEqualsProj(leftMatch), rightEquals) == 0) {
//        val values = leftTypes.zipWithIndex.map(x => leftMatch.get(x._2, x._1))
//        loogError(s"matchyBoii ${values.mkString(",")}")

        (leftMatch, right)
      }
      else {
        // when the leftMatch is not equal to the right, then all stack elements aren't equal.
        // so, clear out the stack!
        rightPause = (right != null && stack.nonEmpty) || (right == null && left != null)

//        logError("I want to break free!!")
        if (leftMatch == null) {

//          if (left != null) {
//            dumpLeft("cleaning left ")
//          }
          (left, null)
        }
        else {
//          val values = leftTypes.zipWithIndex.map(x => leftMatch.get(x._2, x._1))
//          logError(s"matchyBoii staxx ${values.mkString(",")}")
          (leftMatch, null)
        }
      }
    }
  }
}
