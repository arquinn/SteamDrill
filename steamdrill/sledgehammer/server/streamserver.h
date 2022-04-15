
#ifndef __STREAMSERVER_H__
#define __STREAMSERVER_H__

//ports
#define STREAMSERVER_PORT 19764

//constant sizes
#define BUFFER_SIZE 1024
#define FILENAME_LEN 256


struct replay_path {
    char path[FILENAME_LEN];
};

struct cache_info {
    uint32_t        dev;
    uint32_t        ino;
    struct timespec mtime;
};

#endif
