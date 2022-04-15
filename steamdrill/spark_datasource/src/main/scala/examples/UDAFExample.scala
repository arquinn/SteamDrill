package examples


import java.util.Locale

import org.apache.spark.sql.catalyst.dsl.expressions._
import org.apache.spark.sql.{Column, SparkSession, functions}
import org.apache.spark.sql.functions._
import org.apache.spark.sql.expressions.Window
import org.apache.spark.sql.expressions.MutableAggregationBuffer
import org.apache.spark.sql.catalyst.expressions.{AggregateWindowFunction, And, AttributeReference, EqualNullSafe, Expression, If, Literal, Rank, RankLike}
import org.apache.spark.sql.types._


case class OrderedAggregate(incr: Expression, decr: Expression) extends AggregateWindowFunction {
  override val children = incr :: decr :: Nil


  protected val depth: AttributeReference = AttributeReference("depth", IntegerType, nullable = false)()
  protected val zero: Expression = Literal(0)
  protected val one: Expression = Literal(1)

  val adjustDepth = If(incr, depth + one, If (decr, depth - one, depth))

  override val aggBufferAttributes: Seq[AttributeReference] = depth :: Nil
  override val initialValues = zero :: Nil
  override val updateExpressions = adjustDepth :: Nil
  override val evaluateExpression: Expression = depth

  override def sql: String = s"${prettyName.toUpperCase(Locale.ROOT)}()"
}



object UDAFExample {

  def test(incr: Column, decr: Column): Column = new Column(new OrderedAggregate(incr.expr, decr.expr))

  def main(args: Array[String]) {

    System.out.println("*** Example UDAF datasource:")
    val spark: SparkSession = SparkSession.builder().appName("SBasic").getOrCreate()

    val ids = spark.range(1, 20)
    System.out.println("*** Created Temp View")

    val df = ids.select(col("id"),
                        functions.rand(seed=10).alias("uniform"),
                        when(functions.rand(seed=20) > 0.5, 1).otherwise(0).alias("category"))

    val unOrder = Window.orderBy(col("uniform"))

    df.withColumn("uniform_rank", test(col("category") === 0, col("category") === 1).over(unOrder)).show()

    //  agg(gm(col("id")).as("GeometricMean")).show()

    spark.stop()
  }
}