package datasources;

import org.apache.log4j.Logger;

import java.io.*;
import java.util.Iterator;
import java.util.NoSuchElementException;

public class LineIterator implements Iterator<String>, Closeable {
    static Logger log = Logger.getLogger(LineIterator.class.getName());

    private BufferedReader reader;
    private String nextLine;
    private int count;
    public LineIterator(InputStream is) {
        reader = new BufferedReader(new InputStreamReader(is));
    }

    @Override
    public boolean hasNext() {
        if (nextLine != null) {
            return true;
        }
        try {
            nextLine = reader.readLine();
            return nextLine != null;
        }
        catch (IOException ioe) {
            log.error("Problem reading stream: " + ioe.toString());
        }
        return false;
    }

    @Override
    public String next() {
        if (!hasNext()) {
            throw new NoSuchElementException();
        }
        String result = nextLine;
        nextLine = null;
        return result;
    }

    @Override
    public void close() throws IOException {
        reader.close(); //not 100% sure how this works?
    }
}
