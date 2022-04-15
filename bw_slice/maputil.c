#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

#include <iostream>
#include "maputil.h"

int map_file (const char* filename, int* pfd, u_long* pdatasize, u_long* pmapsize, char** pbuf)
{
    struct stat64 st;
    u_long size;
    int fd, rc;
    char* buf;

    fd = open (filename, O_RDONLY | O_LARGEFILE);
    if (fd < 0) {
	fprintf (stderr, "map_file: unable to open %s, rc=%d, errno=%d\n", filename, fd, errno);
	return fd;
    }
    rc = fstat64(fd, &st);
    if (rc < 0) {
	fprintf (stderr, "Unable to stat %s, rc=%d, errno=%d\n", filename, rc, errno);
	return rc;
    }
    if (st.st_size > 0) {
	if (st.st_size%4096) {
	    size = st.st_size + 4096-st.st_size%4096;
	} else {
	    size = st.st_size;
	}
	buf = (char *) mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (buf == MAP_FAILED) {
	    fprintf (stderr, "Cannot map file %s, errno=%d\n", filename, errno);
	    return -1;
	}
	*pmapsize = size;
	*pbuf = buf;
    } else {
	*pmapsize = 0;
	*pbuf = NULL;
    }
    *pfd = fd;
    *pdatasize = st.st_size;

    return 0;
}

void unmap_file (char* buf, int fd, u_long mapsize)
{
    if (buf) {
	munmap (buf, mapsize);
    }
    close (fd);
}



static void
unlink_buffer (const char* prefix, const char* group_directory)

{}
/*    char filename[256];
    snprintf(filename, 256, "/%s_shm%s", prefix, group_directory);
    for (u_int i = 1; i < strlen(filename); i++) {
	if (filename[i] == '/') filename[i] = '.';
    }

    long rc = shm_unlink (filename);
    if (rc < 0) {
	fprintf (stderr, "shm_unlink of %s failed, rc=%ld, errno=%d\n", filename, rc, errno);
    }
}

*/

void*
map_buffer (const char* prefix, const char* group_directory, u_long& datasize, int& fd)
{
    char filename[256];
    snprintf(filename, 256, "/%s_shm%s", prefix, group_directory);

    for (u_int i = 1; i < strlen(filename); i++) {
	if (filename[i] == '/') filename[i] = '.';
    }

    fd = shm_open(filename, O_RDWR, 0644);
    if (fd < 0) {
	fprintf(stderr, "could not open shmem %s, errno %d\n", filename, errno);
	return NULL;
    }

    struct stat64 st;
    int rc = fstat64 (fd, &st);
    if (rc < 0) {
	fprintf (stderr, "Cannot fstat shmem %s, rc=%d, errno=%d\n", filename, rc, errno);
	return NULL;
    }
    datasize = st.st_size;
    if (datasize == 0) return NULL;  // Some inputs may actually have no data

    int mapsize = datasize;
    if (mapsize%4096) mapsize += (4096-mapsize%4096);
    void* ptr = mmap (NULL, mapsize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
	fprintf (stderr, "Cannot map input data for %s, errno=%d\n", filename, errno);
	return NULL;
    }

#ifdef DEBUG
    fprintf (debugfile, "map_buffer: mapsize %d datasize %lu \n",mapsize, datasize);
#endif

    unlink_buffer (prefix, group_directory);
    return ptr;
}

