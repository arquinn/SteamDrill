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


object AssemOps {

  def defs(ot: OmniTable) : DataFrame = {
    import ot.sparkSession.implicits._
    val m = MemoryRegions(ot)
    val fd = Functions.defs(ot)
    val subQ = m.join(fd, "Executable")

    Dataset.ofRows(ot.sparkSession,
      NestedGenerator.createB(
        Config.staticFile("assemOps"),
        "assemOps.schema",
        m("CacheFile") :: fd("LowPC") :: fd("HighPC") :: Nil,
        subQ))
      .select(
        MemoryRegions.condIncr($"Address", m("SharedLib"), m("Location")).as("Address"),
        $"Executable",
        $"Inst",
        $"Reads",
        $"Writes",
        $"Name".as("FuncName"))



    //val g = ot.gen("assemOps", "", s"CacheFile <Address >=Address").as("g")
//    m.join(g, g("CacheFile") === m("CacheFile"))
//      .withColumn("Address", MemoryRegions.condIncr($"Address", $"SharedLib", $"Location"))
//      .join(fd, fd("LowPC") <=  $"Address" && $"Address" <= fd("HighPC"))
//      .select(m("Executable"), g("Inst"), g("Reads"), g("Writes"), $"Address", $"Name".as("FuncName"))

  }


  // FuncName


  // todo: include the reads / writes as a column
  def apply(ot: OmniTable): DataFrame = {
    import ot.sparkSession.implicits._
    ot().join(defs(ot), $"EIP" === $"Address")
  }
}
