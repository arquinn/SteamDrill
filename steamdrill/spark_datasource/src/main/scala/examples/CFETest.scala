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

import org.apache.spark.sql.types.DataType

object CFETest {

  def dumpShit(dt : DataType): Unit =
  {
    System.out.print("datatype " + dt + " " + dt.defaultSize + "\n")
    System.out.print(dt.json + "\n")

    System.out.print("\n")
    System.out.print("\n")
  }
  def main(args: Array[String]) {

    System.out.print("*** Query w/out breakpoint:\n")

    val replay = "not sure..."

    val ot = new OmniTable(replay)
    val vars = Variables(ot)
    vars.where("Name = 'localVariable'")
             .where("REGISTER_EIP = 0x8048535")
             .show()


/*    // Query #2: Show me all the times when localVariable is 10
    funcVars.view.where("Name = 'main'" )
      .where("locals.localVariable ='10'")
      .select("locals.localVariable")
      .show()

    ot.stop
    */
  }
}
