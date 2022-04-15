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

import executors.OmniTableScanBuilder
import logical.OmniTableLogicalPlan

import scala.collection.mutable
import org.apache.spark.internal.Logging
import org.apache.spark.sql.Strategy
import org.apache.spark.sql.catalyst.expressions.{Alias, AttributeReference, Expression, NamedExpression, PredicateHelper}
import org.apache.spark.sql.catalyst.planning.PhysicalOperation
import org.apache.spark.sql.catalyst.plans.InnerLike
import org.apache.spark.sql.catalyst.plans.logical.{GlobalLimit, Join, LogicalPlan, Project, ResolvedHint, ReturnAnswer}
import org.apache.spark.sql.execution.SparkPlan


object RoundifyStrategy extends Strategy with PredicateHelper with Logging  {

  val rounds : mutable.Buffer[OmniTableScanBuilder] = mutable.Buffer.empty
  private def getOrSetBuilder(round: Int, ot: OmniTableLogicalPlan): OmniTableScanBuilder = {
    assert (rounds.size + 1 >= round)
    if (rounds.size == round) {
      rounds += OmniTableScanBuilder(ot.replay, ot.exitClock, round) // not sure how/if this works?
    }

    rounds(round)
  }

  val indexes : mutable.HashMap[LogicalPlan, Int] = mutable.HashMap.empty
  private def setIndex(plan: LogicalPlan, index : Int): Unit = {
    indexes.update(plan, index)
  }
  def getIndex(plan: LogicalPlan) : Option[Int] = {
    indexes.get(plan)
  }

  val scans : mutable.HashMap[LogicalPlan, OmniTableScanBuilder] = mutable.HashMap.empty
  private def setBuilder(plan : LogicalPlan, builder : OmniTableScanBuilder) : Unit = {
    scans.update(plan, builder)
  }
  def getBuilder(plan : LogicalPlan): Option[OmniTableScanBuilder] = {
    scans.get(plan)
  }

  private def subAttrRefs(e: Expression, subs : Seq[NamedExpression]) : Expression =
    e match {
      case ar : AttributeReference =>
        subs.find(_.toAttribute.semanticEquals(ar)).getOrElse(ar)
      case e =>
        e.withNewChildren(e.children.map(subAttrRefs(_, subs)))
    }

  private def addToBuilder(ot: OmniTableLogicalPlan,
                           round: Int,
                           output : Seq[NamedExpression],
                           filters : Seq[Expression],
                           joinedRel : LogicalPlan,
                           joinConds : Seq[Expression]) : (OmniTableScanBuilder, Int) = {
    val builder = getOrSetBuilder(round, ot)
    val index = builder.addElem() // always tack on another element!

    val unhandledJoinConds: Seq[Expression] = builder.joinPushdown(joinConds, joinedRel.output)
    val unhandledFilters = builder.pushFilterExpressions(filters)
    builder.pushExpressions(output ++ unhandledFilters ++ unhandledJoinConds)
    // I think the problem is *really* in here, no?


    builder.addChild(joinedRel)

    // val adjustedStreamProject = output.map(substitute(_, output).asInstanceOf[NamedExpression])
    // val adjustedPostFilters = postFilters.map(substitute(_, output))
    // val adjustedPostConds = postConds.map(substitute(_, output))


    // setup our scans, joins and filters:
    // val filterCondition = (adjustedPostFilters ++ adjustedPostConds).reduceLeftOption(And)
    // val withFilter = filterCondition.map(FilterExec(_, scan)).getOrElse(scan)
    // val finalizd = ProjectExec(adjustedStreamProject, withFilter)

    // logDebug(s"nlje: ${finalizd.treeString}")
    (builder, index)
  }

  def iterate(plan: LogicalPlan, round: Int) : (Int, Boolean) = {
    // not sure that I need to return all of that??
    logDebug(s"iterate on $plan with $round")
    plan match {
      // Projection over a Join.
      case p@Project(projectList,
                     Join(ResolvedHint(left, hintInfo),
                     PhysicalOperation(proj, fils, omnitable : OmniTableLogicalPlan),
                     _ : InnerLike,
                     cond))
        if hintInfo.broadcast =>
        val (broadcastRound, usedOT) = iterate(left, round)
        val newRound = if (usedOT) broadcastRound + 1 else broadcastRound
        val unrolledProj = projectList.map(subAttrRefs(_, proj).asInstanceOf[NamedExpression])

        val unrolledJoin = splitConjunctivePredicates(cond.orNull)
          .filter(_ != null)
          .map(subAttrRefs(_, proj))


        val (builder, index) = addToBuilder(omnitable, newRound, unrolledProj, fils, left, unrolledJoin)
                       //         splitConjunctivePredicates(cond.orNull).filter(_ != null))
        setBuilder(p, builder)
        setIndex(p, index)

        (newRound, true)
      case j@Join(ResolvedHint(left, hintInfo),
                PhysicalOperation(proj, fils, omnitable : OmniTableLogicalPlan),
                _ : InnerLike,
                cond)
        if hintInfo.broadcast =>
        val (broadcastRound, usedOT) = iterate(left, round)
        val newRound = if (usedOT) broadcastRound + 1 else broadcastRound

        // we're consuming the join, so our output is the left followed by right
        val (builder, index) = addToBuilder(omnitable, newRound, left.output ++  proj, fils, left,
          splitConjunctivePredicates(cond.orNull).filter(_ != null))
        setBuilder(j, builder)
        setIndex(j, index)

        (newRound, true)

      case lp if lp.children.nonEmpty =>
        val newKids = lp.children.map(iterate(_, round))
        (newKids.map(_._1).max, newKids.map(_._2).foldLeft(false)(_ || _))
      case _ => (round, false)
      }
    }

  override def apply(plan: LogicalPlan): Seq[SparkPlan] = {
    plan match {
      // we're looking for the top-level logical node and trying to ONLY match that. We shall see?
      case ra: ReturnAnswer =>

        // clear any old data for the last query:
        indexes.clear()
        rounds.clear()
        scans.clear()
        iterate(ra, 0)
      case _ =>
    }
    Nil
  }
}
