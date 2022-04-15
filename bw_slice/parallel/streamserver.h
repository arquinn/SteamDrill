
#ifndef __STREAMSERVER_H__
#define __STREAMSERVER_H__

#include <semaphore.h>

#define STREAMSERVER_PORT 19764
#define AGG_BASE_PORT     10000 //we're going to need a way to have multples of these...? 

#define SEND_ACK      0x1
#define SEND_RESULTS  0x2
#define SEND_STATS    0x8

//Filters
#define FILTER_OUT  0x04

#define NAMELEN 256
#define PATHLEN 512

struct replay_path {
    char path[PATHLEN];
};

struct cache_info {
    uint32_t        dev;
    uint32_t        ino;
    struct timespec mtime;
};


// Possible commands
#define DO_DIFT          0
#define DO_SLICE         1


// Info from description file
struct epoch_hdr {
    uint32_t epochs;
    bool     start_flag;
    bool     finish_flag;
    u_char   parallelize;
    u_char   flags;
    u_char   cmd_type;
    u_char   filter_flags;
    char     filter_part[NAMELEN];
    char     filter_output_after[NAMELEN];
    char     dirname[NAMELEN];
    char     prev_host[NAMELEN];
    char     next_host[NAMELEN];
};


struct epoch_data {
    pid_t    start_pid;
    char     start_level;
    uint32_t start_clock;
    char     stop_level;
    uint32_t stop_clock;
    uint32_t ckpt;
    uint32_t slice_ip;  //these are really epoch_hdr stuff (probably..?) 
    uint32_t slice_location;
    uint32_t slice_clock;
    uint32_t fork_flags; //definitely needed
    uint32_t port;              // Aggregation port
    char     hostname[NAMELEN]; // Aggregation hostname
};

struct epoch_ack {
    uint32_t retval;
};

#define TAINTQSIZE (256*1024*1024)
#define TAINTENTRIES (TAINTQSIZE/sizeof(uint32_t))
#define TAINTQHDRSIZE (4096)
#define TAINTBUCKETSIZE    (4096)
#define TAINTBUCKETENTRIES (TAINTBUCKETSIZE/sizeof(uint32_t))
#define TAINTBUCKETS       (TAINTENTRIES/TAINTBUCKETENTRIES)

struct taintq_hdr {
    sem_t           epoch_sem;
    pthread_mutex_t lock;
    pthread_cond_t  full;
    pthread_cond_t  empty;
    char            pad1[64];
    ulong           read_index;
    char            pad2[64];
    ulong           write_index;
    char            pad3[64];
};

#endif
