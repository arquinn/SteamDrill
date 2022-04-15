#include <string>
#include <vector>

#ifndef __EXPRESSION_H_
#define __EXPRESSION_H_
namespace Query{

// just mocked up.. not sure how to support these yet?
class Operator
{
};

// A generic expression recurssively defined one or more expressions
class Expression
{
 public:
  Operator *function;
  std::vector<Expression *> operands;
  std::string name;

  Expression()
  {
    function = NULL;
    name = "";
  };

  Expression(std::string n)
  {
    function = NULL;
    name = n;
  }

  virtual ~Expression() = default;

  friend std::ostream& operator<<
  (std::ostream &os, const Query::Expression &e);
};

class BuiltinExpression : public Expression
{
 public:
  enum {REGISTER, MEMORY, COUNT, REPLAY_CLOCK} type;
  union
  {
    int registerOffset;   // register offset from sys/reg.h
    int memoryLocation;
  };

  static BuiltinExpression* getRegister(int reg)
  {
    BuiltinExpression *rtn = new BuiltinExpression();
    rtn->type = REGISTER;
    rtn->registerOffset = reg;
    return rtn;
  }

  static BuiltinExpression* getMemory(int mem)
  {
    BuiltinExpression *rtn = new BuiltinExpression();
    rtn->type = MEMORY;
    rtn->registerOffset = mem;
    return rtn;
  }
};

enum Comparison {LESS, GREATER, LEQ, GEQ, EQ};

// A generic expression recurssively defined one or more expressions
class ComparisonExpression
{
 protected:
  Expression *_operand1;
  Expression *_operand2;
  Comparison _comp;
  std::string _name;
 public:
  ComparisonExpression() {
    _operand1 = _operand2 = NULL;
    _name = "";
  };

  ComparisonExpression(Expression *o,
                       Expression *t,
                       Comparison op,
                       std::string name) {
    _operand1 = o;
    _operand2 = t;
    _comp = op;
    _name = name;
  }

  friend std::ostream& operator<<
  (std::ostream &os, const Query::ComparisonExpression &e);
};
}
#endif /* EXPRESION_H_ */
