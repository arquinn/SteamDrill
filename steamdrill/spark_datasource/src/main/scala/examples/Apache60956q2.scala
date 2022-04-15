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

import org.apache.spark.sql.expressions.Window
import org.apache.spark.sql.functions._
import org.apache.spark.sql.types.DataTypes

object Apache60956q2 {
  def main(args: Array[String]) {

    val replay = "/replay_logdb/rec_290819"
    System.out.print("*** apache httpd 60956 bug")

    val ot = new OmniTable(replay)
    val f = Functions(ot)
    //val g = new GlobalVariables(ot)

    import ot.sparkSession.implicits._

    val region = f.select (row_number.over(Window.partitionBy($"Name").orderBy($"order")) as "index", $"order")
      .where("Executable LIKE '%mpm_event%' AND Name = 'process_socket' AND index >= 1822 AND index <= 1828")
      .agg(max("order") as "max", min("order") as "min")

    //region.show(1000, false)

    // finds all 'blocking' calls from listener within the above region:
    /*
    f.entries
      .where($"Executable".contains("libc") && $"Name" === "__poll")
      .select("Executable", "METADATA_TID", "METADATA_PID", "Args", "Order")
      // .where($"Order".getField("METADATA_REPLAY_CLOCK") >= 9432345 &&
      //      $"Order".getField("METADATA_REPLAY_CLOCK") <= 9587289)
      .where($"METADATA_Tid" === 12250 )
      .show(Int.MaxValue, false)
*/


    val sockNB = "fcntl(#int,args.sockfd#,F_GETFL,0) & O_NONBLOCK"
    val largeTO = "#int,args.timeout# > 1000"
    f.where($"Executable".contains("libc.so"))
      .where((code(largeTO, "bool") && $"name" isin ("epoll_wait", "__poll")) ||
         (code(sockNB, "bool") && $"name" isin ("__send", "recv", "accept4")))
      .join(f.where("Executable Like '%mod_mpm_event%' AND Name = listener_thread"))
      .select("Executable", "Name", "Args", "Order")
      .where($"METADATA_Tid" === 12250 )
      .show(Int.MaxValue, false)

    ot.stop
  }
}
