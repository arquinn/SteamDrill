
//#include <nlohmann/json.hpp>
#include <iostream>

#include "type.hpp"

using std::string;
namespace Parser {

//using nlohmann::json;
std::string BaseType::scalaJson() {
  //  json obj;
  switch (type) {
    case BuiltinType::BOOL:
      return "boolean";
    case BuiltinType::INT:
      if (size > 32)
        return "long";
      return "integer";
    case BuiltinType::CHAR:
    case BuiltinType::UCHAR:
      // just treat these both as unsigned... *sighs all around*
      return "byte";
    case BuiltinType::UINT:
      if (size >= 32)
        return "long"; // this is how we get enough size for unsigned ints ugh.
      return "integer";
    case BuiltinType::FLOAT:
      if (size > 32)
        return "double";
      else
        return "float";
    default:
      return Type::scalaJson();
  }
}

Type* BaseType::clone() {
  BaseType *bt = new BaseType();
  bt->name = name;
  bt->size = size;
  bt->type = type;
  return bt;
}

string BaseType::definition() {
  switch(type) {
    case BuiltinType::BOOL:
      return "bool";
    case BuiltinType::INT:
      if (size > 32)
        return "int64_t";
      return "int";
    case BuiltinType::CHAR:
      return "char";
    case BuiltinType::UCHAR:
      // just treat these both as unsigned... *sighs all around*
      return "u_char";
    case BuiltinType::UINT:
      if (size >= 32)
        return "uint64_t"; // this is how we get enough size for unsigned ints ugh.
      return "u_int";
    case BuiltinType::FLOAT:
      if (size > 32)
        return "double";
      else
        return "float";
    default:
      return Type::scalaJson();
  }
}

std::string PointerType::scalaJson() {
  // just want a 4 byte interger
  return "integer";
}

Type* PointerType::clone() {
  PointerType *t = new PointerType();
  t->name = name;
  t->size = size;

  if (ptype)
    t->ptype = unique_ptr<Type>(ptype->clone());

  return t;
}

string PointerType::definition() {
  return name + "*";
}



std::string ArrayType::scalaJson() {
  // just want a 4 byte interger
  return "integer";
}

Type* ArrayType::clone() {
  ArrayType *t = new ArrayType();
  t->name = name;
  t->size = size;

  t->ptype = unique_ptr<Type>(ptype->clone());
  return t;
}

string ArrayType::definition() {
  return name + "[]";
}


std::string EnumerationType::scalaJson() {
  return "string";
}

Type* EnumerationType::clone() {
  EnumerationType *t = new EnumerationType();
  t->name = name;
  t->size = size;

  t->types.resize(types.size());
  std::copy(types.begin(), types.end(), t->types.begin());
  return t;
}

std::string EnumerationType::definition() {
  stringstream ss;

  ss << "enum " << name << "{";
  for (auto t : types) {
    ss << t.first << ":" << t.second <<", ";
  }
  ss <<"};";
  return ss.str();
}

std::string StructType::scalaJson() {
  stringstream ss;

  ss << "struct " << name << "{";
  for (auto t : fields) {
    // need to know that the offset works!
    /*    if (t.offset) {
      return "unsupported struct1";
    }
    */
    ss << t.type->definition() << " " << t.name <<";\n";
  }
  ss <<"};";
  return ss.str();

}

std::string StructType::definition() {
  return "unsupported def";
}

Type* StructType::clone() {
  StructType *t = new StructType();
  t->name = name;
  t->size = size;

  t->fields.resize(fields.size());
  std::copy(fields.begin(), fields.end(), t->fields.begin());
  return t;
}

std::string UnionType::scalaJson() {
  return "unsupported union";
}

std::string UnionType::definition() {
  stringstream ss;

  ss << "union " << name << "{";
  for (auto t : types) {
    if (t.offset) {
      return "unsupported union";
    }
    ss << t.type->definition() << " " << t.name <<";\n";
  }
  ss <<"};";
  return ss.str();

  //  return "unsupported def";
}

Type* UnionType::clone() {
  UnionType *t = new UnionType();
  t->name = name;
  t->size = size;

  t->types.resize(types.size());
  std::copy(types.begin(), types.end(), t->types.begin());
  return t;
}


StructField::StructField(StructField &other) {
  name = other.name;
  offset = other.offset;
  type = unique_ptr<Type>(other.type->clone());
}

StructField::StructField(StructField &&other) {
  name = other.name;
  offset = other.offset;
  type = unique_ptr<Type>(other.type->clone());
}


StructField StructField::operator=(const StructField &other) {
  name = other.name;
  offset = other.offset;
  type = unique_ptr<Type>(other.type->clone());
  return *this;
}

std::string Type::scalaJson() {
  return "this... uh doesn't *really* work actuall";
}

std::string Type::definition() {
  return "this... uh doesn't *really* work actuall";
}

Type *Type::clone() {
  Type *t = new Type();
  t->name = name;
  t->size = size;
  return t;
}
}; // namespace Parser
