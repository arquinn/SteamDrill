#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <syscall.h>
#include <assert.h>
#include <fcntl.h>
#include <glib-2.0/glib.h>

#include <iostream>
#include <fstream>
#include <unordered_map> 
#include <unordered_set>
#include <queue> 

#include "../graph_interface/node.h"
#include "../graph_interface/graph_interface.h"
#include "merge_mapper.h"
#include "util.h"

//#define DEBUgGTRACE_OUTPUT 0x47df9c 
#ifdef DEBUGTRACE_OUTPUT 
int debug_me = 0;
#endif

//#define DETAILS 1
using namespace std;

#define CHUNK 0x1000 * sizeof(node_t)
#define SIZE (0x100018000 + CHUNK)
int test_shm()
{
    node_t *buf; 
    int shmfd = shm_open ("/test", O_TRUNC | O_RDWR | O_LARGEFILE | O_CREAT, 0644);
    assert(shmfd > 0);

    cerr << "opening test with " << hex << (O_TRUNC | O_RDWR | O_LARGEFILE | O_CREAT) << " mode "
	 << 0644 << endl;

    cerr << SIZE % sizeof(node_t) << endl;

    int rc = ftruncate64 (shmfd, 0x400000000); 

    cerr << "ftruncate with " << 0x400000000 << endl;
    if (rc < 0) {
	cerr << "ftruncate on " << shmfd << " " << errno << endl;
	assert(0);
    } 

    buf = (node_t *)mmap64(0, CHUNK, PROT_READ | PROT_WRITE, MAP_SHARED, 
				       shmfd, 0);

    cerr << "mmap64 with " << (PROT_READ | PROT_WRITE )<< endl;

    if (buf == MAP_FAILED)
	cerr << "huh? " << errno << endl;

    assert(buf != MAP_FAILED);

    uint64_t counter = 0, i;
    for (i = 0; i < SIZE / sizeof(node_t); ++i) 
    {
	if (counter >= CHUNK / sizeof(node_t)) {
	    assert(munmap(buf, CHUNK) >= 0);
//	    buf = (node_t *)mmap64(0, CHUNK, PROT_READ | PROT_WRITE , MAP_SHARED, 
//				   shmfd, i * sizeof(node_t));

	    off_t page_offset = (i * sizeof(node_t)) >> 12; 

//	    cerr << i * sizeof(node_t) << " my attempt " << page_offset << endl;


	    buf = (node_t *)syscall(SYS_mmap2, 0, CHUNK, PROT_READ | PROT_WRITE , MAP_SHARED, 
				    shmfd, page_offset);


	    if ( buf == MAP_FAILED)
		cerr << "huh? " << errno << endl;
	

	    counter = 0;
	}
	

	buf[counter++].p1 = i;
	if (i == 0x924a924)
	    cerr << "placed " << i << "in 0x924a924\n";

    }
    cerr << "wrote it all out\n"; 
    rc = ftruncate64 (shmfd, SIZE);
    sleep(60);
    close(shmfd);

    shmfd = shm_open ("/test", O_RDONLY | O_LARGEFILE, 0644);

    counter = 0;
    node_t node;
    while(read(shmfd, &node, sizeof(node_t)))
    {
	if (node.p1 != counter) 
	{
	    cerr << "problem? " << node.p1 << " " << counter; 
	}

	assert(node.p1 == counter++);
	if (counter % 0x10000 == 0) cerr << hex << "read in " << counter << endl;
    }

    return 0;
}

int main(int argc, char *argv[]) 
{
    int rc, shmfd, fd;
    u_long shmsize, mergesize, count = 0; 
    node_t shm_node, mergenode; 
    struct stat64 st;
    char filename[256];    

//    test_shm();
//    return 0;

    assert(argc > 1);
    sprintf(filename, "/node_nums_shm.tmp.%s", argv[1]);
       
    shmfd = shm_open (filename, O_RDONLY | O_LARGEFILE, 0644);
    assert(shmfd > 0);
    fd = open("results/ghostscript.slice/node_nums", O_RDONLY | O_LARGEFILE); 
    assert(fd > 0);

    //stat to get sizes
    rc = fstat64(shmfd, &st);
    assert(rc >= 0);
    shmsize = st.st_size; 

    rc = fstat64(fd, &st);
    assert(rc >= 0);
    mergesize = st.st_size; 
    
    cerr << hex << shmsize << "," << mergesize << endl;
    
//    assert(shmsize == mergesize);


//    while (count < (shmsize / sizeof(node_t))) { 
    while (read(shmfd, &shm_node, sizeof(node_t)) == sizeof(node_t) &&
	   read(fd, &mergenode, sizeof(node_t)) == sizeof(node_t))
    {
	count ++;
	if (count % 0x100000 == 0) 
	    cerr << hex << count << endl;

//	cerr << hex << shm_node << endl;
//	cerr << mergenode << endl;

	
	if(shm_node.instcount != mergenode.instcount) 
	{
	    cerr << hex << "problem @ " << count 
		 << " byte " << count * sizeof(node_t) << endl;
	}
	assert(shm_node.instcount == mergenode.instcount); 
	
    }  
}
