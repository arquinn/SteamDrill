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

import Views.{Functions, Globals, Variables}
import helpers.{Config, OmniTable, TracingCode}
import helpers.TracingCode._
import org.apache.spark
import org.apache.spark.sql.expressions.Window
import org.apache.spark.sql.functions._
import org.apache.spark.sql.types.DataTypes

object Apache60956q1 {
  def main(args: Array[String]) {

    val replay = "/replay_logdb/rec_356353"
    System.out.print("*** apache httpd 60956 bug")

    val ot = new OmniTable(replay)
    val f = Functions(ot)
    val g = Globals(ot)

    import ot.sparkSession.implicits._


    val region = Functions.entries(ot)
      .select (row_number.over(Window.partitionBy($"Name").orderBy($"order")) as "index", $"order")
      .where("Executable LIKE '%mpm_event%' AND Name = 'process_socket' AND index >= 1760 AND index <= 1770")
        .select(max($"order"), min($"order"))
    // region.show(1000000, false)

    g.where("Name ='worker_queue_info' AND File LIKE '%mod_mpm_event.so'")
        .select($"Mem"($"Value".cast(DataTypes.IntegerType)).cast(DataTypes.StringType) as "queue_info", $"order")
      .where(get_json_object($"queue_info", ".idlers") > 0)
        .show(1000, false)

/*
    val globe = g.view.select(code("#fd_queue_info_t*,Value#->idlers - 2147483647", "int") as "idlers", $"order", $"Name")
      .where("idlers >= 2 AND File LIKE '%mpm_event%' AND Name = 'worker_queue_info'")
      .join(region, $"order" <= $"max" && $"order" >= $"min", "Inner")
*/


  ot.stop
  }
}
