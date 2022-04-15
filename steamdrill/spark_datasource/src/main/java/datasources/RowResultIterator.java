package datasources;

import org.apache.log4j.Logger;
import org.apache.spark.sql.catalyst.InternalRow;
import org.apache.spark.sql.catalyst.expressions.GenericInternalRow;
import org.apache.spark.sql.catalyst.util.GenericArrayData;
import org.apache.spark.sql.types.*;
import org.apache.spark.unsafe.types.UTF8String;

import java.io.Closeable;
import java.io.IOException;
import java.util.*;


public class RowResultIterator<T extends Iterator<String> & Closeable> implements Iterator<InternalRow>, Closeable{
    static Logger logger = Logger.getLogger(RowResultIterator.class.getName());

    private static Object convertLong(String entry) {
        return Long.parseLong(entry, 16);
    }
    private static Object convertInt(String entry) { return new Integer((int)Long.parseLong(entry, 16)); }
    private static Object convertString(String entry) { return UTF8String.fromString(entry);}
    private static Object convertBoolean(String entry) { return Boolean.parseBoolean(entry);}

    private static Object convertArray(DataType array, String entry) {
        String[] items = entry.split(",", 0);
        Object[] output = {};
        if (items.length > 0) {
            output = new Object[items.length - 1];

            for (int i = 1; i < items.length; ++i) {
                String item = items[i].trim();
                output[i - 1] = convert(((ArrayType) array).elementType(), item);
            }
        }
        return new GenericArrayData(output);
    }

    private static Object convertMap(DataType type, String entry) {
        MapType map = (MapType) type;
        ArrayBasedMapBuilder builder = new ArrayBasedMapBuilder(map.keyType(), map.valueType());

        String[] items = entry.split(",", -1);
        for (int i = 1; i < items.length; ++i) {
            String[] keyVal = items[i].split(":");

            Object key = convert(map.keyType(), keyVal[0].trim());
            Object val = convert(map.valueType(), keyVal[1].trim());

            builder.put(key, val);

        }
        return builder.build();
    }

    private static Object convertStruct(DataType type, String entry) {
        StructField[] structFields = ((StructType) type).fields();

        // it kinda msatters that structs are NOT recursive...!
        String[] fields = entry.split(":");

        Object[] values = new Object[fields.length];

        for (int i = 0; i < fields.length; ++i) {
            values[i] = convert(structFields[i].dataType(), fields[i]);
        }

        return new GenericInternalRow(values);
    }

    private static Object convert(DataType dt, String entry) {

        // logger.trace("converting a " + dt.toString() + " with value " + entry);
      if (entry.length() == 0) {
        return null;
      }
      else if (dt.sameType(DataTypes.LongType)) {
        return convertLong(entry);
      }
      else if (dt.sameType(DataTypes.IntegerType)) {
        return convertInt(entry);
      }
      else if (dt.sameType(DataTypes.StringType)) {
        return convertString(entry);
      }
      else if (dt.sameType(DataTypes.BooleanType)) {
        return convertBoolean(entry);
      }
      else if (dt instanceof ArrayType) {
        return convertArray(dt, entry);
      }
      else if (dt instanceof MapType) {
        return convertMap(dt, entry);
      }
      else if (dt instanceof StructType) {
        return convertStruct(dt, entry);
      }
      else {
        logger.error("No converter for " + dt.simpleString());
        assert(false);
      }
      return null;
    }

    T current;
    List<DataType> output;
    Object[] valBuf;
    String sep;

    public RowResultIterator(T res, List<DataType> output, String sep) {

        StringBuilder sb = new StringBuilder();
        for (DataType dt : output) {
            sb.append(dt.toString());
        }

        this.output = output;
        this.valBuf = new Object[output.size()];
        this.current = res;
        this.sep = sep;
        // this.joiner = joiner;
    }

    @Override
    public boolean hasNext() {
        return current.hasNext();
    }

    @Override
    public InternalRow next() {
        // create output values and initialize everything to null (indicating does not exist)

        // now update values for all elements in the next row:
        String nxt = current.next();
        String[] entries = nxt.split(sep, -1);

        //        logger.error("nextrow " + nxt);
        //        logger.error("entries: " + entries.length + " output " + output.size());

        for (int i = 1; i < entries.length; ++i) {
            String entry = entries[i].trim();
            if (entry.length() != 0) {
              // -1 accounts for some weirdness with ,'s or something
              valBuf[i - 1] = convert(output.get(i - 1), entry);
            }
        }

        return new GenericInternalRow(valBuf);
    }

    @Override
    public void close() throws IOException {
        current.close();
    }
}
