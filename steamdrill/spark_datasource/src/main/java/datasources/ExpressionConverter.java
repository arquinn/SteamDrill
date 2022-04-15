package datasources;

import helpers.TracingCallExpression$;
import helpers.TracingCast;
import io.grpc.steamdrill.*;


import io.grpc.steamdrill.Metadata;
import org.apache.spark.sql.Column;
import org.apache.spark.sql.catalyst.expressions.Literal;

import org.apache.spark.sql.catalyst.util.ArrayData;
import org.apache.spark.sql.types.*;
import helpers.TracingCallExpression;

import org.apache.log4j.Logger;
import org.apache.spark.sql.catalyst.expressions.*;
import org.apache.zookeeper.Op;
import scala.Tuple2;

import java.util.*;

import static org.apache.spark.sql.types.DataTypes.StringType;


class ExpressionConverter {

    private InternalRowConverter cData;
    private ArrayList<Alias> aliases;

    private HashMap<String, TracingCodeDef> tracingFunctions;
    private static Logger logger = Logger.getLogger(ExpressionConverter.class.getName());

    public ExpressionConverter(ArrayList<Alias> columns) {
        cData = null;
        aliases = new ArrayList<>();
        for (Alias a : columns) {
            aliases.add(a);
        }
        tracingFunctions = new HashMap<>();
    }
    public void loadRow(InternalRowConverter irc) {
        logger.trace("loading row: " + irc.toString());
        cData = irc;
    }

    public static Boolean canHandle(Expression e, List<Attribute> joins) {
        Class neClass = e.getClass();
        StringBuffer l = new StringBuffer();
        for (Attribute j : joins) {
            l.append(j.toString());
            if (j.semanticEquals(e)) {
                return true;
            }
        }
        if (neClass == Literal.class) {
            return true;
        } else if (e instanceof AttributeReference) {
            return  ColumnIDToColumn(((AttributeReference)e).name()) != null;
        } else if (e instanceof GetArrayItem) {
            GetArrayItem oper = (GetArrayItem)e;
            return oper.left() instanceof AttributeReference &&
                    ((AttributeReference)oper.left()).name().equalsIgnoreCase("Mem") &&
                    canHandle(oper.right(), joins);

        }else if (e instanceof BinaryOperator  ||
                e instanceof Alias ||
                e instanceof Cast ||
                e instanceof ArrayContains ||
                e instanceof TracingCallExpression ||
                e instanceof Concat ||
                e instanceof TracingCast ||
                e instanceof GetJsonObject) {
            scala.collection.Iterator<Expression> child = e.children().iterator();
            while (child.hasNext()) {
                if (!canHandle(child.next(), joins)) {
                    return false;
                }
            }
            return true;
       }
        else if (!(e instanceof CreateNamedStruct)) {
            logger.error("cannot handle! " + e + " type " + e.getClass().getSimpleName());
        }
        return false;
    }


    private static String getDataType(org.apache.spark.sql.types.DataType inputDT) {
        if (inputDT == DataTypes.IntegerType) {
            return "int";
        } else if (inputDT == DataTypes.LongType) {
            return "long";
        } else if (inputDT == StringType) {
            return "string";
        } else if (inputDT instanceof org.apache.spark.sql.types.MapType) {
            org.apache.spark.sql.types.MapType mt = (org.apache.spark.sql.types.MapType) inputDT;
            String ktype = getDataType(mt.keyType());
            String vtype = getDataType(mt.valueType());

            return "unordered_map<" + ktype + "," + vtype + ">";
        }

        logger.error("Cannot parse data type! " + inputDT);
        return "";
    }


    private int tcIndex = 0;
    private String addTracingCall(String code, String dt, List<String> inputs) {
        logger.trace("add tracing call for " + code);
        if(tracingFunctions.containsKey(code)) {
            return tracingFunctions.get(code).getName();
        }

        String name = String.format("tc%d", tcIndex);
        tcIndex++;

        TracingCodeDef.Builder tcd = TracingCodeDef.newBuilder();
        tcd.setName(name);
        tcd.setCode(code);
        tcd.setReturnType(dt);
        for (String in : inputs) {
            tcd.addInputs(in);
        }
        tracingFunctions.put(code, tcd.build());
        return name;
    }
    HashMap<String, TracingCodeDef> getTracingFunctions() { return tracingFunctions;}


    private static SDExpression ColumnIDToColumn(String s) {

        SDExpression.Builder build = SDExpression.newBuilder();
        if (s.equalsIgnoreCase("EAX")) {
           build.setReg(Register.newBuilder().setVal(Register.DoubleWord.EAX));
        } else if (s.equalsIgnoreCase("EBX")) {
            build.setReg(Register.newBuilder().setVal(Register.DoubleWord.EBX));
        } else if (s.equalsIgnoreCase("ECX")) {
            build.setReg(Register.newBuilder().setVal(Register.DoubleWord.ECX));
        } else if (s.equalsIgnoreCase("EDX")) {
            build.setReg(Register.newBuilder().setVal(Register.DoubleWord.EDX));
        } else if (s.equalsIgnoreCase("EDI")) {
            build.setReg(Register.newBuilder().setVal(Register.DoubleWord.EDI));
        } else if (s.equalsIgnoreCase("ESI")) {
            build.setReg(Register.newBuilder().setVal(Register.DoubleWord.ESI));
        } else if (s.equalsIgnoreCase("ESP")) {
            build.setReg(Register.newBuilder().setVal(Register.DoubleWord.ESP));
        } else if (s.equalsIgnoreCase("EBP")) {
            build.setReg(Register.newBuilder().setVal(Register.DoubleWord.EBP));
        } else if (s.equalsIgnoreCase("EIP")) {
            build.setReg(Register.newBuilder().setVal(Register.DoubleWord.EIP));
        } else if (s.equalsIgnoreCase("LOGICAL_CLOCK")) {
            build.setMeta(Metadata.newBuilder().setVal(Metadata.MetadataType.LOGICAL_CLOCK));
        } else if (s.equalsIgnoreCase("REPLAY_CLOCK")) {
            build.setMeta(Metadata.newBuilder().setVal(Metadata.MetadataType.REPLAY_CLOCK));
        } else if (s.equalsIgnoreCase("THREAD")) {
            build.setMeta(Metadata.newBuilder().setVal(Metadata.MetadataType.THREAD));
        } else if (s.equalsIgnoreCase("PROCESS")) {
            build.setMeta(Metadata.newBuilder().setVal(Metadata.MetadataType.PROCESS));
        } else {
            return null;
        }
        return build.build();
    }

    private static io.grpc.steamdrill.Literal literalIntColumn(int i) {
        return io.grpc.steamdrill.Literal.newBuilder().setType(io.grpc.steamdrill.Literal.LiteralType.INTEGER).setIval(i).build();
    }

    private static io.grpc.steamdrill.Literal literalLongColumn(long i) {
        return io.grpc.steamdrill.Literal.newBuilder().setType(io.grpc.steamdrill.Literal.LiteralType.INTEGER).setIval((int)i).build();
    }

    private static io.grpc.steamdrill.Literal literalStringColumn(String s) {
        return io.grpc.steamdrill.Literal.newBuilder().setType(io.grpc.steamdrill.Literal.LiteralType.STRING).setSval(s).build();
    }

    private static io.grpc.steamdrill.Literal literalBoolColumn(Boolean b) {
        return io.grpc.steamdrill.Literal.newBuilder().setType(io.grpc.steamdrill.Literal.LiteralType.BOOLEAN).setBval(b).build();
    }


    SDExpression toExpression(Expression ne) {
        List<SDExpression> exprs = convert(ne);
        assert (exprs.size() == 1);
        return exprs.get(0);
    }

    // it's complete fucking bullshit that I have to do this!
    SDExpression toExpression(NamedExpression ne) {
        return toExpression((Expression)ne);
    }

    private List<SDExpression> convert(Expression ne) {
        logger.trace("toExpression on " + ne.toString());
        InternalRowConverter.RowData o = cData.getAttribute(ne);
        List<SDExpression> rtn = new ArrayList<>();
        if (o != null) {
            rtn.addAll(convertLiteral(new Literal(o.data, o.datatype)));
        }
        else {
            for (Alias a : aliases) {
                if (ne.semanticEquals(a.toAttribute())) {
                    return convert(a);
                }
            }
            if (ne instanceof Alias || ne instanceof Cast || ne instanceof Hex) {
                rtn.add(convertPassThrough(ne));
            } else if (ne instanceof Literal) {
                rtn.addAll(convertLiteral(ne));
            } else if (ne instanceof AttributeReference) {
                rtn.add(convertAttributeReference(ne));
            } else if (ne instanceof TracingCallExpression) {
                rtn.add(convertTracingCall(ne));
            } else if (ne instanceof BinaryComparison) {
                rtn.add(convertyBinaryComparison(ne));
            } else if (ne instanceof BinaryOperator) {
                rtn.add(convertBinaryOperator(ne));
            } else if (ne instanceof GetArrayItem) {
                rtn.add(convertGetArrayItem(ne));
            } else if (ne instanceof TracingCast) {
                rtn.add(convertTracingCast(ne));
            } else if (ne instanceof GetJsonObject) {
                rtn.add(convertGetJsonObject(ne));
            }
            /*else if (neClass == IsNotNull.class) {
                return convertIsNotNull(ne);
            }*/
             else if (ne instanceof ArrayContains) {
                rtn.add(convertArrayContains(ne));
            } else {
                logger.error("Couldn't convert " + ne.toString());
            }
        }
        return rtn;
    }

    private SDExpression convertGetJsonObject(Expression input) {
        SDField.Builder sdfield = SDField.newBuilder();
        GetJsonObject gjo = (GetJsonObject) input;

        String field = toExpression(gjo.path()).getLit().getSval();
        assert (field.length() > 0);

        sdfield.setField(field);
        sdfield.setExpr(toExpression(gjo.json()));
        return SDExpression.newBuilder().setField(sdfield).build();
    }

    private SDExpression convertTracingCast(Expression input) {
        SDCast.Builder cast = SDCast.newBuilder();
        TracingCast tc = (TracingCast) input;

        String type = toExpression(tc.typeName()).getLit().getSval();
        assert (type.length() > 0);

        cast.setType(type);
        cast.setExpr(toExpression(tc.child()));
        return SDExpression.newBuilder().setCat(cast).build();
    }


    private SDExpression convertPassThrough(Expression e) {
        assert (e.children().size() == 1);
        return toExpression(e.children().head());
    }

    private SDExpression convertGetArrayItem(Expression e) {
        GetArrayItem gai = (GetArrayItem) e;
        MemoryDereference.Builder md = MemoryDereference.newBuilder();
        return SDExpression.newBuilder()
                .setMd(md.setOperand(toExpression(gai.right())))
                .build();
    }

    private SDExpression convertTracingCall(Expression input) {
        TracingCodeCall.Builder tf = TracingCodeCall.newBuilder();
        TracingCallExpression stc = (TracingCallExpression) input;

        SDExpression code = toExpression(stc.code());
        SDExpression returnType = toExpression(stc.returnType());

        if (!returnType.hasLit())
            logger.error("tracingCall doesn't have a returnType? " + stc.toString());

        String codeString = code.getLit().getSval();
        if (codeString.length() > 0) {
            List<String> inputList = new ArrayList<>();
            scala.collection.Iterator<Tuple2<Expression, Expression>> argumentIter = stc.inputs().iterator();
            scala.collection.Iterator<Expression> parameterIter = stc.parameters().iterator();

        while (argumentIter.hasNext() && parameterIter.hasNext()) {
            Expression parameter = parameterIter.next();
            tf.addOperands(toExpression(parameter));

            Tuple2<Expression, Expression> argument = argumentIter.next();
            SDExpression inputType = toExpression(argument._1);
            if (!inputType.hasLit())
                logger.error("tracingCall input " + argument._1.toString() + " " +
                        argument._1.dataType().toString() + " missing type? ");

            String typeString = inputType.getLit().getSval();
            if (typeString.isEmpty()) {
                if (parameter.dataType() == StringType)
                    typeString = "char*";
                else if (parameter.dataType() == DataTypes.IntegerType)
                    typeString = "long";
                else if (parameter.dataType() == DataTypes.LongType)
                    typeString = "long long";
                else
                    logger.error("what in tarnations? " + parameter.dataType().typeName());
            }

            SDExpression inputName = toExpression(argument._2);
            if (!inputName.hasLit())
                logger.error("tracingCall input name doesn't.. err have name?");
            String inputNameString = inputName.getLit().getSval();

            inputList.add(typeString + " " + inputNameString);
        }

        // figure out the returnType
        String returnTypeStr = "";
        if (returnType.hasLit()) {
            returnTypeStr = returnType.getLit().getSval();
        }
        if (returnTypeStr.isEmpty()) {
            returnTypeStr = getDataType(stc.dataType());
        }

        String nameString = addTracingCall(codeString, returnTypeStr, inputList);
        tf.setFunctionName(nameString);
        }
        else {
            tf.setFunctionName("");
        }
        return SDExpression.newBuilder().setTc(tf.build()).build();
    }


    private SDExpression convertAttributeReference(Expression input) {
        AttributeReference ar = (AttributeReference) input;
        logger.trace("convertAttributeRef: " + input);
        return ColumnIDToColumn(ar.name());
    }

    private io.grpc.steamdrill.Literal convertSimpleLiteral(org.apache.spark.sql.types.DataType dt, Object o) {
        if (dt.equals(StringType))
            return literalStringColumn(o.toString());
        else if (dt.equals(DataTypes.IntegerType))
            return literalIntColumn(((Integer)o));
        else if (dt.equals(DataTypes.LongType))
            return literalLongColumn(((Long)o));
        else if (dt.equals(DataTypes.BooleanType))
            return literalBoolColumn((Boolean)o);
        else
            logger.error("convertSimpleLiteral failed! bad type " + dt.simpleString());
        return null;
    }

    private List<SDExpression> convertLiteral(Expression input) {
        Literal l = (Literal) input;
        List<SDExpression> rtn = new ArrayList<>();
        if (l.dataType() instanceof ArrayType) {
            org.apache.spark.sql.types.DataType elementType = ((ArrayType) l.dataType()).elementType();
            ArrayData ad = (ArrayData) l.value();
            for (int i = 0; i < ad.numElements(); ++i) {
                io.grpc.steamdrill.Literal newlit = convertSimpleLiteral(elementType, ad.get(i, elementType));
                rtn.add(SDExpression.newBuilder().setLit(newlit).build());
            }
        }
        else {
            rtn.add(SDExpression.newBuilder().setLit(convertSimpleLiteral(l.dataType(), l.value())).build());
        }
        return rtn;
    }


    private SDExpression convertConcat(Expression input) {
        // add myself and iterate through my children!
        Concat c = (Concat) input;

        scala.collection.Iterator<Expression> child = c.children().iterator();
        String value = "";
        while (child.hasNext()) {
            Expression nextChild = child.next();

            // seems like assert doesn't work...?
            if (!(nextChild instanceof org.apache.spark.sql.catalyst.expressions.Literal)) {
                logger.error("concat cannot handle unresolved inputs: " + nextChild.toString());
            }
            org.apache.spark.sql.catalyst.expressions.Literal l =
                    (org.apache.spark.sql.catalyst.expressions.Literal) nextChild;
            if (l.dataType().equals(StringType))
                value += l.value().toString();
            else
                value += l.value();
        }

        return SDExpression.newBuilder().setLit(literalStringColumn(value)).build();
    }


    private SDExpression convertArrayContains(Expression input) {
        ArrayContains ac = (ArrayContains) input;

        List<SDExpression> list = convert(ac.left());
        SDExpression l = toExpression(ac.right());

        SDExpression base = SDExpression.newBuilder().setBce(BinaryComparisonExpression.newBuilder()
                .setE1(l)
                .setE2(list.get(0))
                .setOp(BinaryComparisonExpression.Operator.EQUALS))
                .build();

        for (int i = 1; i < list.size(); ++i) {
            SDExpression next = SDExpression.newBuilder().setBce(BinaryComparisonExpression.newBuilder()
                    .setE1(l)
                    .setE2(list.get(i))
                    .setOp(BinaryComparisonExpression.Operator.EQUALS))
                    .build();
            base = SDExpression.newBuilder().setBoe(BinaryOperatorExpression.newBuilder()
                    .setE1(base)
                    .setE2(next)
                    .setOp(BinaryOperatorExpression.Operator.OR))
                    .build();
        }
        // it needs to be the case that r is an array that we already know about (this is how it warks):
        return base;
    }

    private SDExpression convertyBinaryComparison(Expression input) {
        BinaryComparison bc = (BinaryComparison) input;

        // the semantics of a binary comparison change depending upon struct vs. array (I think?).
        List<SDExpression> e1 = convert(bc.left());
        List<SDExpression> e2 = convert(bc.right());
        BinaryComparisonExpression.Operator op = BinaryComparisonExpression.Operator.EQUALS;
        switch (bc.symbol()){
            case "=":
                op = BinaryComparisonExpression.Operator.EQUALS;
                break;
            case ">":
                op = BinaryComparisonExpression.Operator.GREATER;
                break;
            case "<":
                op = BinaryComparisonExpression.Operator.LESS;
                break;
            case ">=":
                op = BinaryComparisonExpression.Operator.GEQ;
                break;
            case "<=":
                op = BinaryComparisonExpression.Operator.LEQ;
                break;
            default:
                logger.error("vas is das?" + bc.symbol());
        }

        if (e1.size() != e2.size()) {
            if (op == BinaryComparisonExpression.Operator.EQUALS) {
                return toExpression(new Literal(Boolean.FALSE, DataTypes.BooleanType));
            }
            // this isn't *quite* true, but seems close enough!
            return toExpression(new Literal(Boolean.TRUE, DataTypes.BooleanType));
        }

        SDExpression base = SDExpression.newBuilder().setBce(
                BinaryComparisonExpression.newBuilder().setE1(e1.get(0)).setE2(e2.get(0)).setOp(op)).build();
        for (int i = 1; i < e1.size(); ++i) {
            SDExpression add = SDExpression.newBuilder().setBce(
                    BinaryComparisonExpression.newBuilder().setE1(e1.get(i)).setE2(e2.get(i)).setOp(op)).build();
            base = SDExpression.newBuilder().setBoe(BinaryOperatorExpression.newBuilder()
                        .setE1(base)
                        .setE2(add)
                        .setOp(BinaryOperatorExpression.Operator.AND))
                    .build();
        }
        return base;
    }

    private SDExpression convertBinaryOperator(Expression input) {
        BinaryOperator bc = (BinaryOperator) input;
        BinaryOperatorExpression.Builder eeBuilder =
                BinaryOperatorExpression.newBuilder()
                        .setE1(toExpression(bc.left()))
                        .setE2(toExpression(bc.right()));

        if (bc.symbol().equalsIgnoreCase("+")) {
            eeBuilder.setOp(BinaryOperatorExpression.Operator.ADD);
        } else if (bc.symbol().equalsIgnoreCase("*")) {
            eeBuilder.setOp(BinaryOperatorExpression.Operator.MULTIPLY);
        } else if (bc.symbol().equalsIgnoreCase("&&")) {
            eeBuilder.setOp(BinaryOperatorExpression.Operator.AND);
        } else if (bc.symbol().equalsIgnoreCase("||")) {
            eeBuilder.setOp(BinaryOperatorExpression.Operator.OR);
        } else if (bc.symbol().equalsIgnoreCase("-")) {
            eeBuilder.setOp(BinaryOperatorExpression.Operator.SUB);
        }

        return SDExpression.newBuilder().setBoe(eeBuilder.build()).build();
    }
}
