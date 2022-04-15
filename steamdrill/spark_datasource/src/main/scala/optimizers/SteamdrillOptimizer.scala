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

import datasources.{ProgramDataSource}
import logical.{LeftStackOJT, OmniTableLogicalPlan, OrderedJoin, NestedGenerator}
import org.apache.spark.internal.Logging
import org.apache.spark.sql.catalyst.expressions.{Alias, And, AttributeReference, Cast, Contains, EqualTo, Expression, NamedExpression, PredicateHelper}
import org.apache.spark.sql.catalyst.expressions.aggregate.AggregateFunction
import org.apache.spark.sql.catalyst.planning.PhysicalOperation
import org.apache.spark.sql.catalyst.plans.logical.{Aggregate, Deduplicate, Filter, HintInfo, Join, LogicalPlan, Project, ResolvedHint, SubqueryAlias}
import org.apache.spark.sql.catalyst.rules.Rule
import org.apache.spark.sql.catalyst.util.sideBySide
import org.apache.spark.sql.execution.datasources.v2.DataSourceV2Relation

object OrderGenerators extends Rule[LogicalPlan] with Logging {
  override def apply(plan: LogicalPlan): LogicalPlan = {
    val newPlan = plan.transformUp {
      case j@Join(PhysicalOperation(_, _, left : DataSourceV2Relation), _, _, _)
        if left.options.contains(ProgramDataSource.GENERATOR_KEY) &&
          !j.right.isInstanceOf[ResolvedHint] =>
        // we flip the right and left, because broadcasts must happen on left.
        Join(ResolvedHint(j.right, HintInfo(true)), j.left, j.joinType, j.condition)
      case j@Join(_, PhysicalOperation(_, _, right : DataSourceV2Relation), _, _)
        if right.options.contains(ProgramDataSource.GENERATOR_KEY) &&
           !j.left.isInstanceOf[ResolvedHint] =>
        Join(ResolvedHint(j.left, HintInfo(true)), j.right, j.joinType, j.condition)
    }
    logTrace(
      s"""Result of SimpBCastRule
          ${sideBySide(plan.treeString, newPlan.treeString).mkString("\n")}""".stripMargin)
    newPlan
  }
}
object SimpBCastRule extends Rule[LogicalPlan] with Logging {

  private def cost(plan: LogicalPlan) : Int = plan match {
    case d: OmniTableLogicalPlan =>
      1
    case p =>
      p.children.map(cost).sum
  }

  override def apply(plan: LogicalPlan): LogicalPlan = {
    val newPlan = plan.transformUp {
      case j@Join(PhysicalOperation(_, _, _ : OmniTableLogicalPlan), _, _, _)
        if cost(j.right) == 0 && !j.right.isInstanceOf[ResolvedHint] =>
        // we flip the right and left, because broadcasts must happen on left.
        Join(ResolvedHint(j.right, HintInfo(true)), j.left, j.joinType, j.condition)

      case j@Join(_, PhysicalOperation(_, _, _ : OmniTableLogicalPlan), _, _)
        if cost(j.left) == 0 && !j.left.isInstanceOf[ResolvedHint] =>
        // we flip the right and left, because broadcasts must happen on left.
        Join(ResolvedHint(j.left, HintInfo(true)), j.right, j.joinType, j.condition)


      case j@Join(_, PhysicalOperation(_, _, right : DataSourceV2Relation), _, _)
        if right.options.contains(ProgramDataSource.GENERATOR_KEY) &&
           cost(j.left) == 0 &&
           !j.left.isInstanceOf[ResolvedHint] =>
        Join(ResolvedHint(j.left, HintInfo(true)), j.right, j.joinType, j.condition)
    }
    logTrace( s"""Result of SimpBCastRule
          ${sideBySide(plan.treeString, newPlan.treeString).mkString("\n")}""".stripMargin)

    newPlan
  }
}
object ProjDown extends Rule[LogicalPlan] with Logging {

  private def split(condition: Seq[NamedExpression], left: LogicalPlan, right: LogicalPlan) = {
    val pushers = condition.filter(_.deterministic)
    val leftPushers = pushers.filter(_.references.subsetOf(left.outputSet))
    val rightPushers = pushers.filter(_.references.subsetOf(right.outputSet))

    (leftPushers, rightPushers)
  }

  private def mergeProjection(toPush: Seq[NamedExpression],
                              needed: Seq[Expression],
                              proj: LogicalPlan): LogicalPlan = {

    // we need to produce needed from toPush and proj:
    if (toPush.nonEmpty) {
      // this needs to fold an expression if necessary!

      proj match {
        case project: Project =>

          // get all child outputs that (1) will remove (2) are complex (3) needed
          val replacements = project.projectList.filterNot(_.isInstanceOf[AttributeReference])
            .filterNot(o => needed.exists(_.references.contains(o)))
            .filter(o => toPush.exists(_.references.contains(o)))

          logDebug(
            s"""
               |replacements ${replacements.mkString(", ")}
               |pushing ${toPush.mkString(", ")}
               |needed ${needed.mkString(", ")}
							 |projection ${project.projectList.mkString(", ")}
               |""".stripMargin)

          val pushing = toPush.map(_.transformUp {
            case ar: AttributeReference =>
              logTrace(
                s"""AR: $ar
                   |MATCH? ${replacements.find(_.toAttribute.semanticEquals(ar))}
                   |${replacements.map(_.exprId == ar.exprId).mkString(", ")}
                   |""".stripMargin)

              replacements.find(_.toAttribute.semanticEquals(ar)).getOrElse(ar)
            case a => a

          }.asInstanceOf[NamedExpression])

          logDebug(s"""pushing ${pushing.mkString(", ")}""")


          Project((pushing ++ project.projectList).filter(o => needed.exists(_.references.contains(o))), project.child)
        case _ =>
          Project((proj.output ++ toPush).filter(o => needed.exists(_.references.contains(o))), proj)
      }
    }
    else {
      proj
    }
  }

  override def apply(plan: LogicalPlan): LogicalPlan = {

    val newPlan = plan.transform {
      case p@Project(projectList, Join(left, right, t, cond)) =>
        val (leftP, rightP) = split(projectList, left, right)

        // figure out which elements in leftP and rightP are worth pushing:
				/// we need to be a bit 'leaner' about the way that we actually do this. 
				// (1) push only if there's actual computation?

        val leftToPush = leftP.filter(x => x.collect {
								case c : Cast => false
								case a : Alias => false
								case ar : AttributeReference => false
								case _ => true
								}.foldLeft(false)(_ || _))

        val rightToPush = rightP.filter(x => x.collect {
								case c : Cast => false
								case a : Alias => false
								case ar : AttributeReference => false
								case _ => true
								}.foldLeft(false)(_ || _))


        if (leftToPush.nonEmpty || rightToPush.nonEmpty) {
          logDebug(
            s"""
               | applying ProjDown to
               | $p
               | left ${leftToPush.mkString(", ")}
               | right ${rightToPush.mkString(", ")}
               |""")

          val pushers = leftToPush ++ rightToPush
          val newProjList = projectList.map(
            pj => pushers.find(_.semanticEquals(pj)).map(_.toAttribute).getOrElse(pj))

          val newLeft = mergeProjection(leftToPush, newProjList ++ cond, left)
          val newRight = mergeProjection(rightToPush, newProjList ++ cond, right)

          val newP = Project(newProjList, Join(newLeft, newRight, t, cond))

          logDebug(s"""ProjDown: $newP""")
          newP
        }
        else {
          p
        }

      case a@Aggregate(groupers, aggs, child) =>
        if (groupers.exists(!_.isInstanceOf[AttributeReference])) {

          // anything that doesn't have an AggregateFunction is pushable.
          val (aggPush, aggRemain) = aggs.partition(_.find(_.isInstanceOf[AggregateFunction]).isEmpty)

          // anything that isn't an AttrRef and isn't subsumed by the above should be pushed.
          // val (groupersRemain, temp) = groupers.partition(_.isInstanceOf[AttributeReference])

          val toPush = groupers.filterNot(a => aggPush.exists(_.asInstanceOf[Alias].child == a)) ++ aggPush

          // new convert toPush:
          val named: Seq[NamedExpression] = toPush.map {
            case expr: NamedExpression =>
              expr
            case expr =>
              Alias(expr, "testAlias")()
          }

          val newAggs = aggRemain ++ aggPush.map(_.toAttribute)

          val newGroupers = named.map(_.toAttribute)
          // we need to get references and use them in groupers
          val newChild = mergeProjection(named, newGroupers ++ newAggs, child)

          logTrace(
            s"""PD for $a
               |aggRemain ${aggRemain.mkString(", ")}
               |toPush ${toPush.mkString(", ")}
               |named ${named.mkString(", ")}
               |""".stripMargin)

          val newA = Aggregate(newGroupers, newAggs, newChild)
          logTrace(s"new Agg $newA")
          newA
        }
        else {
          a
        }
    }
    logTrace(
      s"""Result of ProjDown
          ${sideBySide(plan.treeString, newPlan.treeString).mkString("\n")}""".stripMargin)
    newPlan
  }
}

object CleanupProj extends Rule[LogicalPlan] with Logging {
    override def apply(plan: LogicalPlan): LogicalPlan = plan.transform {
      case p@Project(_, child) if child.output == p.output =>
        child
    }
}



object PushFilterThroughOrderedJoin extends Rule[LogicalPlan] with PredicateHelper with Logging {

  private def parseCondition(cond: Expression, joins: Seq[EqualTo]): Seq[Expression] = {
   val conditions = cond match {
      case EqualTo(left, right) =>
        joins.collect {
          case a if a.left.semanticEquals(left) =>
            EqualTo(a.right, right)
          case a if a.left.semanticEquals(right) =>
            EqualTo(a.right, left)
          case a if a.right.semanticEquals(left) =>
            EqualTo(a.left, right)
          case a if a.right.semanticEquals(right) =>
            EqualTo(a.left, left)
          }

      case Contains(left, right) =>
        joins.collect {
          case a if a.left.semanticEquals(left) =>
            Contains(a.right, right)
          case a if a.right.semanticEquals(left) =>
            Contains(a.left, right)
        }
      case _ =>
        Nil
    }
    if (conditions.nonEmpty) cond :: Nil ++ conditions else Nil

  }

  override def apply(plan: LogicalPlan) : LogicalPlan = {
    val newPlan = plan.transform {
      // push the where condition down into join filter
      case Filter(filter, OrderedJoin(left, right, leftExpr, rightExpr, jc, t))
        if t == LeftStackOJT =>
        val joinConditions =
          jc.map(splitConjunctivePredicates).getOrElse(Nil).asInstanceOf[Seq[EqualTo]]

        val filters = splitConjunctivePredicates(filter)
        val pushable = filters.flatMap(parseCondition(_, joinConditions)).distinct
        val unpushable = filters.filterNot(pushable.contains(_))

        logTrace(s"""$filter becomes ${pushable.mkString(", ")} and ${unpushable.mkString(", ")}""")

        val (leftFilters, temp) =
          pushable.partition(c => {c.deterministic && c.references.subsetOf(left.outputSet)})
        val (rightFilters, leftovers) =
          temp.partition(c => {c.deterministic && c.references.subsetOf(right.outputSet)})

        val newLeft = leftFilters.reduceLeftOption(And).map(Filter(_, left)).getOrElse(left)
        val newRight = rightFilters.reduceLeftOption(And).map(Filter(_, right)).getOrElse(right)

        val ordered = OrderedJoin(newLeft, newRight, leftExpr, rightExpr, jc, t)

        (leftovers ++ unpushable).reduceLeftOption(And).map(Filter(_, ordered)).getOrElse(ordered)

    }
    logTrace(
      s"""Result of PushFilterThroughOrderedJoin
        ${sideBySide(plan.treeString, newPlan.treeString).mkString("\n")}""".stripMargin)
    newPlan
  }
}



object NestedGenPushdown extends Rule[LogicalPlan] with Logging {
  override def apply(plan: LogicalPlan): LogicalPlan = plan.transform {
    case PhysicalOperation(cols, fils, NestedGenerator(exec, schema, inputs, plan))
        if fils.map(_.references.subsetOf(plan.outputSet)).foldLeft(false)(_ || _) =>

      // I don't think filtering on columns is useful, at least not yet:
      val (subfils, outerfils) = fils.partition(_.references.subsetOf(plan.outputSet))

      val newSubplan = subfils.reduceLeftOption(And).map(Filter(_, plan)).getOrElse(plan)
      val nestedGen = NestedGenerator(exec, schema, inputs, newSubplan)
      val withFilter =  outerfils.reduceLeftOption(And).map(Filter(_, nestedGen)).getOrElse(nestedGen)

      if (cols.size > 0) {
        Project(cols, withFilter)
      }
      else {
        withFilter
      }
  }
}


/*
object AssemblyReorder extends Rule[LogicalPlan] with PredicateHelper with Logging {

  def isStaticTable(key: String, plan : LogicalPlan) : Boolean =
  plan match {
    case Join(Aggregate(_, _, _), PhysicalOperation(_, _, dsv: DataSourceV2Relation), _, _)
        if dsv.options.contains(ProgramDataSource.EXECUTABLE_KEY)
        && dsv.options.getOrElse(ProgramDataSource.EXECUTABLE_KEY, "").contains(key) =>
      true
    case _ =>
      false
  }


  def isFunction(plan : LogicalPlan) : Boolean = plan match {

    case OrderedJoin(PhysicalOperation(_, _, Join(PhysicalOperation(_, _, _ : OmniTableLogicalPlan), Project(_, f1 : Join), _, _)),
      PhysicalOperation(_, _, Join(PhysicalOperation(_, _, _ : OmniTableLogicalPlan), Project(_, f2 : Join), _, _))
        , _, _, _, _) =>
      true

//    case o @OrderedJoin(_, _, _, _, _, _) =>
//      logError(s"partially caught! ${o.treeString}")
//        false
    case _ =>
      false
  }

  override def apply(plan: LogicalPlan) : LogicalPlan = {

    logError(s"called AssembReorder on ${plan.treeString}")
    val newPlan = plan.transform {

      case r@Join(PhysicalOperation(assembProj, assembFilter, Join(p@Project(_, _ : OmniTableLogicalPlan),
                                                a@PhysicalOperation(_, _, Join(PhysicalOperation(_, _, suba), _, _, _)), assembCond, assembType)),
                  funcs@PhysicalOperation(_, _, f), topLevelCond, topLevelType)
          if isFunction(f) && isStaticTable("assem", suba) =>


        // we just use funcs as is.
        // we just us a as is.

        // figure out how to join together a && f based upon assCond + topLevelCond.

        // 

        logError(s"found join!! ${r.treeString}")
        r


//      case r@Join(p@Project(_, _ : OmniTableLogicalPlan),
//        a@PhysicalOperation(_, _, Join(PhysicalOperation(_, _, suba), _, _, _)), _, _)
//          if isStaticTable("assem", suba) =>

//        logError(s"found assembly! ${r.treeString}")

//        r

//      case f if isFunction(f) =>
//        logError(s"found all of functions!! ${f.treeString}")
//        f

//      case r@Join(PhysicalOperation(_, _, _ : OmniTableLogicalPlan), Project(_, f1 : Join), _, _)
//        if isStaticTable("Func", f1) =>
//        logError(s"part of functions? ${r.treeString}")
//        r

    }
    newPlan
  }

}
 */

/*
object PushPredicateThroughOrderedJoin extends Rule[LogicalPlan] with PredicateHelper with Logging {
  /**
   * Splits join condition expressions or filter predicates (on a given join's output) into three
   * categories based on the attributes required to evaluate them. Note that we explicitly exclude
   * non-deterministic (i.e., stateful) condition expressions in canEvaluateInLeft or
   * canEvaluateInRight to prevent pushing these predicates on either side of the join.
   *
   * @return (canEvaluateInLeft, canEvaluateInRight, haveToEvaluateInBoth)
   */
  private def split(condition: Seq[Expression], left: LogicalPlan, right: LogicalPlan) = {
    val (candidates, nonDet) = condition.partition(c => {c.deterministic && c != Literal(true)})
    val (leftExprs, rest) = candidates.partition(_.references.subsetOf(left.outputSet))
    val (rightExprs, common) = rest.partition(_.references.subsetOf(right.outputSet))

    (leftExprs, rightExprs, common ++ nonDet)
  }

  private def parseCondition(cond: Expression, filters: Seq[Expression]): Seq[Expression] = {
    cond match {
      case _ @ Or(left, right) =>
        val lList = parseCondition(left, filters)
        val rList = parseCondition(right, filters)
        lList.flatMap(l => rList.map(Or(l, _)))

      case _ @ And(left, right) =>
        val lList = parseCondition(left, filters)
        val rList = parseCondition(right, filters)
        lList.flatMap(l => rList.map(And(l, _)))

      case _ @ EqualTo(left, right) =>
        // determine if the left or right can be substituted into any filters
        filters.flatMap( filter => {
          val childMap = filter.children.flatMap(child => {
            if (child == left) {
              Some((child, right))
            }
            else if (child == right) {
              Some((child, left))
            }
            else {
              None
            }
          })

          // now, we know if children map at all.
          childMap.map( tuple => {
            val n = filter.mapChildren(e => {
              if (e == tuple._1) {
                tuple._2
              }
              else {
                e
              }
            })
            n
          })
        })
      case _ =>
        Nil
    }
  }

  private def containsNullComp(expr: Expression): Boolean = {
    expr match {
      case IsNull(_) =>
        true
      case IsNotNull(_) =>
        true
      case _ =>
        if (expr.children.nonEmpty) {
          expr.children.map(containsNullComp(_)).exists(identity)
        } else {
          false
        }
    }
  }


  private def createExtraFilters(filters: Seq[Expression],
                                 conditions: Seq[Expression] ): Seq[Expression] = {

    val (candidates, _) = filters.partition(_.deterministic)
    conditions.flatMap( cond => parseCondition(cond, candidates))
  }

  def apply(plan: LogicalPlan): LogicalPlan = plan transform {

    // push the where condition down into join filter
    case f @ Filter(filterCondition, SDOrderedJoin(left, right, leftExpr, rightExpr, jc, t)) =>
      val extraFilters = createExtraFilters(splitConjunctivePredicates(filterCondition),
        jc.map(splitConjunctivePredicates).getOrElse(Nil))

      val (leftConds, rightConds, commonConds) =
        split(splitConjunctivePredicates(filterCondition), left, right)
      val (extraLeft, extraRight, extraCommon) = split(extraFilters, left, right)

      t match {

        case LeftStackOJT =>
          // push down the single side `where` condition into respective sides
          val newLeft =
            (leftConds ++ extraLeft).reduceLeftOption(And).map(Filter(_, left)).getOrElse(left)

          // figure out which (if any) of the right conditionals we can push:
          val rightPushable =
            (extraRight ++ rightConds).filterNot(containsNullComp(_))
          val rightUnpushable = (rightConds).filter(containsNullComp(_))

          logInfo(
            s"""pushable: ${rightPushable.mkString(", ")}
               |unpushable: ${rightUnpushable.mkString(", ")}
             """.stripMargin)

          val newRight =
            rightPushable.reduceLeftOption(And).map(Filter(_, right)).getOrElse(right)

          val rtn = SDOrderedJoin(newLeft, newRight, leftExpr, rightExpr, jc, t)
          rightUnpushable.reduceLeftOption(And).map(Filter(_, rtn)).getOrElse(rtn)

        case _ =>
          // push down the single side `where` condition into respective sides
          val newLeft =
            (leftConds ++ extraLeft).reduceLeftOption(And).map(Filter(_, left)).getOrElse(left)
          val newRight =
            (rightConds ++ extraRight).reduceLeftOption(And).map(Filter(_, right)).getOrElse(right)
          val (newJoinConds, others) = (commonConds ++ extraCommon).partition(canEvaluateWithinJoin)
          val newCond = (newJoinConds ++ jc).reduceLeftOption(And)

          var rtn: LogicalPlan = SDOrderedJoin(newLeft, newRight, leftExpr, rightExpr, newCond, t)
          if (others.nonEmpty) {
            rtn = Filter(others.reduceLeft(And), rtn)
          }

          logInfo(s"filter ${sideBySide(f.treeString, rtn.treeString).mkString("\n")}")
          rtn
      }

    // push down the join filter into sub query scanning if applicable
    case j @ SDOrderedJoin(left, right, leftExpr, rightExpr, joinCondition, t) =>
      val (leftJoinConditions, rightJoinConditions, commonJoinCondition) =
        split(joinCondition.map(splitConjunctivePredicates).getOrElse(Nil), left, right)


      // push down the single side only join filter for both sides sub queries
      val newLeft = leftJoinConditions.
        reduceLeftOption(And).map(Filter(_, left)).getOrElse(left)
      val newRight = rightJoinConditions.
        reduceLeftOption(And).map(Filter(_, right)).getOrElse(right)
      val newJoinCond = commonJoinCondition.reduceLeftOption(And)

      val newJoin = SDOrderedJoin(newLeft, newRight, leftExpr, rightExpr, newJoinCond, t)
      logInfo(s"nxtJoin\n ${sideBySide(j.treeString, newJoin.treeString).mkString("\n")}")
      newJoin
  }
}



// OmniTable NextJoin needs to be treated similarly to a Join!
// We use a blacklist to prevent us from creating a
// situation in which this happens; the rule will only remove an alias if its child
// attribute is not on the black list.

case SDOrderedJoin(left, right, leftExpr, rightExpr, condition, t) =>
val newLeft = removeRedundantAliases(left, blacklist ++ right.outputSet)
val newRight = removeRedundantAliases(right, blacklist ++ newLeft.outputSet)
val mapping = AttributeMap(
createAttributeMapping(left, newLeft) ++
createAttributeMapping(right, newRight))
val newCondition = condition.map(_.transform {
case a: Attribute => mapping.getOrElse(a, a)
})
val newLeftExpr = leftExpr.map(le => mapping.getOrElse(le.asInstanceOf[Attribute], le))
val newRightExpr = rightExpr.map(re => mapping.getOrElse(re.asInstanceOf[Attribute], re))
SDOrderedJoin(newLeft, newRight, newLeftExpr, newRightExpr, newCondition, t)
*/
