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

/*
 * Stripped version of the ArrayBasedMapBuilder from the new Spark 3.0 code in
 * github since we don't need that fancy, detect if duplicates stuff.
 */
package datasources

import scala.collection.mutable

import org.apache.spark.sql.catalyst.util.ArrayBasedMapData
import org.apache.spark.sql.catalyst.util.GenericArrayData
import org.apache.spark.sql.types._

/**
 * A builder of [[ArrayBasedMapData]], which fails if a null map key is detected
 */
class ArrayBasedMapBuilder(keyType: DataType, valueType: DataType) extends Serializable {

  private lazy val keys = mutable.ArrayBuffer.empty[Any]
  private lazy val values = mutable.ArrayBuffer.empty[Any]

  def put(key: Any, value: Any): Unit = {
    if (key == null) {
      throw new RuntimeException("Cannot use null as map key.")
    }

    keys.append(key)
    values.append(value)
  }

  private def reset(): Unit = {
    keys.clear()
    values.clear()
  }

  /**
    * Builds the result [[ArrayBasedMapData]] and reset this builder to free up the resources. The
    * builder becomes fresh afterward and is ready to take input and build another map.
    */
  def build(): ArrayBasedMapData = {
    val map = new ArrayBasedMapData(
      new GenericArrayData(keys.toArray), new GenericArrayData(values.toArray))
    reset()
    map
  }


  /**
    * Returns the current size of the map which is going to be produced by the current builder.
    */
  def size: Int = keys.size
}