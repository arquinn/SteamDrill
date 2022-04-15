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

import org.apache.spark.sql.{DataFrame, Dataset}
import org.apache.spark.sql.functions._

object Functions {
  implicit def DataSetToExended[T](df: Dataset[T]): ExtendedDataFrame[T] =
    new ExtendedDataFrame[T](df)

  def defs(ot: OmniTable): DataFrame = {
    import ot.sparkSession.implicits._
    val m = MemoryRegions(ot)

    Dataset.ofRows(ot.sparkSession,
      NestedGenerator.createB(Config.staticFile("staticFunctions"), "staticFunctions.schema", m("CacheFile") :: Nil, m))
    .select(m("Executable"), $"Name",
      MemoryRegions.condIncr($"LowPC", $"SharedLib", $"Location").as("LowPC"),
      MemoryRegions.condIncr($"HighPC", $"SharedLib", $"Location").as("HighPC"),
      MemoryRegions.condArrayIncr($"EntryPCs", $"SharedLib", $"Location").as("entries"),
      MemoryRegions.condArrayIncr($"ReturnPCs", $"SharedLib", $"Location").as("returns"))

//    val g = ot.gen("staticFunctions", "", s"CacheFile")

//    funcs.select(m("Executable"), $"Name",
//      MemoryRegions.condIncr($"LowPC", $"SharedLib", $"Location").as("LowPC"),
//      MemoryRegions.condIncr($"HighPC", $"SharedLib", $"Location").as("HighPC"),
//      MemoryRegions.condArrayIncr($"EntryPCs", $"SharedLib", $"Location").as("entries"),
//      MemoryRegions.condArrayIncr($"ReturnPCs", $"SharedLib", $"Location").as("returns"))



/*    m.join(g, g("CacheFile") === m("CacheFile"))
      .select(m("Executable"), $"Name",
        MemoryRegions.condIncr($"LowPC", $"SharedLib", $"Location").as("LowPC"),
        MemoryRegions.condIncr($"HighPC", $"SharedLib", $"Location").as("HighPC"),
        MemoryRegions.condArrayIncr($"EntryPCs", $"SharedLib", $"Location").as("entries"),
        MemoryRegions.condArrayIncr($"ReturnPCs", $"SharedLib", $"Location").as("returns"))
 */
  }

  def entries(ot: OmniTable): DataFrame = {
    import ot.sparkSession.implicits._
    ot().join(defs(ot), array_contains($"entries", $"EIP"))
  }

  def returns(ot: OmniTable): DataFrame = {
    import ot.sparkSession.implicits._
    ot().join(defs(ot), array_contains($"returns", $"EIP"))
  }

  def apply(ot: OmniTable): DataFrame = {
    import ot.sparkSession.implicits._
    entries(ot).as("e").orderedJoin(returns(ot).as("r"),
      $"e.name" === $"r.name" &&
        $"e.Executable" === $"r.Executable" &&
        $"e.Thread" === $"r.Thread",
        $"e.order" :: Nil,
        $"r.order" ::Nil,
        "left_stack")
        .select($"e.Executable" as "Executable",
          $"e.name" as "Name",
          $"e.Thread" as "Thread",
          $"e.order" as "EntryOrder",
          $"r.order" as "ReturnOrder",
        $"e.eip" as "EntryEip",
        $"r.eip" as "ReturnEip")
  }
}
