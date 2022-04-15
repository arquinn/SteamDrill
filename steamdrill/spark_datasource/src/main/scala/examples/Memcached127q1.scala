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

import Views._
import helpers.Config
import helpers.OmniTable
import helpers._

import org.apache.spark.sql.functions._
import org.apache.spark.sql.types.DataTypes


object Memcached127q1 {
  def main(args: Array[String]) {
    val replay = "/replay_logdb/rec_376836"
//    val replay = "/replay_logdb/rec_372738"
    // Config.set_hosts(args(0))
    val ot = new OmniTable(replay)
    import ot.sparkSession.implicits._


    // Prototype of function:
    //
    // static void process_arithmetic_command(conn *c,
    //                                        token_t *tokens,
    //                                        const size_t ntokens,
    //                                        const bool incr);
    //


    val funcArgs = Arguments(ot)
      .where("Executable LIKE '%memcached%' And Name = 'process_arithmetic_command'")
    val k = funcArgs.where("PName = 'tokens'")
    val v = funcArgs.where("PName = 'tokens'")
    val i = funcArgs.where("PName = 'incr'")


    val out = k.as("k").join(v.as("v"), "order").join(i.as("i"), "order")
		      .groupBy($"k.Mem"($"k.Value".cast(DataTypes.IntegerType) + 1).cast(DataTypes.StringType) as "key",
									 $"v.Mem"($"v.Value".cast(DataTypes.IntegerType) + 2).cast(DataTypes.StringType) as "val",
                   $"i.Value" as "incr").count()


		//out.explain(true)
    // out.printSchema()
    out.show(1000000, false)

    ot.stop
  }
}
