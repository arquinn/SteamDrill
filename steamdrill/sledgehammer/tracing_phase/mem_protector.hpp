#include <sys/mman.h>
#include <sys/user.h>
#include <unistd.h>
#include <stdlib.h>


#include <algorithm>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>


#ifndef __MMAP_PROTECTOR_H_
#define __MMAP_PROTECTOR_H_

using std::vector;

namespace ThreadGroup {

#define UNWRITE(x) (x & (PROT_EXEC | PROT_READ))
#define WRITABLE(x) (x & PROT_WRITE)
#define GET_PAGE(addr) addr & PAGE_MASK

//forward decl
class Task;


// this is really a struct for tabulating changes to pages:
class PageState {
public:
  u_long page;
  int oprot; // original protections
  int sprot; // set protections
  PageState(u_long p, int op, int sp): page(p), oprot(op), sprot(sp) {};

  friend std::ostream& operator<<(std::ostream &os, const PageState &p) {
    os << p.page << "(oprot=" << p.oprot << ", sprot=" << p.sprot << ")";
    return os;
  }
  std::string debugString() {
    std::stringstream ss;
    ss << *this;
    return ss.str();
  }

};

class MemProtector
{
 private:
  std::unordered_map<u_long, PageState> _pages;

 public:
  MemProtector() {};

  void protectAll(Task *t);
  bool protectOne(Task *t, long addr);

  void unprotectAll(Task *t);
  bool unprotectOne(Task *t, long addr);

  void updateRange(Task *t, u_long addr, u_long len, int prot);

  bool inline inSet(u_long addr)  {
    auto it = _pages.find(GET_PAGE(addr));
    return it != _pages.end();
  }

  bool inline protecting(u_long addr)  {
    auto it = _pages.find(GET_PAGE(addr));
    return it == _pages.end() ? false : WRITABLE(it->second.oprot) && !WRITABLE(it->second.sprot);
  }

  std::string debugString() {
    std::stringstream ss;
    ss << *this;
    return ss.str();
  }
  friend std::ostream& operator<<(std::ostream &os, const MemProtector &mp) {
    vector<u_long> storedList;
    for (auto &pr : mp._pages) {
      storedList.push_back(pr.first);
    }

    std::sort (storedList.begin(), storedList.end());
    os << std::hex;
    for (auto pr : storedList) {
      auto it = mp._pages.find(pr);
      os << it->second << "; ";
    }
    return os;
  }

};
} /*namespace ThreadGroup*/
#endif


