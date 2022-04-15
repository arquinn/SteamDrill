package datasources;

import org.apache.log4j.Logger;
import org.apache.spark.sql.catalyst.InternalRow;
import org.apache.spark.sql.catalyst.expressions.Attribute;
import org.apache.spark.sql.catalyst.expressions.Expression;
import org.apache.spark.sql.catalyst.expressions.Literal;
import org.apache.spark.sql.types.DataType;
import org.apache.spark.sql.types.DataTypes;
import scala.collection.Iterator;
import scala.collection.JavaConversions;
import scala.collection.Seq;

import java.util.*;


public class InternalRowConverter {
    Logger logger;

    public class RowData {
        public DataType datatype;
        public Object data;

        public RowData(DataType t, Object d) {
            datatype = t;
            data = d;
        }

        @Override
        public String toString() {
            return "(" + datatype.toString() + ", " + data.toString() + ")";
        }
    }

    HashMap<ExpressionWrapper, RowData> attrMap;;

    public InternalRowConverter(List<Attribute> output, InternalRow row) {
        logger = Logger.getLogger(InternalRowConverter.class.getName());
        ArrayList<DataType> types = new ArrayList<>();
        output.forEach((o) -> types.add(o.dataType()));

        attrMap = new HashMap<>();
        for (int i = 0; i < types.size(); ++i) {
            Object elem = row.get(i, types.get(i));

            RowData rd;
            if (elem == null)
                rd = new RowData(DataTypes.StringType, "");
           else
                rd = new RowData(types.get(i), elem);


            attrMap.put(new ExpressionWrapper(output.get(i)), rd);
            logger.trace("InternalRow adding " + output.get(i) + " " + rd.toString());
        }
    }

    public RowData getAttribute(Expression e) {
        logger.trace("getAttribute: " + e.toString());
        ExpressionWrapper ew = new ExpressionWrapper(e);
        if (attrMap.containsKey(ew))
            return attrMap.get(ew);
        return null;
    }

    @Override
    public String toString() {
        StringBuffer sb = new StringBuffer();
        sb.append("[");
        for (HashMap.Entry<ExpressionWrapper, RowData> entry : attrMap.entrySet()) {
            sb.append(entry.getKey().expr.toString()).append(":").append(entry.getValue().data.toString());
        }
        sb.append("]");
        return sb.toString();
    }
}
