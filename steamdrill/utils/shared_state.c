#define _LARGEFILE64_SOURCE
#define _LARGEFILE_SOURCE
//#define _FILE_OFFSET_BITS=64

#include <assert.h>      // for debugging
#include <errno.h>
#include <fcntl.h>
#include <limits.h>      /// for int-Max
#include <malloc.h>
#include <stdarg.h>      // for va_* functions
#include <string.h>      // for strerror
#include <sys/file.h>    // for the flock stuf
#include <sys/stat.h>    // for fstat
#include <sys/syscall.h> // for syscall nums
#include <sys/time.h>    // for timespec
#include <unistd.h>      // for futex syscalll stuff

#include "shared_state.h"

//#define TRACE(...) fprintf(stderr, __VA_ARGS__)
#define TRACE(x, ...)

// wrapper around futex:
static int futex(uint32_t *uaddr, int futex_op, uint32_t val,
                 const struct timespec *timeout, uint32_t *uaddr2, uint32_t val3) {

  return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

static struct shared_state* shared_create(const char *shared_filename, int creat) {
  struct stat64 stat_buf;
  struct shared_state *shared = (struct shared_state*)  malloc(sizeof(struct shared_state));


  shared->fd = open(shared_filename, (creat ? O_CREAT : 0)| O_RDWR | O_LARGEFILE, 0644);
  if (shared->fd == 0) {
    fprintf(stderr, "shared_create: could not open shared_filename %s", strerror(errno));
    return NULL;
  }

  int rc = flock(shared->fd, LOCK_EX);
  if (rc) {
    fprintf(stderr, "cannot flock");
  }

  rc = fstat64(shared->fd, &stat_buf);
  if (rc) {
    fprintf(stderr, "cannot fstat! %s(%d)", strerror(errno), errno);
  }

  shared->created = stat_buf.st_size == 0 && creat;
  if (shared->created) {
    int rc = ftruncate64(shared->fd, SHARED_START_SIZE);
    if (rc != 0) {
      fprintf(stderr, "shared_create: could not ftruncate error: %s\n", strerror(errno));
      return NULL;
    }

    shared->bound = SHARED_START_SIZE;
    // not sure if this is needed:
    rc = fsync(shared->fd);
    if (rc) {
      fprintf(stderr, "couldn't fsync! %s(%d)\n", strerror(errno), errno);
      return NULL;
    }
  }
  else {
    shared->bound = stat_buf.st_size;
    //    fprintf(stderr, "%p reused %s!\n", shared, shared_filename);
  }

  rc = flock(shared->fd, LOCK_UN);

  // header is a separate single-page MMMAP so
  // that mmapping the iterator is much simpler:
  shared->header = (struct shared_header*) mmap(NULL,
                                                0x1000,
                                                PROT_READ | PROT_WRITE,
                                                MAP_SHARED,
                                                shared->fd,
                                                0);
  if (shared->header == MAP_FAILED) {
    fprintf(stderr, "oh no! mapping header failed %s\n", strerror(errno));
    assert (0);
  }
  if (shared->created) {
    shared->header->size = sizeof(struct shared_header);
    shared->header->count = 0;
  }
  TRACE(stderr, "header, %s, %p, %x\n", shared_filename, shared->header, 0x1000);

  shared->offset = 0;
  shared->data = (char *) mmap64(NULL,
                                 MMAP_SIZE,
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED,
                                 shared->fd,
                                 0);
  if (shared->data == MAP_FAILED) {
    fprintf(stderr, "Cannot map data %s(%d) ", strerror(errno), errno);
    fprintf(stderr, "on fd=%d, size=%x \n", shared->fd, MMAP_SIZE);
    return NULL;
  }

  // this is the actual ending; you update shared->data afterwards (else the endpoint is messed up!)
  shared->dataEnd = shared->data + MMAP_SIZE;

  shared->data += sizeof(struct shared_header);

  return shared;
}

void shared_wait(struct shared_state* shared) {
  do {
    uint32_t cval = __atomic_load_n(&shared->header->futex, __ATOMIC_SEQ_CST);
    if (cval == FIN) {
      break;
    }
    int rc = futex((uint32_t*)&shared->header->futex, FUTEX_WAIT, WAITING, NULL, NULL, 0);
    if (rc && errno != EAGAIN) {
      fprintf(stderr, "futex problem %s(%d)\n", strerror(errno), errno);
    }
  } while (1);
}


void shared_finish(struct shared_state *shared) {
  off64_t size = shared->header->size;
  int rc = ftruncate64(shared->fd, size);
  if (rc) {
    fprintf(stderr, "ftruncate failed! %s fd=%d size=%lld\n", strerror(errno), shared->fd, size);
  }
  uint32_t value = __atomic_exchange_n(&shared->header->futex, FIN, __ATOMIC_SEQ_CST);
  if (value == WAITING) {
    futex((uint32_t*)&shared->header->futex, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
  }
}

struct shared_state *shared_reader(const char *shared_filename) {
  struct shared_state *out = shared_create(shared_filename, 1);
  out->writer = 0; // just to make sure (!)
  return out;
}

struct shared_state *shared_writer(const char *shared_filename) {
  struct shared_state *out = shared_create(shared_filename, 0);
  out->writer = 1;
  return out;
}

void shared_close(struct shared_state *in) {
  close(in->fd);
  munmap(in->dataEnd - MMAP_SIZE, MMAP_SIZE);

  munmap(in->header, 0x1000);
}

char* shared_nextMsg(struct shared_state *in) {
  // always wait, because it's quite complex to handle this otherwise
  shared_wait(in);

  off64_t off = shared_getDataOffset(in);
  if (off == in->header->size) {
    //    SS_INFO("%p: off=%llx, size=%llx, finished!\n", in, off, in->header->size);
    return NULL;
  }

  // otherwise, make sure the full message is in memory:
  char *rtn = in->data;
  while (in->data < in->dataEnd && *in->data) ++in->data;

  if (in->data == in->dataEnd) {
    //    SS_INFO("reload at %llx\n", in->offset);
    shared_mmap_next(in, rtn);
    rtn = in->data;
    // loop again to setup for next time:
    while (in->data < in->dataEnd && *in->data) ++in->data;
  }

  ++in->data; // skip over the null terminator (!)
  SS_DEBUG("%p: msg=%s off=%llx size=%llx\n", in, rtn, off, in->header->size);
  return rtn;
}


void shared_mmap_next(struct shared_state* shared, char* overlap) {
  munmap(shared->dataEnd - MMAP_SIZE, MMAP_SIZE);

  // we want overlap to remain in memory and become the next value of shared->data.
  int map_offset = overlap - (shared->dataEnd - MMAP_SIZE);
  shared->offset += map_offset & PAGE_MASK;
  map_offset = map_offset - (map_offset & PAGE_MASK);

  SS_DEBUG("%p, shared_off=%llx\n", shared, shared->offset);

  // if we're the creator.. (no?)
  if (shared->offset + MMAP_SIZE > shared->bound && shared->writer) {
    // grow the size!
    // double until 1 GB, then add a gig at a time:
    uint64_t new_size = shared->bound >= 0x40000000 ?
        shared->bound + 0x40000000 :
        shared->bound * 2;

    int rc = ftruncate64(shared->fd, new_size);
    if (rc) {
      fprintf(stderr, "ftruncate error on 0x%llx (0x%x)%s\n", new_size, errno, strerror(errno));
    }
    assert (!rc);
    shared->bound = new_size;
  }

  // now we map
  shared->data = (char *) mmap64(NULL,
                                 MMAP_SIZE,
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED,
                                 shared->fd,
                                 shared->offset);

  if (shared->data == MAP_FAILED) {
    SS_ERROR("oh no! mapping at 0x%llx failed (0x%x) %s\n", shared->offset, errno , strerror(errno));
    assert (0);
  }

  shared->dataEnd = shared->data + MMAP_SIZE;
  shared->data += map_offset;
}
