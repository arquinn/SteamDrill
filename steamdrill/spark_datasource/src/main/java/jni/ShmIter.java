package jni;

import org.apache.log4j.Logger;
import org.apache.spark.sql.types.DataType;

import java.io.*;
import java.util.Iterator;
import java.util.NoSuchElementException;

public class ShmIter implements Iterator<Object[]>, Closeable {
  private Object[] next;
  private String filename;
  private String[] types;
  private long ptr;  // used within the JNI code to store the open shared_state
  private long count;  // used within the JNI code to store the open shared_state
  private int joinBytes;
  private int joinRowsOffset;


  public ShmIter(String name, String[] ts, int joining, int joinRowsOff) {
    System.loadLibrary("shm_iter");
    next = null;
    types = ts;
    initialize(name);
    filename = name;
    joinBytes = joining;
    joinRowsOffset = joinRowsOff;
  }
  public int getTypesSize() {
    int size = 0;
    for (String t : types) {
      if (t.startsWith("i") ||
          t.startsWith("l")) {
        size += 4;
      }
      else if (t.startsWith("sh")) {
        size += 4;
      }
      else {
        System.out.println("not sure what to do with this" + t);
      }
    }
    size += joinBytes;
    return size;
  }

  public int getJoinBytes() {
    return joinBytes;
  }

  public int getJoinRowsOffset() {
    return joinRowsOffset;
  }

    @Override
    public boolean hasNext() {
        next = nativeNext();
        return next != null;
    }

    @Override
    public Object[] next() {
        if (next == null) {
            throw new NoSuchElementException();
        }
        return next;
    }

    @Override
    public void close() {
        closeJNI(filename);
    }

    public native boolean getCreated();
    public native int getFd();
    private native void initialize(String filename);
    private native Object[] nativeNext();
    private native void closeJNI(String filename);
}
