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

import logical.NestedGenerator
import executors.NestedGeneratorExec

import org.apache.spark.internal.Logging
import org.apache.spark.sql.Strategy

import org.apache.spark.sql.catalyst.expressions.{ AttributeReference, Expression, NamedExpression}
import org.apache.spark.sql.catalyst.planning.PhysicalOperation
import org.apache.spark.sql.execution.{ ProjectExec, SparkPlan}
import org.apache.spark.sql.functions._
import org.apache.spark.sql.catalyst.plans.logical.LogicalPlan

import scala.collection.mutable.HashMap


object NestedGeneratorStrategy extends Strategy with Logging {

  override def apply(plan: LogicalPlan): Seq[SparkPlan] = {
    plan match {
      case ng @ NestedGenerator(exec, execOut, inExprs, child) => {

/*        for (ine <- inExprs) {
          val idx = child.output.indexWhere(_.exprId == ine.toAttribute.exprId)
          logError(s"in expr $ine at $idx")
        }
 */

        val inIndexes = inExprs.map(x => child.output.indexWhere(_.exprId == x.toAttribute.exprId))
        NestedGeneratorExec(exec, execOut, inIndexes, planLater(child)) :: Nil
      }
      case _ => Nil
    }
  }
}
