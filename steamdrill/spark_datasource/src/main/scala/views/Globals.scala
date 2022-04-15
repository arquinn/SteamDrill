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

package Views

import helpers._
import logical._

import org.apache.spark.sql._
import org.apache.spark.sql.types.DataTypes
import org.apache.spark.sql.functions._
import org.apache.spark.sql.Column


object Globals {
  def defs(ot: OmniTable): DataFrame = {
    import ot.sparkSession.implicits._
    val m = MemoryRegions(ot)

    Dataset.ofRows(ot.sparkSession,
      NestedGenerator.createB(Config.staticFile("globals"), "globals.schema", m("CacheFile") :: Nil, m))
      .select($"File", $"Name", $"ReturnType", MemoryRegions.condIncr($"Ptr", $"SharedLib", $"Location").as("Ptr"))
  }

  def apply(ot: OmniTable): DataFrame = {
    import ot.sparkSession.implicits._
    ot().crossJoin(defs(ot))
      .withColumn("Value", $"Mem"(new Column(new TracingCast($"Ptr", $"ReturnType")).cast(DataTypes.IntegerType)))
      .select("Name", "File" :: "Value" :: Nil ++ ot().columns : _* )

    // not sure what to do about that cast there!
  }
}
