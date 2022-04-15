#include <string>
#include <iostream>

#include "expression.hpp"

std::ostream& Query::operator<<(std::ostream &os,
                                const Query::Expression &e)
{
  if (e._operator != NULL)
    os << "Can't handle operators" << std::endl;
  else
    os << e._name;
  return os;
}

static std::string ComparisonToString(Query::Comparison comp)
{
    switch(comp)
    {
    case Query::LESS:
	return "<";
    case Query::GREATER:
	return ">";
    case Query::LEQ:
	return "<=";
    case Query::GEQ:
	return ">=";
    }
    std::cerr << "whoops! you messed up my binaryOperator!" << std::endl;
    return "";
}


std::ostream& Query::operator<<(std::ostream &os,
                                const Query::ComparisonExpression &e)
{
  os << "(" << *(e._operand1) << " "
     << ComparisonToString(e._comp) << " "
     << *(e._operand2) << ")";
  return os;
}
