#include <cassert>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include "globals.hpp"
#include "mem_protector.hpp"
#include "task.hpp"
#include "util.h"

namespace ThreadGroup {

int inline static protectHelper(int dev_fd, pid_t pid,
                                u_long addr, u_long size, u_long prot) {
  return  mprotect_other(dev_fd, addr, size,  prot, pid);
}


//void addOrUpdate(u_long start, u_long end, int prot);


void MemProtector::protectAll(Task *t) {
  auto g = Globals::Globals::getInstance();
  for (auto &page : _pages) {
    if (WRITABLE(page.second.sprot)) {
      int rc = protectHelper(g->getDevFd(), t->getPid(),
                             page.second.page, PAGE_SIZE,
                             UNWRITE(page.second.oprot));
      if (!rc) {
        page.second.sprot = UNWRITE(page.second.oprot);
      }
      else {
        ERROR("cannot protect page? {}", page.second.debugString());
      }
    }
  }
}

bool MemProtector::protectOne(Task *t, long addr) {
  auto g = Globals::Globals::getInstance();
  auto page = _pages.find(GET_PAGE(addr));
  if (page == _pages.end()) {
    //need to add a page

    u_long prot = 0, pa = GET_PAGE(addr);
    t->findAllRegions([&prot, pa] (MemRegion && mr) {
        if (mr.getStart() <= pa && pa < mr.getEnd()) {
          prot = mr.getProt();
          return true;
        }
        return false;
      });
    NDEBUG("adding a {:x},{:d} ", GET_PAGE(addr), prot);
    page = _pages.emplace(pa, PageState(pa, prot, prot)).first;
  }
  if (WRITABLE(page->second.sprot)) {
    int rc = protectHelper(g->getDevFd(), t->getPid(),
                           page->second.page, PAGE_SIZE,
                           UNWRITE(page->second.oprot));
    if (!rc) {
      page->second.sprot = UNWRITE(page->second.oprot);
    }
    else {
        ERROR("cannot protect page? {}", page->second.debugString());
    }
  }
  return true; // I don't know what to protect here...?
}


void MemProtector::unprotectAll(Task *t) {
  auto g = Globals::Globals::getInstance();
  for (auto &page : _pages) {
    if (page.second.sprot != page.second.oprot) {
      int rc = protectHelper(g->getDevFd(), t->getPid(),
                             page.second.page, PAGE_SIZE, page.second.oprot);
      if (!rc) {
        page.second.sprot = page.second.oprot;
      }
      else
        ERROR("cannot protect page? {}", page.second.debugString());
    }
  }
}

bool MemProtector::unprotectOne(Task *t, long addr) {
  auto g = Globals::Globals::getInstance();
  auto page = _pages.find(GET_PAGE(addr));
  if (page != _pages.end()) {
    TRACE("unprotect {} to {}", page->first, page->second.oprot);
    int rc = protectHelper(g->getDevFd(), t->getPid(),
                           page->second.page, PAGE_SIZE, page->second.oprot);
    if (!rc) {
      page->second.sprot = page->second.oprot;
    }
    else {
      ERROR("cannot unprotect page? {}", page->second.debugString());
    }
  }
  return true; // maybe wrong??
}


void MemProtector::updateRange(Task *t, u_long start, u_long len, int prot) {
  // TODO: which one is it faster to iterate through?
  for (auto &page : _pages) {
    if (start <= page.second.page && page.second.page < start + len) {
      NDEBUG("updating protection for {:x} {}", page.second.page, prot);
      page.second.sprot = page.second.oprot = prot;
      protectOne(t, page.second.page);
    }
  }
}
}
