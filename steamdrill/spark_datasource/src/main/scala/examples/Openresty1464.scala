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
import helpers.TracingCode.code
import helpers.{Config, OmniTable}

object Openresty1464 {
  def main(args: Array[String]) {

    val replay = "/replay_logdb/rec_622598"
    System.out.print("*** Nginx 1464 bug (opened through openresty):\n")

    val ot = new OmniTable(replay)
    import ot.sparkSession.implicits._

    // should wrap this up in an analysis pass
    // e.g., The magic hex number is the instruction of the segfault
    //
    // val memOps = new MemoryInstructions(ot)
    // memOps.where("eip" === 0x823751c).groupBy("Address").count()
    //
    // Result: shows a 2 addresses used by this instruction,
    // one is 0x0, the other is a reasonable pointer



/*    ot.table.where($"REGISTER_EIP" === 0x0823751c)
      .withColumn("BSHack", code("(u_long*)(#REGISTER_EAX# + 0x10)", "u_long*"))
      .select("REGISTER_EIP", "Order", "BSHack")
      .groupBy("BSHACK").count()
      .show(Int.MaxValue, false)
*/
    // *really* want is backwards slice logic on the value REGISTER_EAX == 0 at that instruction
    // (1) We'd have to know how to instrument the entire program to get an entry
    // (2) This isn't *really* expressible in SQL..

    val funcs = Functions(ot)
    // funcs.(-1).select("Executable", "Name").show(100, false)

    funcs.select("Executable", "Name", "ReturnEip", "EntryEip", "EntryOrder", "ReturnOrder")
     .show(1000000000, false)


    // note: How do I know to use watchpoints here..?
    //      (probably needs a custom optimizer for semantics of memOps)
    //
    // memOps.where("Address" == pointer)
    //       .where("Read/Write" == "Write")
    //
    // Result: shows a few writes for the correct pointer

    ot.stop
  }
}
