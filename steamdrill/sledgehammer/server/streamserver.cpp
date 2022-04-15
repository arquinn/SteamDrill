// Shell program for running a sequential multi-stage DIFT
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>

#include <iostream>
#include <vector>
#include <atomic>
using namespace std;

#include "../../../test/util.h"
#include "streamserver.h"
#include "config.hpp"

//#define DETAILS

int agg_port = -1;

void doTreeMerge (int s, Config::ServerConfig &c)
{
    printf ("immediately done. lol\n");
    c.sendAck(s);
    c.sendTreeOutput(s);
    close (s);
}


void doCheckpoint (int s, Config::ServerConfig &c)
{
    struct timeval tv_start, tv_done;
    gettimeofday (&tv_start, NULL);

    c.doCkptEpochs();
    c.waitForDone();

    printf ("done with take checkpoints\n");
    gettimeofday (&tv_done, NULL);
    printf ("Dift start time: %ld.%06ld\n", tv_start.tv_sec, tv_start.tv_usec);
    printf ("Dift end time: %ld.%06ld\n", tv_done.tv_sec, tv_done.tv_usec);
    fflush(stdout);
    
    c.sendAck(s);
    c.sendCkptStats(s);
	
    close (s);
}


void doPtrace (int s, Config::ServerConfig &c)
{
    struct timeval tv_start, tv_done;
    gettimeofday (&tv_start, NULL);
    
    c.doAnalyzers();
    c.doEpochs();
    c.waitForDone();

    printf ("done with epochs\n");
    gettimeofday (&tv_done, NULL);
    printf ("Dift start time: %ld.%06ld\n", tv_start.tv_sec, tv_start.tv_usec);
    printf ("Dift end time: %ld.%06ld\n", tv_done.tv_sec, tv_done.tv_usec);

    fflush(stdout);
    
    c.sendAck(s);
    c.sendOutput(s);

    c.sendStats(s);
    c.sendAltrace(s);
	
    close (s);
}

void* doRequest (void* arg) 
{
    long s = (long) arg;

    int dev_fd = open ("/dev/spec0", O_RDWR);
    if (dev_fd < 0) {
	perror("open /dev/spec0");
	return NULL;
    }
    //std::cerr << "new request " << s << std::endl;
    Config::ServerConfig c(dev_fd); 

    c.readConfig(s);        
    //std::cerr << "read request " << s << std::endl;
    //c.dump();

    //sync replay if requested:
    c.recvReplayFiles(s);
    
    if (c.isPtrace())
	doPtrace(s,c);
    else if (c.isCheckpoint())
	doCheckpoint(s,c);
    else if (c.isTreeMerge())
	doTreeMerge(s,c);
    else
	std::cerr << "config is not ptrace?" << std::endl;  

    std::cout << "done with request" << std::endl;
    return NULL;
}

int main (int argc, char* argv[]) {
    int rc;
    if(argc > 1)
	agg_port = atoi(argv[1]);

    // Listen for incoming commands
    int c = socket (AF_INET, SOCK_STREAM, 0);
    if (c < 0) {
	fprintf (stderr, "Cannot create listening socket, errno=%d\n", errno);
	return c;
    }

    int on = 1;
    rc = setsockopt (c, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (rc < 0) {
	fprintf (stderr, "Cannot set socket option, errno=%d\n", errno);
	return rc;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    if(agg_port > 0)
	addr.sin_port = htons(agg_port);
    else 
	addr.sin_port = htons(STREAMSERVER_PORT);

    addr.sin_addr.s_addr = INADDR_ANY;
    rc = bind (c, (struct sockaddr *) &addr, sizeof(addr));
    if (rc < 0) {
	fprintf (stderr, "in streamserver, agg_port %d\n", agg_port);
	fprintf (stderr, "Cannot bind socket, errno=%d\n", errno);
	return rc;
    }

    rc = listen (c, 5);
    if (rc < 0) {
	fprintf (stderr, "Cannot listen on socket, errno=%d\n", errno);
	return rc;
    }

    while (1) {
	long s = accept (c, NULL, NULL);
	if (s < 0) {
	    fprintf (stderr, "Cannot accept connection, errno=%d\n", errno);
	    return s;
	}
        /// Spawn a thread to handle this request
	pthread_t tid;
 	rc = pthread_create (&tid, NULL, doRequest, (void *) s);
	if (rc < 0) {
	    fprintf (stderr, "Cannot spawn stream thread,rc=%d\n", rc);
	}
    }

    return 0;
}
