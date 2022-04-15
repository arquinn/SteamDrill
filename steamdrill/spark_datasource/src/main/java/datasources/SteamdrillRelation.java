package datasources;

import org.apache.spark.sql.catalyst.expressions.Expression;

import java.io.Serializable;
import java.util.List;
import java.util.StringJoiner;

public class SteamdrillRelation implements Serializable {
    List<Expression> columns;
    List<Expression> filters;

    public SteamdrillRelation(List<Expression> c, List<Expression> f) {
        columns = c;
        filters = f;
    }

    @Override
    public String toString() {
        StringBuffer sb = new StringBuffer();
        StringJoiner joiner = new StringJoiner(", ");

        for (Expression c : columns) {
            joiner.add(c.toString());
        }
        sb.append("[");
        sb.append(joiner.toString());
        sb.append("] ");

        joiner = new StringJoiner(", ");

        for (Expression f : filters) {
            joiner.add(f.toString());
        }
        sb.append("filter (");
        sb.append(joiner.toString());
        sb.append(")");
        return sb.toString();
    }
}
