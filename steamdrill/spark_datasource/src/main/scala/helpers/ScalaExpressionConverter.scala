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

//import helpers.TracingCast
//import helpers.TracingCallExpression


import datasources.InternalRowConverter

import scala.collection.mutable.HashMap
// import io.grpc.steamdrill._

import io.grpc.steamdrill.BinaryComparisonExpression
import io.grpc.steamdrill.BinaryOperatorExpression
import io.grpc.steamdrill.MemoryDereference
import io.grpc.steamdrill.Metadata
import io.grpc.steamdrill.Register
import io.grpc.steamdrill.SDCast
import io.grpc.steamdrill.SDExpression
import io.grpc.steamdrill.SDField
import io.grpc.steamdrill.TracingCodeCall
import io.grpc.steamdrill.TracingCodeDef


import org.apache.spark.sql.Column
import org.apache.spark.sql.catalyst.expressions.Literal
import org.apache.spark.sql.catalyst.util.ArrayData
import org.apache.spark.sql.types._
import org.apache.spark.sql.catalyst.expressions._
import org.apache.spark.internal.Logging
import org.apache.spark.sql.types.DataTypes.StringType


object ScalaExpressionConverter extends Logging{

  def canHandle(e: Expression, joins: Seq[Attribute]): Boolean = {
    // base case?
    for (j <- joins) {
      if (j.semanticEquals(e)) return true
    }
    e match {
      case ar : AttributeReference =>
        ColumnIDToColumn(ar.name) != null
      case _ : Literal =>
        true
      case gai : GetArrayItem =>
        gai.left.isInstanceOf[AttributeReference] &&
          gai.left.asInstanceOf[AttributeReference].name.equalsIgnoreCase("Mem") &&
          canHandle(gai.right, joins)
      case chillins @ (_: BinaryOperator | _ : Alias | _ : Cast |
                       _ : ArrayContains | _ : TracingCallExpression |
                       _ : Concat | _ : TracingCast | _ : GetJsonObject) =>
        chillins.children.map(c => canHandle(c, joins)).reduce (_ && _)
      case _ =>
        logInfo(s"Cannot handle $e")
        false
    }
  }

  def getDataType(inputDT: DataType): String = inputDT match {
    case DataTypes.IntegerType =>
      "long"
    case DataTypes.LongType =>
      "long long"
    case DataTypes.StringType =>
      "char *"
    case _ =>
      logError(s"Cannot parse dataType $inputDT")
      "int"
  }

  def ColumnIDToColumn(s: String): SDExpression = {
    val build = SDExpression.newBuilder
    s.toUpperCase() match {
      case "EAX" =>
        SDExpression.newBuilder.setReg(Register.newBuilder.setVal(Register.DoubleWord.EAX)).build()
      case "EBX" =>
        SDExpression.newBuilder.setReg(Register.newBuilder.setVal(Register.DoubleWord.EBX)).build()
      case "ECX" =>
        SDExpression.newBuilder.setReg(Register.newBuilder.setVal(Register.DoubleWord.ECX)).build()
      case "EDX" =>
        SDExpression.newBuilder.setReg(Register.newBuilder.setVal(Register.DoubleWord.EDX)).build()
      case "EDI" =>
        SDExpression.newBuilder.setReg(Register.newBuilder.setVal(Register.DoubleWord.EDI)).build()
      case "ESI" =>
        SDExpression.newBuilder.setReg(Register.newBuilder.setVal(Register.DoubleWord.ESI)).build()
      case "ESP" =>
        SDExpression.newBuilder.setReg(Register.newBuilder.setVal(Register.DoubleWord.ESP)).build()
      case "EBP" =>
        SDExpression.newBuilder.setReg(Register.newBuilder.setVal(Register.DoubleWord.EBP)).build()
      case "EIP" =>
        SDExpression.newBuilder.setReg(Register.newBuilder.setVal(Register.DoubleWord.EIP)).build()
      case "LOGICAL_CLOCK" =>
        SDExpression.newBuilder.setMeta(Metadata.newBuilder.setVal(Metadata.MetadataType.LOGICAL_CLOCK)).build()
      case "REPLAY_CLOCK" =>
        SDExpression.newBuilder.setMeta(Metadata.newBuilder.setVal(Metadata.MetadataType.REPLAY_CLOCK)).build()
      case "THREAD" =>
        SDExpression.newBuilder.setMeta(Metadata.newBuilder.setVal(Metadata.MetadataType.THREAD)).build()
      case "PROCESS" =>
        SDExpression.newBuilder.setMeta(Metadata.newBuilder.setVal(Metadata.MetadataType.PROCESS)).build()
      case _ =>
        null
    }
  }
  def literalIntColumn(i: Int) =
    io.grpc.steamdrill.Literal.newBuilder
      .setType(io.grpc.steamdrill.Literal.LiteralType.INTEGER)
      .setIval(i).build

  def literalShortColumn(i : Int) =
    io.grpc.steamdrill.Literal.newBuilder
      .setType(io.grpc.steamdrill.Literal.LiteralType.SHORT)
      .setIval(i).build

  def literalLongColumn(i: Long) =
    io.grpc.steamdrill.Literal.newBuilder
      .setType(io.grpc.steamdrill.Literal.LiteralType.INTEGER)
      .setIval(i.toInt).build
  def literalStringColumn(s: String) =
    io.grpc.steamdrill.Literal.newBuilder
      .setType(io.grpc.steamdrill.Literal.LiteralType.STRING)
      .setSval(s).build
  def literalBoolColumn(b: Boolean) =
    io.grpc.steamdrill.Literal.newBuilder
      .setType(io.grpc.steamdrill.Literal.LiteralType.BOOLEAN)
      .setBval(b).build
}


class ScalaExpressionConverter(val aliases: Seq[Alias]) extends Logging{

  private val tracingFunctions = new HashMap[String, TracingCodeDef]
  private var cData : InternalRowConverter = _
  private var tcIndex = 0

  def loadRow(irc: InternalRowConverter): Unit = {
    cData = irc
  }

  private def addTracingCall(code: String, dt: String, inputs: Seq[String]): String = {
    val defined = tracingFunctions.get(code)
    if (defined.isDefined) {
      defined.get.getName
    } else {
      val name = s"tc$tcIndex"
      tcIndex += 1
      val tcd = TracingCodeDef.newBuilder.setName(name).setCode(code).setReturnType(dt)

      for (in <- inputs) {
        tcd.addInputs(in)
      }
      tracingFunctions.put(code, tcd.build)
      name
    }
  }

  def getTracingFunctions: HashMap[String, TracingCodeDef] = tracingFunctions

  def toExpression(ne: Expression) : SDExpression = {
    val exprs = convert(ne)
    assert(exprs.size == 1)
    exprs(0)
  }

  private def convert(ne: Expression): Seq[SDExpression] = {
    if (ne == null) {
      logError("nully boi")
      assert (false)
    }
    val o = if (cData != null && cData.getAttribute(ne) != null) {
      cData.getAttribute(ne)
    }
    else {
      null
    }

    if (o != null) {
      convertLiteral(new Literal(o.data, o.datatype))
    }
    else {
      for (a <- aliases) {
        if (ne.semanticEquals(a.toAttribute)) return convert(a)
      }
      ne match {
        case l: Literal =>
          convertLiteral(l)
        case ar: AttributeReference =>
          convertAttributeReference(ar) :: Nil
        case tce: TracingCallExpression =>
          convertTracingCall(tce):: Nil
        case bc: BinaryComparison =>
          convertyBinaryComparison(bc):: Nil
        case bo: BinaryOperator =>
          convertBinaryOperator(bo) :: Nil
        case _@(_: Alias | _: Cast | _: Hex) =>
          convertPassThrough(ne):: Nil
        case gai: GetArrayItem =>
          convertGetArrayItem(gai):: Nil
        case tc: TracingCast =>
          convertTracingCast(tc):: Nil
        case gjo: GetJsonObject =>
          convertGetJsonObject(gjo) :: Nil
        case ac: ArrayContains =>
          convertArrayContains(ac) :: Nil
      }
    }
  }

  private def convertGetJsonObject(gjo: GetJsonObject): SDExpression = {
    val sdfield = SDField.newBuilder
    val field = toExpression(gjo.path).getLit.getSval
    assert(field.length > 0)
    sdfield.setField(field)
    sdfield.setExpr(toExpression(gjo.json))
    SDExpression.newBuilder.setField(sdfield).build
  }

  private def convertTracingCast(tc: TracingCast): SDExpression = {
    val cast = SDCast.newBuilder
    val `type` = toExpression(tc.typeName).getLit.getSval
    assert(`type`.length > 0)
    cast.setType(`type`)
    cast.setExpr(toExpression(tc.child))
    SDExpression.newBuilder.setCat(cast).build
  }

  private def convertPassThrough(e: Expression): SDExpression = {
    assert(e.children.size == 1)
    toExpression(e.children.head)
  }

  private def convertGetArrayItem(gai: GetArrayItem): SDExpression = {
    val md = MemoryDereference.newBuilder
    SDExpression.newBuilder.setMd(md.setOperand(toExpression(gai.right))).build
  }

  private def convertTracingCall(stc: TracingCallExpression): SDExpression = {
    val code = toExpression(stc.code)
    val returnType = toExpression(stc.returnType)
    if (! returnType.hasLit) {
      logError("tracingCall doesn't have a returnType? " + stc.toString)
    }

    val codeString = code.getLit.getSval
    val tf = if (codeString.length > 0) {
      val tracingCall = TracingCodeCall.newBuilder

      for (par <- stc.parameters) {
        tracingCall.addOperands(toExpression(par))
      }

      val inputs = (stc.inputs zip stc.parameters).map(p => {
        val fromInputs : String = toExpression(p._1._1).getLit.getSval
        val typeString =
          if (fromInputs.nonEmpty) {
            fromInputs
          } else {
            ScalaExpressionConverter.getDataType(p._2.dataType)
          }
        val input = toExpression(p._1._2).getLit.getSval
        typeString + " " +  input
      })
      val retType = if (returnType.hasLit) {
        returnType.getLit.getSval
      } else {
        ScalaExpressionConverter.getDataType(stc.dataType)
      }

      val nameString = addTracingCall(codeString, retType, inputs)
      tracingCall.setFunctionName(nameString)
    }
    else {
      TracingCodeCall.newBuilder.setFunctionName("")
    }
    SDExpression.newBuilder.setTc(tf.build).build
  }

  private def convertAttributeReference(ar: AttributeReference) : SDExpression = {
    ScalaExpressionConverter.ColumnIDToColumn(ar.name)
  }

  private def convertSimpleLiteral(dt: DataType, o: Any): io.grpc.steamdrill.Literal = dt match {
    case StringType =>
      ScalaExpressionConverter.literalStringColumn(o.toString)
    case DataTypes.IntegerType =>
      ScalaExpressionConverter.literalIntColumn(o.asInstanceOf[Integer])
    case DataTypes.ShortType =>
      ScalaExpressionConverter.literalShortColumn(o.asInstanceOf[Integer])
    case DataTypes.LongType =>
      ScalaExpressionConverter.literalLongColumn(o.asInstanceOf[Long])
    case DataTypes.BooleanType =>
      ScalaExpressionConverter.literalBoolColumn(o.asInstanceOf[Boolean])
    case _ =>
      logError(s"Oh no! bad type ${dt.simpleString}")
      null
  }

  private def convertLiteral(l : Literal) : Seq[SDExpression] = l.dataType match {
    case ArrayType(elementType, _) =>
      val ad = l.value.asInstanceOf[ArrayData]
      (0 until ad.numElements()).map(x => SDExpression.newBuilder.setLit(
                                  convertSimpleLiteral(elementType, ad.get(x, elementType))).build)
    case _ =>
      SDExpression.newBuilder.setLit(convertSimpleLiteral(l.dataType, l.value)).build :: Nil
  }

  private def convertConcat(c: Concat) : SDExpression = {
    // add myself and iterate through my children!
    val name = c.children.foldLeft("") {
      (left, right) => right match {
        case rlit : Literal =>
          left + rlit.value.toString
        case _ =>
          left
      }
    }
    SDExpression.newBuilder.setLit(ScalaExpressionConverter.literalStringColumn(name)).build
  }

  private def convertArrayContains(ac: ArrayContains): SDExpression = {
    val haystack = convert(ac.left)

    val needle = toExpression(ac.right)

    val equals = haystack.map(hay => SDExpression
                              .newBuilder.setBce(BinaryComparisonExpression
                                         .newBuilder.setE1(needle)
                                                    .setE2(hay)
                                                    .setOp(BinaryComparisonExpression.Operator.EQUALS))
                                                    .build)

    equals.tail.foldLeft(equals.head) {(left, right) => SDExpression.newBuilder
                                   .setBoe(BinaryOperatorExpression.newBuilder.setE1(left)
                                                                              .setE2(right)
                                       .setOp(BinaryOperatorExpression.Operator.OR)).build
    }
  }

  private def convertyBinaryComparison(input: Expression): SDExpression = {
    val bc = input.asInstanceOf[BinaryComparison]
    // the semantics of a binary comparison change depending upon struct vs. array (I think?).
    val e1 = convert(bc.left)
    val e2 = convert(bc.right)
    val op = bc.symbol match {
      case "=" =>
        BinaryComparisonExpression.Operator.EQUALS
      case ">" =>
        BinaryComparisonExpression.Operator.GREATER
      case "<" =>
        BinaryComparisonExpression.Operator.LESS
      case ">=" =>
        BinaryComparisonExpression.Operator.GEQ
      case "<=" =>
        BinaryComparisonExpression.Operator.LEQ
      case _ =>
        logError("vas is das?" + bc.symbol)
        BinaryComparisonExpression.Operator.EQUALS
    }
    if (e1.size != e2.size) {
      if (op eq BinaryComparisonExpression.Operator.EQUALS) {
        toExpression(new Literal(false, DataTypes.BooleanType))
      } else {
        toExpression(new Literal(true, DataTypes.BooleanType))
      }
      // this isn't *quite* true, but seems close enough!
    } else {
      (e1 zip e2).map(p => {
        SDExpression.newBuilder.setBce(BinaryComparisonExpression.newBuilder.setE1(p._1).setE2(p._2).setOp(op)).build
      }).reduceLeft((left, right) => {
        SDExpression.newBuilder.setBoe(BinaryOperatorExpression.newBuilder.setE1(left).setE2(right).setOp(BinaryOperatorExpression.Operator.AND)).build
      })
    }
  }

  private def convertBinaryOperator(bc: BinaryOperator) : SDExpression = {
    val oper = bc.symbol match {
      case "+" =>
        BinaryOperatorExpression.Operator.ADD
      case "*" =>
        BinaryOperatorExpression.Operator.MULTIPLY
      case "&&" =>
        BinaryOperatorExpression.Operator.AND
      case "||" =>
        BinaryOperatorExpression.Operator.OR
      case "-" =>
        BinaryOperatorExpression.Operator.SUB
    }
    SDExpression.newBuilder.setBoe(
      BinaryOperatorExpression.newBuilder.setE1(toExpression(bc.left))
                                          .setE2(toExpression(bc.right))
                                          .setOp(oper)
    ).build()
  }
}
