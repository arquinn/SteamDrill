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

import org.apache.spark.internal.Logging
import org.apache.spark.sql.Column
import org.apache.spark.sql.catalyst.expressions.{Expression, Literal}
import org.apache.spark.sql.functions.{col, lit}
import org.apache.spark.sql.types.{DataType, DataTypes, MapType, StringType}

class InputList() {
  var offset = 0

  var seq: Seq[(String, String)] = Nil
  val typeSep = ','
  def addInput(name: String): (String, String) = {

    val endOfType = name.indexOf(typeSep, 0)
    val (t, column): (String, String) = if (endOfType != -1) {
      (name.substring(0, endOfType), name.substring(endOfType + 1))
    }
    else {
      ("string", name)
    }

    val patched = column.replace('.', '_')
    seq = seq :+ (t, patched)
    (column, patched)
  }
}

/**
 * A tracing function that is declared in line. That is, the actual c++ code lives is IN THE QUERY
 * (super useful for one-liners).
 *
 * This really only exists for the apply method (which is helpful for calling it).
 *
 * @since 1.3.0
 */
object TracingCode extends Logging
{
  var offset = 0
  val colSep = '#'
  def nextName : String = {
    val co = offset
    offset += 1
    "tc" + co
  }

  // this should obviously use the code section below (should just call the other helper)
  def code(input: String, returnT : String = "string"): Column = {
    var code = new StringBuilder
    val inputs = new InputList
    var cols: Seq[Column] = Nil

    var endOfCol = 0
    var nxtIndex = input.indexOf(colSep)

    // logInfo(s"looking for ${colSep} in $input yields $nxtIndex ")

    while (nxtIndex != -1) {
      code ++= input.substring(endOfCol, nxtIndex)
      logInfo(s"added string from $endOfCol to $nxtIndex ${code.toString}")


      // now find the end of the column and add it as input

      endOfCol = input.indexOf(colSep, nxtIndex + 1)
      assert (endOfCol != -1)
      logInfo(s"nxtCol from $nxtIndex to $endOfCol ${input.substring(nxtIndex + 1, endOfCol)}")
      val colInput = input.substring(nxtIndex + 1, endOfCol)
      val (colname, codename) = inputs.addInput(colInput)

      cols = cols :+ new Column(colname)
      code ++= codename

      // do it all again!
      nxtIndex = input.indexOf(colSep, endOfCol + 1)
    }

    code ++= input.substring(endOfCol + 1)
    // logInfo(s"code is ${code.toString}")

    val dt = {
      if (returnT.equals("bool")) {
        DataTypes.BooleanType
      } else if (returnT.equals("string")) {
        DataTypes.StringType
      } else if (returnT.equals("int")) {
        DataTypes.IntegerType
      } else if (returnT.equals("u_char")) {
        DataTypes.IntegerType
      } else {
        DataType.fromJson("\"" + returnT + "\"")
      }
    }

    new TracingCode(s"{return ${code.toString};}", inputs.seq, returnT, dt)(cols : _*)
  }
}



case class TracingCode  ( code: Expression,
                          inputs: Seq[(Column, String)] = Nil,
                          returnType: Expression = Literal(""),
                          dataType: DataType = DataTypes.StringType) extends Logging {

  val name = Literal.apply(TracingCode.nextName)

  def this(code: String, in: Seq[(String, String)], ret: String, dt: DataType) {
    this(Literal(code), in.map(p => (lit(p._1), p._2)), Literal(ret), dt)
  }

  def this(code: Column, in: Seq[(String, String)], ret: Column, dt: DataType) {
    this(code.expr, in.map(p => (lit(p._1), p._2)), ret.expr)
  }

  def this(code: Column, ret: Column, dt: DataType) {
    this(code.expr, Nil, ret.expr, dt)
  }

  def this(code: Column, ret: Column) {
    this(code.expr, Nil, ret.expr)
  }

  def this(code: String) {
    this(Literal(code))
  }

  /**
   * Returns an expression that invokes the UDF, using the given arguments.
   */
  @scala.annotation.varargs
  def apply(exprs: Column*): Column = {
    val tce = TracingCallExpression(inputs.map(i => (i._1.expr, Literal(i._2))),
      code,
      returnType,
      exprs.map(_.expr),
      dataType)
    new Column(tce)
  }

  /**
   * Returns an expression that invokes the UDF, using the given arguments.
   */
  @scala.annotation.varargs
  def apply(expr: String, exprs: String*): Column = {
    val cols = expr +: exprs
    apply(cols.map(new Column(_)): _*)
  }

  /**
   * Returns an expression that invokes the UDF, using the given arguments.
   */
  def apply(): Column = {
    assert(inputs.isEmpty)
    new Column(TracingCallExpression(Nil, code, returnType, Nil, dataType))
  }
}