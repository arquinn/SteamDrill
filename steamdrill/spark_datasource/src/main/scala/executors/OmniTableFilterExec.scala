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


import java.io.{File, FileOutputStream, IOException}

import helpers.{Config, ScalaExpressionConverter}
import io.grpc.steamdrill.SDRelation
import io.grpc.steamdrill.SDQuery
import jni.ShmIter

import scala.collection.{Iterator, Seq, Map}
import scala.collection.mutable.HashMap
import scala.collection.JavaConverters._
import org.apache.spark.{InterruptibleIterator, Partition, SparkContext, SparkException, TaskContext, broadcast}
import org.apache.spark.internal.Logging
import org.apache.spark.rdd.RDD
import org.apache.spark.sql.catalyst.InternalRow
import org.apache.spark.sql.catalyst.errors.attachTree
import org.apache.spark.sql.catalyst.expressions.{Alias, Attribute, AttributeReference, Expression, GenericInternalRow, Literal, NamedExpression}
import org.apache.spark.sql.catalyst.plans.physical.{BroadcastDistribution, Distribution, IdentityBroadcastMode}
import org.apache.spark.sql.execution.{SQLExecution, SparkPlan, UnaryExecNode}
import org.apache.spark.sql.types.{DataType, DataTypes}
import org.apache.spark.unsafe.types.UTF8String

case class OmniTableFilterExec(
  scan : Int,
  child : OmniTableScanExec)
  extends  UnaryExecNode with Logging {


  override protected def doExecute(): RDD[InternalRow] = {
    child.execute().mapPartitions{ iter =>
      iter.asInstanceOf[OmniTableIterator].filter(scan)
      iter
    }
  }
  override def output: Seq[Attribute] = child.output
}


