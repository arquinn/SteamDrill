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

import datasources.SupportsMultiroundScan
import logical.{SDHint, SHRoundHint}
import org.apache.spark.sql.catalyst.expressions.{And, AttributeReference, Expression, NamedExpression}
import org.apache.spark.sql.catalyst.planning.PhysicalOperation
import org.apache.spark.sql.catalyst.plans.{InnerLike, JoinType}
import org.apache.spark.sql.catalyst.plans.logical.{Join, LogicalPlan, Project, ResolvedHint}
import org.apache.spark.sql.execution.{FilterExec, ProjectExec, SparkPlan}
import org.apache.spark.sql.execution.datasources.v2.{DataSourceV2Relation, DataSourceV2ScanExec, DataSourceV2StrategyBase}
import org.apache.spark.sql.execution.joins.{BroadcastBlockNestedLoopJoinExec, BuildLeft}
import org.apache.spark.sql.functions._
import org.apache.spark.sql.sources.v2.reader._

import scala.collection.mutable.HashMap


object DataSourceV2StrategyExtended extends DataSourceV2StrategyBase  {

  override def apply(plan: LogicalPlan): Seq[SparkPlan] = {
    plan match {
      case j@Join(ResolvedHint(bcast, hintInfo),
      PhysicalOperation(streamProject, streamFilters, stream: DataSourceV2Relation),
      jtype: InnerLike, ogCond) if hintInfo.broadcast => {

        logDebug(
          s"""Found a d2 join: ${j.toString}
             |left rel: ${bcast.toString}
             |right rel: ${stream.toString()}
             |""".stripMargin)

        val reader = stream.newReader()
        reader match {
          case r: SupportsJoinPushdown =>
            // this is the basic optimization stuff that tells the scan about what we need

            val conds: Seq[Expression] = splitConjunctivePredicates(ogCond.orNull).filter(_ != null)
            val postConds: Seq[Expression] = r.joinPushdown(conds.toArray, bcast.output.toArray)
            val (pushFilters, postFilters) = pushFilterExpressions(reader, streamFilters)
            val output = pruneExpressions(reader, stream, streamProject ++ postFilters ++ postConds)

            val adjustedStreamProject =
              streamProject.map(substitute(_, output).asInstanceOf[NamedExpression])
            val adjustedPostFilters = postFilters.map(substitute(_, output))
            val adjustedPostConds = postConds.map(substitute(_, output))

            logDebug(
              s"""
                 |Join Pushdown.
                 |Pushed Filters: ${pushFilters.mkString(", ")}
                 |Post-Scan Filters: ${adjustedPostFilters.mkString(",")}
                 |Post-Scan Joins: ${adjustedPostConds.mkString(", ")}
                 |Output: ${output.mkString(", ")}
                 |Proj: ${adjustedStreamProject.mkString(", ")}
             """.stripMargin)


            // setup our scans, joins and filters:
            val postCond = adjustedPostConds.reduceLeftOption(And)
            val filterCondition = adjustedPostFilters.reduceLeftOption(And)

            val scan = DataSourceV2ScanExec(output.map(_.toAttribute),
              stream.source, stream.options, pushFilters, reader)

            val join = BroadcastBlockNestedLoopJoinExec(planLater(bcast),
              scan,
              BuildLeft,
              jtype,
              postCond)
            val withFilter = filterCondition.map(FilterExec(_, join)).getOrElse(join)
            val finalizd = ProjectExec(adjustedStreamProject ++ bcast.output, withFilter)

            logDebug(s"nlje: ${finalizd.treeString}")

            finalizd :: Nil
          case _ => Nil
        }
      }
      case _ => Nil
    }
  }
}
