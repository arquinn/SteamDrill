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


package planners

import executors.OrderedJoinExec
import logical.{OrderedJoin, OrderedJoinType}
import org.apache.spark.internal.Logging
import org.apache.spark.sql.catalyst.expressions.{And, Coalesce, EqualNullSafe, EqualTo, Expression, Literal, PredicateHelper, RowOrdering}
import org.apache.spark.sql.catalyst.plans.logical.LogicalPlan
import org.apache.spark.sql.execution.{SparkPlan, SparkStrategy}



/**
 * Steamdrill:
 *
 * Extract equiJoinKeys from a next Join
 *
 * Null-safe equality will be transformed into equality as joining key (replace null with default
 * value).
 */
object ExtractEquiOrderedJoinKeys extends PredicateHelper with Logging {
  /** (joinType, leftKeys, rightKeys, condition, leftChild, rightChild) */
  type ReturnType =
    (Seq[Expression], Seq[Expression], Seq[Expression],
      Seq[Expression], LogicalPlan, LogicalPlan, OrderedJoinType)

  def unapply(plan: LogicalPlan): Option[ReturnType] = plan match {

    case _ @ OrderedJoin(left, right, lOexpr, rOexpr, condition, t) =>
      logDebug(s"Considering join on: $condition")
      // Find equi-join predicates in the condition

      val predicates = condition.map(splitConjunctivePredicates).getOrElse(Nil)
      logDebug(s"predicates ${predicates.mkString(",")}")

      val joinKeys = predicates.flatMap {
        case EqualTo(l, r) if l.references.isEmpty || r.references.isEmpty => None
        case EqualTo(l, r) if canEvaluate(l, left) && canEvaluate(r, right) => Some((l, r))
        case EqualTo(l, r) if canEvaluate(l, right) && canEvaluate(r, left) => Some((r, l))
        // Replace null with default value for joining key, then those rows with null in it could
        // be joined together
        case EqualNullSafe(l, r) if canEvaluate(l, left) && canEvaluate(r, right) =>
          Some((Coalesce(Seq(l, Literal.default(l.dataType))),
            Coalesce(Seq(r, Literal.default(r.dataType)))))
        case EqualNullSafe(l, r) if canEvaluate(l, right) && canEvaluate(r, left) =>
          Some((Coalesce(Seq(r, Literal.default(r.dataType))),
            Coalesce(Seq(l, Literal.default(l.dataType)))))
        case _ => None
      }

      if (joinKeys.nonEmpty) {
        val (lKeys, rKeys) = joinKeys.unzip
        logDebug(s"leftKeys:$lKeys | rightKeys:$rKeys")
        Some((lKeys, rKeys, lOexpr, rOexpr, left, right, t))
      } else {
        logDebug(s"no JoinKeys!?")
        None
      }

    case _ => None
  }
}

object OrderedJoinStrategy extends SparkStrategy {

  override def apply(plan: LogicalPlan): Seq[SparkPlan] = plan match {
    // --- for the NextJoin Operator ------------------------------------------------------
    case ExtractEquiOrderedJoinKeys(lKeys, rKeys, lOkey, rOkey, left, right, jType)
      if RowOrdering.isOrderable(lKeys) =>
      OrderedJoinExec(lKeys, rKeys, lOkey, rOkey, jType,
        planLater(left), planLater(right)) :: Nil

    case _ => Nil
  }
}




