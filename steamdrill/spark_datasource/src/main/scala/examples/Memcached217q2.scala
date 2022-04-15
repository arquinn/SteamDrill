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

import Views.{Functions, MemoryRegions}
import helpers.{Config, OmniTable, TracingCode}
import helpers.TracingCode._

object Memcached217q2 {
  def main(args: Array[String]) {
    val replay = "/replay_logdb/rec_372738"
    System.out.print("*** Memcached 217 Q2\n")

    val ot = new OmniTable(replay)
    val funcs = Functions(ot)
    import ot.sparkSession.implicits._

    /*
    val dynFuncs = funcs.hackEntries.where("Executable LIKE '%memcached%'")
      .where($"Type" === "item*")
      .select($"Name", $"arg", $"PName", code("#item*,arg#->it_flags", "u_char") as "status")
      .groupBy($"Name", $"PName").pivot($"status", 2 :: 3 :: 4 :: 17 :: Nil).count()


    dynFuncs.show(1000, false)

     */
  }
}
