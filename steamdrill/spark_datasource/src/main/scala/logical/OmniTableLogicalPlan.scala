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

import scala.collection.mutable

import org.apache.spark.internal.Logging
import org.apache.spark.sql.catalyst.analysis.MultiInstanceRelation
import org.apache.spark.sql.catalyst.expressions.AttributeReference
import org.apache.spark.sql.catalyst.plans.logical.{LeafNode, LogicalPlan, Statistics}


object OmniTableLogicalPlan {
  var roundChildrenMap : mutable.Seq[Seq[LogicalPlan]] = mutable.Seq.empty

  def addChild(round: Int, plan: LogicalPlan = null) : Int = {
    assert (roundChildrenMap.size >= round)
    while (roundChildrenMap.size <= round) {
      roundChildrenMap = roundChildrenMap :+ Seq.empty
    }
    roundChildrenMap(round) = roundChildrenMap(round) :+ plan
    roundChildrenMap(round).size
  }

  def getChildren(round: Int): Seq[LogicalPlan] = {
    roundChildrenMap(round)
  }
}

case class OmniTableLogicalPlan(
  output : Seq[AttributeReference],
  replay : String,
  exitClock : Long,
  round : Int = -1,
  index : Int = 0) extends LeafNode with Logging with MultiInstanceRelation {

  override def simpleString : String = s"OmniTable($replay, $round, $index)"

  override def newInstance(): OmniTableLogicalPlan = {
    copy(output = output.map(_.newInstance()))
  }

  // there isn't really a good way to make this happen.
  // I guess it will depend a lot on the filter/joins around it?
  override def computeStats(): Statistics =
    Statistics(sizeInBytes = conf.defaultSizeInBytes)
}
