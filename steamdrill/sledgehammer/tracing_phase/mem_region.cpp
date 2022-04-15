#include <cassert>

#include "mem_region.hpp"
#include "task.hpp"

#if 0
namespace ThreadGroup {


#define UNWRITABLE(x) x & (PROT_READ | PROT_EXEC)
void MemTracker::unprotect(MemStateMap::iterator ps, const Task *t) {
  t->changeProt(ps->second.getStart(),
                ps->second.getLen(),
                ps->second.getProt());
  ps->second._status = MemTracker::Status::CLEARED;
}

void MemTracker::unprotectPage(const u_long page, const Task *t) {
  auto ps = _memStates.find(page);
  assert (ps != _memStates.end());
  unprotect(ps, t);

}

void MemTracker::protect(MemStateMap::iterator ps, const Task *t) {
  t->changeProt(ps->second.getStart(),
                ps->second.getLen(),
                UNWRITABLE(ps->second.getProt()));
  ps->second._status = MemTracker::Status::PROTECTED;
}

void MemTracker::protectPage(const u_long page, const Task *t) {
  auto ps = _memStates.find(page);
  assert (ps != _memStates.end());
  protect(ps, t);
}



void MemTracker::updateTracking(const u_long start,
                                const u_long end,
                                const u_long prot) {
  auto ps = _memStates.insert({start, Tracker(start, end, prot)});

  // update status if we need to
  if (ps.second)
    ps.first->second._status = MemTracker::Status::UN_INIT;
  else
  {
    assert (ps.first->second.getEnd() == end);
    ps.first->second.setProt(prot);
  }
}

void MemTracker::protectAll(const Task *t) {
  for (MemStateMap::iterator region = _memStates.begin();
       region != _memStates.end();
       ++region)
  {
    protect(region, t);
  }
}

void MemTracker::unprotectAll(const Task *t)
{
  for (MemStateMap::iterator region = _memStates.begin();
       region != _memStates.end();
       ++region)
  {
    unprotect(region, t);
  }
}

} /* namespace ThreadGroup */
#endif
