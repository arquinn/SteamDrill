#include <sstream>
#include <iostream>
#include <iterator>

#include <assert.h>

//boost stuffs
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>


// My libraries
#include "relational_tree.hpp"
#include "expression.hpp"


/////////////////
// Output Fxns //
/////////////////

std::ostream& Query::operator<<(std::ostream &os,
				const Query::SelectionNode &s)
{
  os << "sigma(";
  printHelper<Query::ComparisonExpression>(os,
                                           s._criteria,
                                           " and ");
  return os;
}

std::ostream& Query::operator<<(std::ostream &os,
				const Query::ProjectionNode &p)
{
  os << "pi ";
  return printHelper<Query::Expression>(os, p._expressions, ", ");
}


std::ostream& Query::operator<<(std::ostream &os,
                         const Query::StaticTable &st)
{
  os << st._program << "(";
  printHelper<std::string>(os, st._inputs, ",");
  os <<")";
  return os;
}

std::ostream& Query::operator<<(std::ostream &os,
                                const Query::OmniTable &ot)
{
  return os << "OMNITABLE";
}
