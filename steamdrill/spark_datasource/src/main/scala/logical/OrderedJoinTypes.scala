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

package logical

import java.util.Locale

object OrderedJoinType {
  def apply(typ: String): OrderedJoinType = typ.toLowerCase(Locale.ROOT).replace("_", "") match {
    case "next" => NextOJT
    case "leftnext" => LeftNextOJT

    case "prev" => PrevOJT
    case "stack" | "innerstack" => InnerStackOJT
    case "leftstack" => LeftStackOJT

    case _ =>
      val supported = Seq("next", "prev",
        "stack", "innner_stack",
        "left_stack")

      throw new IllegalArgumentException(s"Unsupported join type '$typ'. " +
        "Supported join types include: " + supported.mkString("'", "', '", "'") + ".")
  }
}

sealed abstract class OrderedJoinType {
  def sql: String
}


case object NextOJT extends OrderedJoinType {
  override def sql: String = "NEXT"
}
case object LeftNextOJT extends OrderedJoinType {
  override def sql: String = "LEFT NEXT"
}

case object PrevOJT extends OrderedJoinType {
  override def sql: String = "PREV"
}

sealed abstract class StackLike extends OrderedJoinType {
  override def sql: String = "STACK"
}

case object InnerStackOJT extends StackLike {
  override def sql: String = "INNER STACK"
}


case object LeftStackOJT extends StackLike {
  override def sql: String = "LEFT STACK"
}


