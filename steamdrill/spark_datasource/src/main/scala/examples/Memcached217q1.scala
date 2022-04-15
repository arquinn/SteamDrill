
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

import Views.{AssemOps, Functions}
import helpers.{Config, ExtendedDataFrame, OmniTable}
import org.apache.spark.sql.Dataset
import org.apache.spark.sql.functions._

object Memcached217q1 {

  implicit def DataSetToExended[T](df: Dataset[T]): ExtendedDataFrame[T] =
    new ExtendedDataFrame[T](df)

  def main(args: Array[String]) {

    val replay = "/replay_logdb/rec_376835"
    System.out.print("*** Memcached 217 Q1\n")

    val ot = new OmniTable(replay, 0)
    val assem = AssemOps(ot)
    val funcs = Functions(ot)

    import ot.sparkSession.implicits._

    val query =  if (Config.getAssemblyReorder) {
      broadcast(broadcast(funcs.as("f")).join(AssemOps.defs(ot).as("d"), $"d.FuncName" === $"f.name"))
        .join(ot().as("ot"), $"ot.EIP" === $"d.Address" &&  $"ot.order" >= $"f.EntryOrder")
        .where($"d.Executable".contains("memcached"))
        .where($"f.Executable".contains("memcached"))
        .where($"f.ReturnOrder".isNull)
        .where($"f.Thread" === 22300)
        .where($"ot.Thread" === 22300)
        .select($"f.EntryOrder", $"f.name")
        .groupBy($"f.EntryOrder", $"f.name").count()

/*
      broadcast(funcs.as("f")).join(AssemOps.defs(ot).as("d"), $"d.FuncName" === $"f.name")
      //  .join(ot().as("ot"), $"ot.EIP" === $"d.Address" &&  $"ot.order" >= $"f.EntryOrder")
        .where($"d.Executable".contains("memcached"))
        .where($"f.Executable".contains("memcached"))
        .where($"f.ReturnOrder".isNull)
        .where($"f.Thread" === 22300)
        //.where($"ot.Thread" === 22300)
        .select($"f.EntryOrder", $"f.name", $"d.Address")
        //.groupBy($"f.EntryOrder", $"f.name").count()
 */
    }
    else {
      assem.as("o").join(funcs.as("f"),
       $"o.FuncName" === $"f.name" &&
         $"o.Executable" === $"f.Executable" &&
         $"o.order" >= $"f.EntryOrder" )
       .where($"o.Executable".contains("memcached"))
       .where($"f.Executable".contains("memcached"))
       .where($"f.ReturnOrder".isNull)
       .where($"f.Thread" === 22300)
       .where($"o.Thread" === 22300)
       .select($"f.EntryOrder", $"f.name")
       .groupBy($"f.EntryOrder", $"f.name").count()
    }

/*.where($"FuncName" === "do_item_get"||
        $"FuncName" === "try_read_command" ||
        $"FuncName" === "process_command" ||
        $"FuncName" === "process_get_command" ||
        $"FuncName" === "assoc_find" ||
        $"FuncName" === "item_get" ||
        $"FuncName" === "drive_machine" ||
        $"FuncName" === "worker_libevent")*/


    // this code should be a prevJoin b/c recursion. But I know that isn't a concern for us,
    // so a regular join makes this all easier for us to implement

		//query.explain(true)
    query.show(1000, false)

//    AssemOps.defs(ot).where($"Executable".contains("memcached"))
//    .show(1000, false)




//    matches.show(1000, true)
    // in a minute we'll do that vvv
//    matches.foreach {row =>
//      println(s"matches: ${row.toSeq.mkString(", ")}")}

//    output.foreach {row =>
//      println(s"cmp: ${row.toSeq.mkString(", ")}")}

//    end.foreach {row =>
//      println(s"exits ${row.toSeq.mkString(", ")}")}



//  matches.show(1000, false)

    ot.stop
  }
}
