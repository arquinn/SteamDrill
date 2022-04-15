/*
 * shared_data.h
 *  author: ARQ
 *
 * Supports for sharing data across a shared memory mapping.
 * - Uses futex and atomics to implement synchronization
 * - Requires developer provides a filename to shared memory
 * - n.b., the sender termiates and then releases data
 *
 * - Potential support for other shared_state management:
 *   -- releasing data incrementally?
 *
 */
#ifndef __SHARED_STATE_H
#define __SHARED_STATE_H

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#include <assert.h>      // for debugging
#include <errno.h>       // for debugging (errno)
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>       // for debugging (fprintf)
#include <string.h>      // for strerror
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/user.h>



//Musl compiles this which doesn't  work well with <linux> includes, so
//we must define these rather than getting them from futex.h
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

//#define PAGE_MASK (~(PAGE_SIZE - 1))

// #define SS_DEBUG(...) fprintf(stderr, __VA_ARGS__);
#define SS_DEBUG(x,...)
#define SS_INFO(...) fprintf(stderr, __VA_ARGS__);
//#define SS_INFO(x,...)
#define SS_ERROR(...) fprintf(stderr, __VA_ARGS__);

// this seems like it's too small, in the sense that shrinking this to one MB probably shouldn't
// be what's causing my failures I should think.
#define MMAP_SIZE         0x100000
#define SHARED_START_SIZE 0x10000000
#define MAX_FMTSTR_SIZE 1024

#define CHECK_SIZE(in, size) if (in->dataEnd - in->data < (long)size)   \
    shared_mmap_next(in, in->data);                                     \

#define UPDATE_SIZE(in, update) in->data += update;\
  in->header->size += update;

enum sf_state {
  WAITING,
  //   READY, we don't use this (for now)
  FIN,
};

struct shared_header {
  // header stuff
  enum sf_state futex;
  uint64_t size;
  uint32_t count;
};

struct shared_state {
  // header stuff
  int fd;
  int created;

  // data stuff
  struct shared_header* header;

  uint64_t offset;
  uint64_t bound;
  char writer; // true if we're the writer;
  char *data, *dataEnd;
  char *modStart;
};

// assertion -- reader goes before writer!
void shared_close(struct shared_state *in);

struct shared_state* shared_reader(const char *shared_filename);
struct shared_state* shared_writer(const char *shared_filename);
void shared_mmap_next(struct shared_state* shared, char* overlap);

static inline void shared_startMods(struct shared_state *shared) {
  shared->modStart = shared->data;
}

static inline void shared_syncMods(struct shared_state *shared) {
  msync(shared->modStart, sizeof(shared->data - shared->modStart), MS_SYNC);

  // we just added a row!
  shared->header->count ++;
  msync(shared->header, sizeof(shared->header), MS_SYNC);
}


static inline off64_t shared_getDataOffset(struct shared_state *shared) {
  // returns how far through the file the data iterator is:
  return shared->data - (shared->dataEnd - MMAP_SIZE) + shared->offset;
}

static inline int shared_isFinished(struct shared_state *in, uint32_t rowCount) {
  if (shared_getDataOffset(in) > (off64_t) in->header->size) {
    SS_INFO("shared_isFinished %p dataOffset %llx %llx\n",
             in, shared_getDataOffset(in), (off64_t)in->header->size);
  }
  assert (shared_getDataOffset(in) <= (off64_t)in->header->size);
  return shared_getDataOffset(in) == (off64_t)in->header->size ||
      in->header->count == rowCount;
}

static inline void shared_addInt(struct shared_state *in, int data) {
  CHECK_SIZE(in, sizeof(int));
  *((int*)in->data) = data;
  UPDATE_SIZE(in, sizeof(int));
}


static inline int shared_getInt(struct shared_state *in) {
  CHECK_SIZE(in, sizeof(int));
  int *rtn = (int *)in->data;
  in->data += sizeof(int);
  return *rtn;
}

static inline void shared_addLongLong(struct shared_state *in, long long data) {
  CHECK_SIZE(in, sizeof(long long));
  *((long long*)in->data) = data;
  UPDATE_SIZE(in, sizeof(long long));
}

static inline int shared_getLongLong(struct shared_state *in) {
  CHECK_SIZE(in, sizeof(long long));
  long long *rtn = (long long *)in->data;
  in->data += sizeof(long long);
  return *rtn;
}

static inline void shared_addShort(struct shared_state *in, short data) {
  CHECK_SIZE(in, sizeof(short));
  *((short*)in->data) = data;
  UPDATE_SIZE(in, sizeof(short));
}

static inline int shared_getShort(struct shared_state *in) {
  CHECK_SIZE(in, sizeof(short));
  short *rtn = (short *)in->data;
  in->data += sizeof(short);
  return *rtn;
}

static inline void shared_addByte(struct shared_state *in, char data) {
  CHECK_SIZE(in, sizeof(char));
  *((char*)in->data) = data;
  UPDATE_SIZE(in, sizeof(char));
}

static inline int shared_getByte(struct shared_state *in) {
  CHECK_SIZE(in, sizeof(char));
  char *rtn = (char *)in->data;
  in->data += sizeof(char);
  return *rtn;
}

static inline void shared_addFmtString(struct shared_state *in, const char *fmt, va_list ap) {
  CHECK_SIZE(in, MAX_FMTSTR_SIZE + sizeof(short));
  //DEBUG("shared_addMsg state=%p off=%llx",  in, shared_getDataOffset(in));
  // we serialize with the size first, but we don't know the size until after!
  int size = vsnprintf((in->data + sizeof(short)), MAX_FMTSTR_SIZE, fmt, ap);
  if (size < 0 ||size >= MAX_FMTSTR_SIZE) {
    fprintf(stderr, "\n%p: vsnprintf problem? %d\n", in, size);
    assert (0);
  }

  *((short *)in->data) = (short)size;
  UPDATE_SIZE(in, sizeof(short) + size + 1);

  //DEBUG(" size=%llx\n", in->header->size);
}

static inline char* shared_getString(struct shared_state *in) {
  CHECK_SIZE(in, sizeof(short));
  short size = *(short *)in->data;
  in->data += sizeof(short);

  CHECK_SIZE(in, size + 1); //plus one for the null ptr.. did I get this wrong!?
  char *rtn = in->data;
  in->data += (size + 1);

  return rtn;
}


// currently implement as one-and-done.
void shared_finish(struct shared_state *in);
void shared_wait(struct shared_state* shared);

#endif /* __SHARED_STATE_H*/
