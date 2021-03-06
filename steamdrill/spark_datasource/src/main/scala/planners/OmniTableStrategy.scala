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


import org.apache.spark.internal.Logging
import org.apache.spark.sql.Strategy
import org.apache.spark.sql.catalyst.expressions.{And, AttributeReference, EqualTo, Expression, Literal, NamedExpression, PredicateHelper}
import org.apache.spark.sql.catalyst.plans.logical.LogicalPlan
import org.apache.spark.sql.execution.{FilterExec, ProjectExec, SparkPlan}
import org.apache.spark.sql.types.DataTypes

import executors.OmniTableFilterExec

object OmniTableStrategy extends Strategy with PredicateHelper with Logging {

  private def subAttrRefs(e: Expression, subs : Seq[NamedExpression]) : Expression =
    e match {
      case ar : AttributeReference =>
        subs.find(_.toAttribute.semanticEquals(ar)).getOrElse(ar)
      case e =>
        e.withNewChildren(e.children.map(subAttrRefs(_, subs)))
    }

  override def apply(plan: LogicalPlan): Seq[SparkPlan] = {
    val otPass = RoundifyStrategy.getBuilder(plan)
    if (otPass.isDefined) {
      val indexNo = RoundifyStrategy.getIndex(plan)
      assert (indexNo.isDefined)

      val otBuilder = otPass.get
      val index = indexNo.get
      val otScan = otBuilder.build

      val unoutput = otBuilder.unhandledOutput(index)
      val unfiltered = otBuilder.getUnhandledFilters(index)
      val unconded = otBuilder.getUnhandledJoinFilters(index)
      // das wark?
      // we really just need to know how to make the unhandled projections, no?


      val subedOut = plan.output.map(subAttrRefs(_, unoutput).asInstanceOf[NamedExpression])

      logDebug(s"""unoutput ${unoutput.mkString(", ")}
                   unfilter ${unfiltered.mkString(", ")}
                   unconded ${unconded.mkString(", ")}""")

      logDebug(s"das wark??? ${subedOut.mkString(", ")}")


//      val eq = EqualTo(otScan.refNum, new Literal(index, DataTypes.IntegerType))
      val otfilter = OmniTableFilterExec(index , otScan)

      val newFilterCond = (unfiltered ++ unconded).reduceLeftOption(And)
      val withFilter = newFilterCond.map(FilterExec(_, otfilter)).getOrElse(otfilter)
      val withProj = ProjectExec(subedOut, withFilter)


      logDebug(s"scan output ${otScan.output.mkString(", ")}")
      logDebug(s"scan refNum ${otScan.refNum}")
      logDebug(s"afta fil ${withFilter.treeString}")

      withProj :: Nil
    }
    else {
      Nil
    }
  }
}
