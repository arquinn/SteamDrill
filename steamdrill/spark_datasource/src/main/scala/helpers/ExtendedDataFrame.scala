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

import logical.{OrderedJoin, OrderedJoinType}

import org.apache.spark.sql.{Column, DataFrame, Dataset}

object ExtendedDataFrame {

}

class ExtendedDataFrame[T] (val df : Dataset[T]) {
  /**
   * :: Experimental ::
   *
   * New for the OmniTable: orderedJoin!!
   * This joins the two relations such that each row is joined with the next row
   * of other (when ordered based on order1 and order2)
   *
   * @since 2.0.0
   */
  def orderedJoin(other: Dataset[T], condition: Column,
                  order1: Seq[Column], order2: Seq[Column], t: String): DataFrame = {
    df.withPlan(OrderedJoin(df.logicalPlan, other.logicalPlan,
      order1.map(_.expr), order2.map(_.expr), Some(condition.expr), OrderedJoinType(t)))
  }
}
