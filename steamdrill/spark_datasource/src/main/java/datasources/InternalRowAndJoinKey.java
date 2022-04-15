package datasources;

import org.apache.spark.sql.catalyst.InternalRow;

public class InternalRowAndJoinKey {
    public InternalRow r;
    public int joinKey;

    public InternalRowAndJoinKey(InternalRow ir, int key) { r = ir; joinKey = key;}
}
