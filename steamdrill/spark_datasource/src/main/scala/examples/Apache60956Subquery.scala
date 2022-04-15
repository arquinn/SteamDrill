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

object Apache60956subquery {
  def main(args: Array[String]) {

    val replay = "/replay_logdb/rec_290819"
    System.out.print("*** apache httpd 60956 bug")

    val ot = new OmniTable(replay, 18608202)
    val f = Functions(ot)

    import ot.sparkSession.implicits._


    val region = f
      .select (row_number.over(Window.partitionBy($"Name").orderBy($"order")) as "index", $"order")
      .where("Executable LIKE '%mpm_event%' AND Name = 'process_socket' AND index >= 1822 AND index <= 1828")
      .agg(max("order") as "max", min("order") as "min")
        .write.save("Apache60956Subquery")

    ot.stop
  }
}
