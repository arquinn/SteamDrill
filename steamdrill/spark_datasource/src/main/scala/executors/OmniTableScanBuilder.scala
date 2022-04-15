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

import datasources.ExpressionWrapper
import helpers.ScalaExpressionConverter
import org.apache.spark.internal.Logging

import scala.collection.{Seq, mutable}
import org.apache.spark.sql.catalyst.expressions.{Alias, Attribute, Expression, Literal, NamedExpression}
import org.apache.spark.sql.catalyst.plans.logical.LogicalPlan
import org.apache.spark.sql.execution.PlanLater

case class OmniTableScanBuilder (
  replay : String,
  exitClock : Long,
  round: Int) extends Logging {

  var filterExprs : Seq[Seq[Expression]] = Seq.empty
  var outputExprs : Seq[Seq[NamedExpression]] = Seq.empty
  var joinFilters : Seq[Seq[Expression]] = Seq.empty
  var joinSchema  : Seq[Seq[Attribute]] = Seq.empty
  var children : mutable.Seq[LogicalPlan] = mutable.Seq.empty

  // now this stuff, which is helpful later on when we build stuff:
  var unhandledOutput : Seq[Seq[NamedExpression]] = Seq.empty
  var unhandledFilters : Seq[Seq[Expression]] = Seq.empty
  var unhandledJoinFilters : Seq[Seq[Expression]] = Seq.empty

  def addElem(): Int = {
    filterExprs = filterExprs :+ Nil
    outputExprs = outputExprs :+ Nil
    joinFilters = joinFilters :+ Nil
    joinSchema = joinSchema :+  Nil

    unhandledOutput = unhandledOutput :+ Nil
    unhandledFilters = unhandledFilters :+ Nil
    unhandledJoinFilters = unhandledJoinFilters :+ Nil

    filterExprs.size - 1
  }

  def addChild(plan : LogicalPlan): Unit = children = children :+ plan

  private var scan : OmniTableScanExec = null

  def build: OmniTableScanExec = {
    if (scan == null) {
      logInfo("Creating new Scan")
      scan = OmniTableScanExec(round, replay, exitClock, children.map(PlanLater),
        filterExprs, outputExprs, joinFilters, joinSchema)
    }
    else {
      logInfo("Reusing scan")
    }
    scan
  }

  //
  // Code for creating everything.
  //
  def pushExpressions(exprs: Seq[Expression]): Seq[NamedExpression] = {
    var set = new mutable.HashSet[ExpressionWrapper]
    for (e <- exprs) {
      set ++= handleableSubexpressions(e)
    }
    // Convert into the actual output

    outputExprs = outputExprs.init :+ set.filterNot(_.expr.isInstanceOf[Literal]).map(
      _.expr match {
        case ne : NamedExpression =>
          ne
        case d : Expression =>
          Alias(d, "temp")()
      }).toSeq

    val unhandled = exprs.filterNot(e => set.contains(new ExpressionWrapper(e)))
      .map {
        case ne: NamedExpression =>
          ne
        case d: Expression =>
          Alias(d, "temp")()
      }

    logDebug(s"""wanted ${exprs.mkString(", ")}
                 pushed ${outputExprs.mkString(", ")}
                 unhand ${unhandled.mkString(", ")} """)

    unhandledOutput = unhandledOutput.init :+ unhandled

    outputExprs.last
  }

  private def handleableSubexpressions(e: Expression) : mutable.HashSet[ExpressionWrapper] = {
    val exprs = new mutable.HashSet[ExpressionWrapper]
    if (ScalaExpressionConverter.canHandle(e, joinSchema.last)) {
      exprs += new ExpressionWrapper(e)
    }
    else {
      for (child <- e.children) {
        exprs ++= handleableSubexpressions(child)
      }
    }
    exprs
  }

  def pushFilterExpressions(exprs: Seq[Expression]): Seq[Expression] = {
    logDebug(s"whoo hoo filters boi ${exprs.mkString(", ")}")

    val (fils : Seq[Expression], unhandled : Seq[Expression]) =
      exprs.partition(ScalaExpressionConverter.canHandle(_, joinSchema.last))

    logTrace(s"fils: ${fils.mkString(", ")} and unhandled: ${unhandled.mkString(", ")}")

    filterExprs = filterExprs.init  :+ fils
    unhandledFilters = unhandledFilters :+ unhandled
    unhandled
  }

  def joinPushdown(exprs: Seq[Expression], input: Seq[Attribute]): Seq[Expression] = {
    joinSchema = joinSchema.init :+ input
    val (fils, unhandled) = exprs.partition(ScalaExpressionConverter.canHandle(_, joinSchema.last))
    joinFilters = joinFilters.init  :+ fils

    unhandledJoinFilters = unhandledJoinFilters.init :+ unhandled
    unhandled
  }

  def getUnhandledOutput(elem: Int) : Seq[NamedExpression] = unhandledOutput(elem)
  def getUnhandledFilters(elem: Int) : Seq[Expression] = unhandledFilters(elem)
  def getUnhandledJoinFilters(elem : Int): Seq[Expression] = unhandledJoinFilters(elem)
}
