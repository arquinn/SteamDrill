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

package logical

import org.apache.spark.sql.catalyst.expressions.{Attribute, Expression, LessThan, PredicateHelper}
import org.apache.spark.sql.catalyst.plans.logical.{BinaryNode, LogicalPlan}
import org.apache.spark.sql.types.BooleanType


case class OrderedJoin(left: LogicalPlan,
                       right: LogicalPlan,
                       leftExpr: Seq[Expression],
                       rightExpr: Seq[Expression],
                       extraConds: Option[Expression],
                       joinType: OrderedJoinType) extends BinaryNode with PredicateHelper {

  override def output: Seq[Attribute] = {
    joinType match {
      case LeftStackOJT =>
        left.output ++ right.output.map(_.withNullability(true))
      case _ =>
        left.output ++ right.output
    }
  }

  lazy val orderCond: Seq[LessThan] = leftExpr.zip(rightExpr).map(p => LessThan(p._1, p._2))

  override protected def validConstraints: Set[Expression] = {
    val allConstraints = left.constraints.union(right.constraints) ++ orderCond
    if (extraConds.isDefined) {
      allConstraints.union(splitConjunctivePredicates(extraConds.get).toSet)
    }
    allConstraints
  }

  def duplicateResolved: Boolean = left.outputSet.intersect(right.outputSet).isEmpty

  // if not a natural join, use `resolvedExceptNatural`. if it is a natural join or
  // using join, we still need to eliminate natural or using before we mark it resolved.
  override lazy val resolved: Boolean =
  childrenResolved &&
    duplicateResolved &&
    orderCond.map(_.resolved).forall(identity) &&
    extraConds.forall(_.dataType == BooleanType)
}