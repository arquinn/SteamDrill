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

package Views

import helpers.{OmniTable}

import org.apache.spark.sql.{Column, DataFrame}
import org.apache.spark.sql.functions.{udf, when}

object MemoryRegions {

  val condArrayIncr = udf { (array: Seq[Long], cond: Boolean, incr: Long) =>
    if (cond) array.map(_ + incr) else array
  }

  def condIncr(value: Column, cond: Column, incr: Column): Column =
    when(cond, value + incr).otherwise(value)

  def apply(ot: OmniTable) : DataFrame =
    ot.gen("mem_maps", s"${ot.replay}")
  // .distinct()
}
