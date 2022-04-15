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
package examples

import Views.Functions
import helpers.{Config, OmniTable}
import helpers.TracingCode._

import org.apache.spark.sql.functions._

object SQLite787fa71q1 {

  def main(args: Array[String]) {

    val replay = "/replay_logdb/rec_344068"
    System.out.print("*** SQLite 787fa71 Q1\n")

    val ot = new OmniTable(replay)
    val funcs = Functions(ot)
    import ot.sparkSession.implicits._

    // gets all operations executed:
    // --magic knowledge--- (This should just be over Variables)
    // sqlite3.c:71635  is at 0x807c509
    // &pOp is at $ebp + 0x508
    val opers = ot().where($"REGISTER_EIP" === 0x807a8d5)
      .select($"REGISTER_EBP" -0x14 as "Value", $"order", $"METADATA_TID" as "thread")
      .select(code("(*#VdbeOp**,Value#)->opcode", "int") as "oper",
              code("(*#VdbeOp**,Value#)->p1", "int") as "cursor",
              $"order", $"thread")
    // for debugging with the spark shell :)
    // opers.coalesce(1).write.parquet("opers.parquet")

    // gets invocations of sqlite3_exec
    val tab = funcs
      .where("Executable LIKE '%test_%' AND Name = 'sqlite3_exec'")
      .select($"EntryOrder", $"ReturnOrder", $"Tid")

    // for debugging with the spark shell :)
    // tab.coalesce(1).write.parquet("funcs.parquet")

    val joined = opers.join(tab, $"order" >= $"EntryOrder" &&
                                 ($"ReturnOrder".isNull ||  $"order" < $"ReturnOrder") &&
                                  hex($"thread") === $"Tid")
        .select($"EntryOrder", $"oper", $"cursor", $"ReturnOrder".isNull as "failed")

    val cases = joined
      .groupBy("EntryOrder", "cursor", "failed")
        .agg(collect_set("oper") as "opers")
        .where(array_contains($"opers", 108))  // I think this is preventing some bad opt?
        .groupBy("opers", "failed").count()

    cases.show(10000, false)

    ot.stop
  }
}
