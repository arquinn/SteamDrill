package datasources;


import helpers.Config;
import org.apache.log4j.Logger;

import org.apache.spark.sql.catalyst.InternalRow;
import org.apache.spark.sql.catalyst.expressions.*;
import org.apache.spark.sql.sources.v2.DataSourceOptions;
import org.apache.spark.sql.sources.v2.DataSourceV2;
import org.apache.spark.sql.sources.v2.ReadSupport;
import org.apache.spark.sql.sources.v2.reader.*;
import org.apache.spark.sql.types.DataType;
import org.apache.spark.sql.types.StructType;
import scala.Equals;

import java.io.*;
import java.util.*;


/**
 * A DataSource for user passed programs. Generic API for adding auxilary data to
 * a Steamdrill Query.
 */
public class ProgramDataSource implements DataSourceV2, ReadSupport {

  public static final String EXECUTABLE_KEY = "EXECUTABLE_DIR";
  public static final String SCHEMA_KEY = "SCHEMA_FILE";
  public static final String INPUT_KEY = "INPUTS";

  /**
   * Spark calls this to create the reader.
   * @param options the options for this programDataSource (executable, scheam, inputs)
   * @return A new DataSource Reader based on the configuration
   */
  @Override
  public DataSourceReader createReader(DataSourceOptions options) {
    Optional<String> exec = options.get(EXECUTABLE_KEY);
    Optional<String> inputs = options.get(INPUT_KEY);

    return new ProgramReader(options.get(SCHEMA_KEY).orElse(""),
                             exec.orElse(""),
                             inputs.isPresent() ? inputs.get().split(" ") : new String[0]);
  }
}


/**
 * A Reader used by spark to actually process a query for a ProgramDataSource.
 * Figures out the schema, and creates the factory datastructure that actually allows
 * the running of the program.
 */
class ProgramReader implements DataSourceReader {

  static Logger log = Logger.getLogger(ProgramReader.class.getName());
  private Schema schema;
  private String executable;
  private List<String> inputs;

  public ProgramReader(String schemaFile, String exec, String[] inputs) {
    try {
      this.schema = Schema.fromStream(new FileInputStream(schemaFile));
      this.inputs = Arrays.asList(inputs);
      this.executable = exec;
    }
    catch (IOException e) {
      log.error("Cannot create Reader due to IOException " + e.toString());
    }
  }
  /*
   * Returns the schema
   */
  @Override
  public StructType readSchema() {
    return schema.getStructType();
  }

  /**
   * Returns an InputPartition to produce the requested data.
   */
  @Override
  public List<InputPartition<InternalRow>> planInputPartitions() {
    return java.util.Arrays.asList(
        new ProgramInputPartition(schema.getTypes(), executable, inputs));
  }
}

/**
 * A factory for building the class that actually does the reading.
 * Note that this has to be serializable. Each instance is sent to an executor,
 * which uses it to create a reader for its own use.
 */

class ProgramInputPartition implements InputPartition<InternalRow> {
  static Logger log = Logger.getLogger(ProgramInputPartition.class.getName());
  List<DataType> output;
  String exec;
  String[] Ins;

  public ProgramInputPartition(List<DataType> output, String exec, List<String> dIns) {
    log.info("Program Input Partitions for " + exec);
    this.output = output;
    this.exec = exec;
    this.Ins = new String[dIns.size()];
    dIns.toArray(this.Ins);
  }

  @Override
  public InputPartitionReader<InternalRow> createPartitionReader() {
    log.info("createParititionReader");
    return new ProgramPartitionReader(output, exec, Ins);
  }
}

class ProgramPartitionReader implements InputPartitionReader<InternalRow> {
  static Logger log = Logger.getLogger(ProgramPartitionReader.class.getName());

  Iterator<InternalRow> procIter;
  InternalRow procRow;

  // for re-creating startExecutable as needed
  List<DataType> output;
  String exec;
  public ProgramPartitionReader(List<DataType> out, String executable, String[] inputs) {
    log.info("programPartitionReader generation");

    output = out;
    exec = executable;
    procIter = null;
    procIter = startExecutable(inputs);
  }

  RowResultIterator<LineIterator> startExecutable(String[] inputs) {
    String[] commandArgs = new String[inputs.length + 1];
    commandArgs[0] = exec;
    System.arraycopy(inputs, 0, commandArgs, 1, inputs.length);

    try {
      ProcessBuilder pb = new ProcessBuilder(commandArgs).redirectError(new File("/tmp/blahblahblah"));
      Map<String, String> env = pb.environment();
      env.put("LD_LIBRARY_PATH", Config.LdLibraryPath());

      Process p = pb.start();
      return new RowResultIterator<>(new LineIterator(p.getInputStream()), output, "\u0002");
    } catch(Exception e) {
      log.info("Problem in reader...? " + e.toString());
    }
    return null;
  }
  @Override
  public boolean next()  {
    if (procIter != null && procIter.hasNext()) {
      procRow = procIter.next();
      return true;
    }
    return false;
  }

  @Override
  public InternalRow get() {
    // log.info("getting row: we have prco as " + procRow);
    return procRow;
  }
  @Override
  public void close() {}
}



