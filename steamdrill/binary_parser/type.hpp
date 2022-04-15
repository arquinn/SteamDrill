#include <memory>
#include <utility>
#include <string>
#include <unordered_map>
#include <vector>

#include "../query_generator/function_code.hpp"

#pragma once


using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::vector;


namespace Parser {
class Type {
 public:
  string name;
  u_long size;
  virtual string scalaJson();
  virtual string definition();
  virtual Type * clone();

  virtual ~Type();
};

enum BuiltinType {BOOL, ADDRES, INT, CHAR, UINT, UCHAR, FLOAT, STRING};
class BaseType : public Type {
 public:
  BaseType(Type &&t): Type(t) {};
  BaseType(Type &t): Type(t) {};
  BaseType() {};

  BuiltinType type;
  string scalaJson();
  string definition();
  Type* clone();
};

class PointerType : public Type {
 public:
  PointerType(Type &&t): Type(t), ptype(nullptr) {};
  PointerType(Type &t): Type(t), ptype(nullptr) {};
  PointerType(): ptype(nullptr) {};

  unique_ptr<Type> ptype;
  string scalaJson();
  string definition();
  Type* clone();
};

class ArrayType : public Type {
 public:
  ArrayType(Type &&t): Type(t) {};
  ArrayType(Type &t): Type(t) {};
  ArrayType() {};

  unique_ptr<Type> ptype;
  string scalaJson();
  string definition();
  Type* clone();
};

class EnumerationType : public Type {
 public:
  EnumerationType(Type &&t): Type(t) {};
  EnumerationType(Type &t): Type(t) {};
  EnumerationType() {};

  vector<std::pair<string, u_long>> types;
  string scalaJson();
  string definition();
  Type* clone();
};


class StructField {
 public:
  string name;
  unique_ptr<Type> type;
  uint64_t offset; //wtf did I do this this way...???

  StructField(string s, Type *t, uint64_t off):
      name(s), type(t), offset(off) {};
  StructField() {};


  StructField(StructField &other);
  StructField(StructField &&other);
  StructField operator=(const StructField &other);
};

class StructType : public Type {
 public:
  StructType(Type &&t): Type(t) {};
  StructType(Type &t): Type(t) {};
  StructType() {};

  vector<StructField> fields;
  string scalaJson();
  string definition();
  Type* clone();
};

// basically just special types of structs
class UnionType : public Type {
 public:
  UnionType(Type &&t): Type(t) {};
  UnionType(Type &t): Type(t) {};
  UnionType() {};

  vector<StructField> types;
  string scalaJson();
  string definition();
  Type* clone();
};
} // namespace Parser
