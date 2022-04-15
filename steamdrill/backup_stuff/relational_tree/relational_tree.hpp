#include <vector>
#include <string>
#include <iostream>
#include <sstream>

#include "expression.hpp"

#ifndef __RELATIONAL_TREE_H_
#define __RELATIONAL_TREE_H_
namespace Query{

// represents a generic node in a relational tree
class RelationNode
{
 protected:
  std::vector<RelationNode> _children;
  RelationNode *_parent;
  std::string _name;

 public:
  RelationNode() { _parent = NULL;}
  virtual ~RelationNode() = default;

  virtual void addChild(RelationNode &child) {_children.push_back(child);}
  virtual void addParent(RelationNode *parent) {_parent = parent;}
  virtual RelationNode *getParent() { return _parent;}
};


class RelationTree
{
 protected:
  RelationNode *root;
};

class SelectionNode : public RelationNode
{
 protected:
  std::vector<ComparisonExpression> _criteria;
 public:
  void addCriteria(ComparisonExpression &e) {_criteria.push_back(e);};

  friend std::ostream& operator<<
  (std::ostream &os, const Query::SelectionNode &s);

};

class ProjectionNode: public RelationNode
{
 public:
  std::vector<Expression *> expressions;
  void addField(Expression *e) {expressions.push_back(e);};

  friend std::ostream& operator<<
  (std::ostream &os, const Query::ProjectionNode &p);
};


class StaticTable : public RelationNode
{
 protected:
  std::string _program;
  std::vector<std::string> _inputs;

 public:
  StaticTable(std::string program) { _program = program;}
  void addInput(std::string str) {_inputs.push_back(str);}

  friend std::ostream& operator<<
  (std::ostream &os, const Query::StaticTable &st);
};

class OmniTable : public RelationNode
{
  friend std::ostream& operator<<
  (std::ostream &os, const Query::OmniTable &o);
};


//------------------//
// Helper Functions //
//------------------//

template<class T> std::ostream&
printHelper(std::ostream &os,
            const std::vector<T> vec,
            std::string pad)
{
  std::string p = "";
  for (auto i : vec)
  {
    os << p << i;
    p = pad;
  }
  return os;
}
}
#endif /*__EXPRESSION_H_*/
