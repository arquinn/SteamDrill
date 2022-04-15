#include <sys/types.h>
//#include <sys/reg.h>
//#include <sys/user.h>
#include <string>
#include <iostream>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#ifndef __ADDRESS_H_
#define __ADDRESS_H_

namespace Configuration {
class Address {
 private:
  friend class boost::serialization::access;
  std::string _library;
  std::string _flags;
  u_long      _offset;
  pid_t       _pid;

 public:
  Address(std::string lib, std::string flags, u_long off, pid_t pid)
  {
    _library = lib;
    _flags = flags;
    _offset = off;
    _pid = pid;
  };
  Address(u_long off): _library(""), _flags(""), _offset(off), _pid(0) {};

  Address(): _library(""), _flags(""), _offset(0), _pid(0)  {};

  // getters
  inline std::string getLibrary(void) {return _library;}
  inline std::string getFlags(void)   {return _flags;}
  inline u_long      getOffset(void)  {return _offset;}
  inline pid_t       getPid(void)  {return _pid;}

  // setters
  inline void setLibrary(std::string l) {_library = l;}
  inline void setFlags(std::string f)   {_flags = f;}
  inline void setOffset(u_long o)  {_offset = o;}
  inline void setPid(pid_t p)  {_pid = p;}

  inline bool sameRegion(Address &other) {
    return _library == other._library && _flags == other._flags;
  }

  bool operator<(Address &other) {
    assert (sameRegion(other));
    return _offset < other._offset;
  }

  bool operator<=(Address &other) {
    assert (sameRegion(other));
    return _offset <= other._offset;
  }

  bool operator>(Address &other) {
    assert (sameRegion(other));
    return _offset > other._offset;
  }

  bool operator>=(Address &other) {
    assert (sameRegion(other));
    return _offset >= other._offset;
  }

  bool operator==(Address &other) {
    return sameRegion(other) && _offset == other._offset;
  }

  std::string toString() {
    std::stringstream ss;
    ss << *this;
    return ss.str();
  }

  // serialization fxns
  friend std::ostream &operator<<(std::ostream &os, const Address &a) {
    os << a._pid << ": " << a._library << ", " << a._flags << ", " << std::hex << a._offset;
    return os;
  };


  template<class Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar & _pid;
    ar & _library;
    ar & _flags;
    ar & _offset;
  }

}; /* end Class Address*/

// Address* parseAddress(std::string input);

} /* end namespace Configuration*/
#endif /* __ADDRESS_H_*/
