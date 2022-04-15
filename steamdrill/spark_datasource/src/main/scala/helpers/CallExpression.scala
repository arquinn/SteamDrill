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

import org.apache.spark.sql.Column
import org.apache.spark.sql.catalyst.InternalRow
import org.apache.spark.sql.catalyst.expressions._
import org.apache.spark.sql.catalyst.expressions.codegen._
import org.apache.spark.sql.types.{ArrayType, DataType, DataTypes}


/**
 * Tracing code Expression.
 * @param code The code of the function to be called
 * @param returnType The return type of the function from a query perspective
 * @param unitType The dataType of the output from this expression
 * @param parameters The inputs to the tracing function.
 * @param nullable  True if the UDF can return null value.
 */
case class TracingCallExpression(
    // name: Expression,
    inputs: Seq[(Expression, Expression)],
    code: Expression,
    returnType: Expression,
    parameters: Seq[Expression],
    unitType: DataType,
    nullable: Boolean = true) extends Expression with NonSQLExpression {

  override def dataType: DataType = code.dataType match {
    case ArrayType(_, _) =>
      DataTypes.createArrayType(unitType)
    case _ =>
      unitType
    }

    override final def children: List[Expression] =
      code :: returnType :: Nil ++ parameters ++ inputs.flatMap(p => List(p._1, p._2))

    override def toString: String = s"TC::${code}(${parameters.mkString(", ")})"

    override def doGenCode(ctx: CodegenContext, ev: ExprCode): ExprCode = {
      throw new IllegalStateException(s"doGenCode should not be called! ${this.toString}")
    }

    override def eval(input: InternalRow): Any = {
      throw new IllegalStateException("ScalaTracingFunction.eval should not be called!")
    }
  }

/**
 * Hack for getting variable types from the SD perspective
 * @param dataType
 * @param nullable
 */
case class TracingCast(child: Expression,
                       typeName: Expression,
                       dataType: DataType = DataTypes.StringType,
                       nullable: Boolean = false) extends Expression with NonSQLExpression {

  def this(child: Column, typeString : String) {
    this(child.expr, Literal(typeString))
  }

  def this(child: Column, typeString : Column) {
    this(child.expr, typeString.expr)
  }

  override final def children: List[Expression] = child :: typeName :: Nil

  override def toString: String = s"TracingCast[${child} as ${typeName}]"

  override def doGenCode(ctx: CodegenContext, ev: ExprCode): ExprCode = {
    throw new IllegalStateException(s"doGenCode should not be called! ${this.toString}")
  }

  override def eval(input: InternalRow): Any = {
    throw new IllegalStateException("ScalaTracingFunction.eval should not be called!")
  }
}