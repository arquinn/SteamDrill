#include <iostream>
#include <fstream>
#include "relational_tree.hpp"
#include "expression.hpp"

using Query::operator<<;


int main()
{
    Query::Expression leaf1("foo");
    Query::Expression leaf2("bar");
    Query::ComparisonExpression greater(&leaf1, &leaf2, Query::GREATER,
                                        "");
    Query::SelectionNode s;
    s.addCriteria(greater);

    Query::ProjectionNode p;
    p.addField(leaf1);
    p.addField(leaf2);

    Query::OmniTable dTable;

    p.addChild(dTable);
    s.addChild(p);

    std::cout << s << std::endl;
}
