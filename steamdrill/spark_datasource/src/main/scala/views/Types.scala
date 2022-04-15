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

import datasources.ProgramDataSource
import helpers.OmniTable

import org.apache.spark.sql.DataFrame
import org.apache.spark.sql.types.DataType


class Types(ot: OmniTable) {
  val staticSourceName: String = "datasources.ProgramDataSource"
  lazy val defined: DataFrame = {

    ot.sparkSession.read.format(staticSourceName)
      .option(ProgramDataSource.EXECUTABLE_KEY, "../static_tables/types")
      .option(ProgramDataSource.SCHEMA_KEY, "../static_tables/types.schema")
      .option(ProgramDataSource.INPUT_KEY, s"${ot.replay} .*")
      .load()
  }

  lazy val localMap: collection.Map[String, DataType] = {
    defined.rdd.map(r => {(r.getString(0), DataType.fromJson(r.getString(1))) }).collectAsMap()
  }
}