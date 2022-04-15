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

package helpers

import java.io.{ByteArrayInputStream, ByteArrayOutputStream, DataInputStream, DataOutputStream}

import scala.collection.mutable

import org.apache.spark.sql.catalyst.InternalRow
import org.apache.spark.sql.catalyst.expressions.{Expression, UnsafeProjection, UnsafeRow}
import org.apache.spark.sql.catalyst.expressions.aggregate.{ImperativeAggregate, TypedImperativeAggregate}
import org.apache.spark.sql.catalyst.util.{ArrayBasedMapData, GenericArrayData}
import org.apache.spark.sql.types.{DataType, MapType}


case class CollectAsMap(key: Expression,
                        child: Expression,
                        mutableAggBufferOffset: Int = 0,
                        inputAggBufferOffset: Int = 0) extends TypedImperativeAggregate[mutable.HashMap[Any, Any]]{

  type CAMHashMap = mutable.HashMap[Any, Any]

  override def prettyName: String = "collect_as_map"

  override def createAggregationBuffer(): CAMHashMap = mutable.HashMap.empty

  override def update(buffer: CAMHashMap, input: InternalRow): CAMHashMap = {
    val k = key.eval(input)
    val v = child.eval(input)

    if (k != null && v != null) {
      buffer += (InternalRow.copyValue(k) -> InternalRow.copyValue(v))
    }
    buffer
  }

  override def merge(buffer: CAMHashMap, input: CAMHashMap): CAMHashMap = {
    buffer ++= input
  }

  override def eval(buffer: mutable.HashMap[Any, Any]): Any = {
    // not sure why, but buffer doesn't like me
    new ArrayBasedMapData(new GenericArrayData(buffer.keys.toArray),
                          new GenericArrayData(buffer.values.toArray))
  }

  lazy val projection = UnsafeProjection.create(Array[DataType](key.dataType, child.dataType))
  override def serialize(buffer: mutable.HashMap[Any, Any]): Array[Byte] = {
    // the UnsafeProjection code cannot handle map types. This code mostly based upon percentile
    val mem = new Array[Byte](4<<10)
    val bos = new ByteArrayOutputStream()
    val out = new DataOutputStream(bos)

    try {
      buffer.foreach { case (key, value) =>
          val row = projection.apply(InternalRow.apply(key, value))
          out.writeInt(row.getSizeInBytes)
          row.writeToStream(out, mem)
      }
      out.writeInt(-1)
      out.flush()

      bos.toByteArray
    } finally {
      out.close()
      bos.close()
    }

  }

  override def deserialize(bytes: Array[Byte]): mutable.HashMap[Any, Any] = {
    val bais = new ByteArrayInputStream(bytes)
    val in = new DataInputStream(bais)
    val buffer = new mutable.HashMap[Any, Any]()
    try {
      var rowSize = in.readInt()
      // we cleverly set to -1 at the end above
      while (rowSize > 0) {
        val bytez = new Array[Byte](rowSize)
        in.readFully(bytez)
        val row = new UnsafeRow(2)
        row.pointTo(bytez, rowSize)
        val k = row.get(0, key.dataType)
        val v = row.get(1, child.dataType)

        buffer.update(k, v)
        rowSize = in.readInt()
      }

      buffer
    } finally {
      in.close()
      bais.close()
    }
  }

  override def withNewMutableAggBufferOffset(newMutableAggBufferOffset: Int): ImperativeAggregate =
    copy( mutableAggBufferOffset = newMutableAggBufferOffset)

  override def withNewInputAggBufferOffset(newInputAggBufferOffset: Int): ImperativeAggregate =
    copy( inputAggBufferOffset = newInputAggBufferOffset)

  override def nullable: Boolean = true

  override def dataType: DataType = MapType.apply(key.dataType, child.dataType)

  override def children: Seq[Expression] = child :: key :: Nil

}



