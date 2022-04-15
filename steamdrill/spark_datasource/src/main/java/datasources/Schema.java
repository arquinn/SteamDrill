package datasources;

import org.apache.log4j.Logger;
import org.apache.spark.sql.types.DataType;
import org.apache.spark.sql.types.DataTypes;
import org.apache.spark.sql.types.StructField;
import org.apache.spark.sql.types.StructType;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.HashMap;

public class Schema {
    private static Logger log = Logger.getLogger(Schema.class.getName());

    private StructType schema;
    private Schema(StructType type) {
        schema = type;
    }

    public StructType getStructType() {
        return schema;
    }

    public List<DataType> getTypes() {
        List<DataType> types = new ArrayList<>();
        for (StructField field : schema.fields()) {
            types.add(field.dataType());
        }
        return types;
    }

    private static DataType getType(List<String> types, HashMap<String, List<StructField>> structs, boolean nullable) {
        assert (types.size() > 0);
        String first = types.get(0);

        if (first.compareToIgnoreCase("Long") == 0) {
            return DataTypes.LongType;
        }
        else if (first.compareToIgnoreCase("Byte") == 0) {
            return DataTypes.ByteType;
        }
        else if (first.compareToIgnoreCase("Integer") == 0) {
            return DataTypes.IntegerType;
        }
        else if (first.compareToIgnoreCase("String") == 0) {
            return DataTypes.StringType;
        }
        else if (first.compareToIgnoreCase("Boolean") == 0) {
            return DataTypes.BooleanType;
        }
        else if (first.compareToIgnoreCase("Array") == 0) {
            assert(types.size() >= 2);
            return DataTypes.createArrayType(getType(Arrays.asList(types.get(1)), structs, nullable), nullable);
        }
        else if (first.compareToIgnoreCase("Map") == 0) {
            assert(types.size() >= 3);
            DataType key = getType(Arrays.asList(types.get(1)), structs, nullable);
            DataType val = getType(Arrays.asList(types.get(2)), structs, nullable);
            return DataTypes.createMapType(key, val);
        }
        else if (structs.containsKey(first)) {
            log.info("returning struct for " + first);
            return DataTypes.createStructType(structs.getOrDefault(first, new ArrayList<>()));
        }

        log.info("nothing found for " + first + " " + structs.containsKey(first));
        return null;
    }

    /**
     * Create the schema for the datasources based on an input file
     */
    public static Schema fromStream(InputStream schemaStream) {
        HashMap<String, List<StructField>> structs = new HashMap<>();

        BufferedReader schemaReader = new BufferedReader(new InputStreamReader(schemaStream));
        String nxtEntry;
        ArrayList<StructField> fields = new ArrayList<>();
        try {
            while ((nxtEntry = schemaReader.readLine()) != null) {
                nxtEntry = nxtEntry.replaceAll("\\s", "");
                List<String> words = Arrays.asList(nxtEntry.split(","));
                if (words.get(0).equalsIgnoreCase("StructDef")) {
                    String structName = words.get(1);
                    String fieldName = words.get(2);
                    boolean nullable = words.get(3).startsWith("T");   // this is, weird? but Okay I guess


                    log.trace("struct " + structName + " field " + fieldName);

                    DataType type = getType(words.subList(3, words.size()), structs, nullable);
                    StructField newField = DataTypes.createStructField(fieldName, type, nullable);

                    structs.putIfAbsent(structName, new ArrayList<>());
                    List<StructField> sfields = structs.get(structName);
                    sfields.add(newField);
                }
                else {
                   // log.trace("getting type for " + nxtEntry);
                    String name = words.get(0);
                    boolean nullable = words.get(1).startsWith("T");   // this is, weird? but Okay I guess

                    DataType type = getType(words.subList(2, words.size()), structs, nullable);
                    assert (type != null);

                    StructField newSF = DataTypes.createStructField(name, type, nullable);
                    //log.trace("schema " + name + " type " + type.toString() + " newSF " + newSF.toString());
                    fields.add(newSF);
                }
            }
            schemaStream.close();
        } catch (IOException blah) {
            log.info("caught IOExcpetion reading schema: " + blah);
        }
        StructType type = DataTypes.createStructType(fields);
        log.trace("type " + type.toString());
        return new Schema(type);
    }
}
