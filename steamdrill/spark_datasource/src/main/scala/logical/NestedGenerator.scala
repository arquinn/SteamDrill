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
import datasources.Schema
import helpers.Config

import java.io.FileInputStream

import scala.collection.mutable

import org.apache.spark.internal.Logging
import org.apache.spark.sql.{Column, DataFrame}
import org.apache.spark.sql.catalyst.analysis.{PassThrough, UnresolvedAlias, UnresolvedAttribute}
import org.apache.spark.sql.catalyst.expressions.{Attribute, AttributeReference, AttributeSet, Expression, NamedExpression}
import org.apache.spark.sql.catalyst.plans.logical.{UnaryNode, LogicalPlan}



case class NestedGenerator(
  exec     : String,
  produced : Seq[AttributeReference],
  inputs   : Seq[NamedExpression], // Some expression over the child (?)
  child    : LogicalPlan) extends PassThrough with Logging {
  
  //logTRACe(s"Creating NestedGenerator with ${exec}, ${produced.mkString(", ")} st: ${new Exception().getStackTrace.mkString("\n")}")
  override def simpleString : String = s"NG_$exec(${inputs.mkString(", ")})[${output.mkString(", ")}]]"

  override def newInstance(): NestedGenerator = {
    copy(produced = produced.map(_.newInstance()))
  }

  def this(exec: String, execOut : String, inputs : Seq[String], child : DataFrame) {
    this(exec,
      Schema.fromStream(new FileInputStream(Config.staticFile(execOut))).getStructType.toAttributes,
      inputs.map(i => UnresolvedAlias(UnresolvedAttribute.quotedString(i))),
      child.logicalPlan)
  }
}

object NestedGenerator extends Logging {
  def create(exec: String, execOut : String, inputs : Seq[String], child : DataFrame): NestedGenerator = {
    NestedGenerator(exec,
      Schema.fromStream(new FileInputStream(Config.staticFile(execOut))).getStructType.toAttributes,
      inputs.map(i => UnresolvedAlias(UnresolvedAttribute.quotedString(i))),
      child.logicalPlan)
  }


  def createB(exec: String, execOut : String, inputs : Seq[Column], child : DataFrame): NestedGenerator = {
    NestedGenerator(exec,
      Schema.fromStream(new FileInputStream(Config.staticFile(execOut))).getStructType.toAttributes,
      inputs.map(_.named),
      child.logicalPlan)
  }
}

