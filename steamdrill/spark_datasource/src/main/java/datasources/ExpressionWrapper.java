package datasources;

import org.apache.spark.sql.catalyst.expressions.Expression;

public class ExpressionWrapper {
    public Expression expr;
    public ExpressionWrapper(Expression e) {
        expr = e;
    }

    @Override
    public boolean equals(Object obj) {
        assert (obj instanceof ExpressionWrapper);
        return expr.semanticEquals(((ExpressionWrapper)obj).expr);
    }

    @Override
    public int hashCode() {
        return expr.semanticHash();
    }
}
