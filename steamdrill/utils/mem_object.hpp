#include <algorithm>
#include <string>
#include <vector>


extern "C" {
#include <libgen.h>
#include <dirent.h>
#include "../../test/parse_syscall_input_lib.h"
}


#ifndef MEM_OBJECT
#define MEM_OBJECT

using namespace std;

class MemObject {
 public:
  uintptr_t start, end, prot, offset;
  std::string name, oname;
  MemObject(uintptr_t s, uintptr_t e, uintptr_t p, uintptr_t off, char *n, char *o):
      start(s), end(e), prot(p), offset(off), name(n), oname(o) {}

  // equality:
  bool operator==(const MemObject &other) const {
    return start == other.start && end == other.end && prot == other.prot &&
        !name.compare(other.name); // ignoring oname
  }
};

// why doesn't the using namesace std take care of the namespace block?
namespace std {
template <> struct hash<MemObject> {
  std::size_t operator()(const MemObject &mo) const {
    return mo.start ^ mo.end ^ mo.prot;
  }
};
}


inline void getMappings(string replay, function<void(MemObject&&)> func) {
    struct dirent *ent;
    DIR *dir = opendir(replay.c_str());
    if (!dir) {
        assert (false);
    }

    while((ent = readdir(dir))) {
        if (!strncmp(ent->d_name, "sysin", 5)) {
            char filename[PATH_MAX];
            struct mmap_input *mm;
            int rc;

            sprintf(filename, "%s/%s", replay.c_str(), ent->d_name);
            auto *log = syscall_input_open(filename);
            assert (log);
            while ((rc = syscall_input_next(log, &mm)) > 0) {
              char cachename[CACHENAME_LEN + 1], origname[PATH_MAX + 1];
              get_cache_filename(cachename,  mm->dev, mm->ino, mm->mtime);
              get_orig_filename(cachename, origname);
              func(MemObject(mm->start, mm->start + mm->len, mm->prot, mm->offset,
                             cachename, origname));
            }
            syscall_input_close(log);
        }
    }
    closedir(dir);
}


#endif
