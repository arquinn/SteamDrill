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

import executors.OmniTableScanExec
import org.apache.spark.sql.catalyst.rules.Rule
import org.apache.spark.sql.execution.SparkPlan
import org.apache.spark.sql.types.StructType

import scala.collection.mutable
import scala.collection.mutable.ArrayBuffer


case class OmniTableScanMerger() extends Rule[SparkPlan] {
  def apply(plan: SparkPlan): SparkPlan = {
    val scans = mutable.HashMap[StructType, ArrayBuffer[OmniTableScanExec]]()

    plan.transformUp {

      case otscan : OmniTableScanExec =>
        val sameSchema = scans.getOrElseUpdate(otscan.schema, ArrayBuffer[OmniTableScanExec]())
        val samePlan = sameSchema.find {
          e => otscan.sameResult(e)
        }
        if (samePlan.isDefined ) {
          logInfo("using the same plan!")
          samePlan.get
        }
        else {
          sameSchema += otscan
          otscan
        }
    }
  }
}
