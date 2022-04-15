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

import Views.Variables
import helpers.{Config, OmniTable}
import helpers.TracingCode._
import org.apache.spark.sql.functions._
import org.apache.spark.sql.types.DataTypes


object Memcached127q2 {
  def main(args: Array[String]) {
    val replay = "/replay_logdb/rec_376836"
//    val replay = "/replay_logdb/rec_372738"
    val ot = new OmniTable(replay)
    import ot.sparkSession.implicits._

    val vars = Variables(ot)

    val itGroupBy = vars.where("Name = 'it' And scope = 'process_arithmetic_command'")
      .select(hex($"EIP") as "hex_EIP",
        get_json_object($"Mem"($"Value".cast(DataTypes.IntegerType)).cast(DataTypes.StringType), ".it_flags").cast(DataTypes.IntegerType) as "linked",
        $"Value"
        ).groupBy("hex_EIP")
				.pivot("linked", "0" :: "2" :: "3" :: "6" :: "65" :: Nil).count()
         .sort("hex_EIP")

				 //    itGroupBy.explain(true)
    itGroupBy.show(1000000, false) // cannot really show everything because the string rep is INSANITY

    // for some reason, parquet output isn't working for this thing!


    ot.stop
  }
}
