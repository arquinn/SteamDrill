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

package optimizers

import helpers.CollectAsMap
import org.apache.spark.sql.catalyst.expressions.aggregate.{AggregateExpression, First}
import org.apache.spark.sql.catalyst.expressions.{Alias, CaseWhen, EqualNullSafe, Expression, GetMapValue, Literal, NamedExpression}
import org.apache.spark.sql.catalyst.plans.logical.{Aggregate, LogicalPlan, Project}
import org.apache.spark.sql.catalyst.rules.Rule

object AggregateMapOptimizer extends Rule[LogicalPlan]{

  private def getMaps(expr: Expression): Seq[GetMapValue] = expr match {
      case p@GetMapValue(child, _) if child.isInstanceOf[AggregateExpression] &&
        child.asInstanceOf[AggregateExpression].aggregateFunction.isInstanceOf[CollectAsMap] =>
        p :: Nil
      case e =>
        e.children.flatMap(getMaps(_))
    }

  private def updateElems(expr: Expression,
                          map : Map[GetMapValue, Expression]) : Expression = expr match {
    case p@GetMapValue(_, _) if map.get(p).isDefined =>
      map.apply(p)
    case e =>
      e.withNewChildren(e.children.map(updateElems(_, map)))
  }

  // This approach might not be optimal at all times, but probably OK for OT.
  def apply(plan : LogicalPlan) : LogicalPlan = plan.transformUp {

    // todo: validate something about determinism?
    case agg@Aggregate(groupings, aggregates, child) =>
      val gmvs = aggregates.flatMap(getMaps(_))

      if (gmvs.nonEmpty) {
        // create an alias for each of the gmvs:

        logInfo(s"AggregateMapOptimizer getmapValues is ${gmvs.mkString(",")}")

        val aliasMap = gmvs.map(gmv => {
          val cam = gmv.child.asInstanceOf[AggregateExpression].aggregateFunction
                            .asInstanceOf[CollectAsMap]
          gmv -> Alias(CaseWhen(Seq((EqualNullSafe(cam.key, gmv.key), cam.child)), None), "alias")()
        }).toMap

        val newChild = Project.apply(child.output ++ aliasMap.values, child)
        val aggMap = aliasMap.map(pair =>
          pair._1 -> First.apply(pair._2, Literal.apply(true)).toAggregateExpression(false))

        val newAggs = aggregates.map(updateElems(_, aggMap).asInstanceOf[NamedExpression])

        Aggregate(groupings, newAggs, newChild)
      }
      else {
        agg
      }
  }
}
