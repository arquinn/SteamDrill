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

#include <cassert>

#include <vector>
#include <atomic>
#include <iostream>
using namespace std;

#include "streamserver.h"
#include "../../test/util.h"
#include "streamnw.h"

//#define DETAILS

#define STATUS_INIT 0
#define STATUS_STARTING 1
#define STATUS_EXECUTING 2
#define STATUS_STREAM 3
#define STATUS_DONE 4

int agg_port = -1;

struct epoch_ctl {
    int    status;
    char   inputqhname[256];
    char   inputqbname[256];
    pid_t  cpid;
    pid_t  spid;
    pid_t  attach_pid;
    pid_t  waiting_on_rp_group; //ARQUINN -> added

    // For timings
    struct timeval tv_start;
    struct timeval tv_start_dift;
    struct timeval tv_done;
};

int setup_shm(struct epoch_ctl *ectl, u_long epochs, struct epoch_hdr *ehdr) 
{
    int rc; 

    // Set up shared memory regions for queues
    u_long qcnt = epochs+1;  
    if (ehdr->finish_flag) qcnt--;
    for (u_long i = 0; i < qcnt; i++) {
	if (i == 0 && ehdr->start_flag) continue; // No queue needed
	sprintf(ectl[i].inputqhname, "/input_queue_hdr%lu", i);
	int iqfd = shm_open (ectl[i].inputqhname, O_CREAT|O_RDWR|O_TRUNC, 0644);	
	if (iqfd < 0) {
	    fprintf (stderr, "Cannot create input queue header %s,errno=%d\n", ectl[i].inputqhname, errno);
	    return iqfd;
	} 
	rc = ftruncate(iqfd, TAINTQHDRSIZE);
	if (rc < 0) {
	    fprintf (stderr, "Cannot truncate input queue header %s,errno=%d\n", ectl[i].inputqhname, errno);
	    return rc;
	}
	struct taintq_hdr* qh = (struct taintq_hdr *) mmap (NULL, TAINTQHDRSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, iqfd, 0);
	if (qh == MAP_FAILED) {
	    fprintf (stderr, "Cannot map input queue, errno=%d\n", errno);
	    return -1;
	}
	rc = sem_init(&(qh->epoch_sem), 1, 0);
	if (rc < 0) {
	    fprintf (stderr, "sem_init returns %d, errno=%d\n", rc, errno);
	    return rc;
	}

	pthread_mutexattr_t sharedm;
	pthread_mutexattr_init(&sharedm);
	pthread_mutexattr_setpshared(&sharedm, PTHREAD_PROCESS_SHARED);
	rc = pthread_mutex_init (&(qh->lock), &sharedm);
	if (rc < 0) {
	    fprintf (stderr, "pthread_mutex_init returns %d, errno=%d\n", rc, errno);
	    return rc;
	}
	pthread_condattr_t sharedc;
	pthread_condattr_init(&sharedc);
	pthread_condattr_setpshared(&sharedc, PTHREAD_PROCESS_SHARED);
	pthread_condattr_setclock(&sharedc,CLOCK_MONOTONIC);  //added for the timeouts
	rc = pthread_cond_init (&(qh->full), &sharedc);
	if (rc < 0) {
	    fprintf (stderr, "pthread_mutex_init returns %d, errno=%d\n", rc, errno);
	    return rc;
	}
	rc = pthread_cond_init (&(qh->empty), &sharedc);
	if (rc < 0) {
	    fprintf (stderr, "pthread_mutex_init returns %d, errno=%d\n", rc, errno);
	    return rc;
	}

	munmap(qh,TAINTQHDRSIZE);
	close (iqfd);

	sprintf(ectl[i].inputqbname, "/input_queue%lu", i);
	iqfd = shm_open (ectl[i].inputqbname, O_CREAT|O_RDWR|O_TRUNC, 0644);	
	if (iqfd < 0) {
	    fprintf (stderr, "Cannot create input queue %s,errno=%d\n", ectl[i].inputqbname, errno);
	    return iqfd;
	} 
	rc = ftruncate(iqfd, TAINTQSIZE);
	if (rc < 0) {
	    fprintf (stderr, "Cannot truncate input queue %s,errno=%d\n", ectl[i].inputqbname, errno);
	    return rc;
	}
	close (iqfd);
    }
    return 0;
}


int cleanup_shm(struct epoch_ctl *ectl, u_long epochs, struct epoch_hdr *ehdr) 
{
    int rc; 

    // Set up shared memory regions for queues
    u_long qcnt = epochs+1;  
    if (ehdr->finish_flag) qcnt--;

    // Clean up shared memory regions for queues
    for (u_long i = 0; i < qcnt; i++) {
	if (i == 0 && ehdr->start_flag) continue; // No queue needed
	rc = shm_unlink (ectl[i].inputqhname);
	if (rc < 0) {
	    fprintf (stderr, "Cannot unlink input queue %s,errno=%d\n", ectl[i].inputqhname, errno);
	    return rc;
	}
	rc = shm_unlink (ectl[i].inputqbname);
	if (rc < 0) {
	    fprintf (stderr, "Cannot unlink input queue %s,errno=%d\n", ectl[i].inputqbname, errno);
	    return rc;
	}
    }
    return 0;
}


struct request_data {
    struct epoch_hdr *ehdr;
    struct epoch_data *edata;
    struct epoch_ctl *this_ectl; 
    struct epoch_ctl *next_ectl;
   

    char dirname[256];    
    char binary[256]; 
    char dift_file[256]; 
    char starting_as[256]; 
    char ending_as[256]; 

    
    bool as_flag; 
    bool finish_flag; 
    bool start_flag; 
};


void* do_pin_request (void *data)
{
    //unpack args
    struct request_data *prd = (struct request_data *)data; 
    struct epoch_data *edata = prd->edata; 
    struct epoch_hdr   *ehdr = prd->ehdr; 
    bool start_flag = prd->start_flag; 
    bool finish_flag = prd->finish_flag; 


    pid_t cpid, waiting_on_rp_group;
    char *binary = prd->binary; 

    int rc;
    struct timeval tv_start, tv_done, tv_start_dift;
    gettimeofday (&tv_start, NULL);

    int fd = open ("/dev/spec0", O_RDWR);
    if (fd < 0) {
	perror("open /dev/spec0");
	return NULL;
    }

    //start the epoch
    cpid = fork ();
    if (cpid == 0) {
	const char* args[256];
	char attach[80];
	char fake[80];
	char ckpt[80];
	int argcnt = 0;
	    
	args[argcnt++] = "resume";
	args[argcnt++] = "-p";
	args[argcnt++] = ehdr->dirname;
	args[argcnt++] = "--pthread";
	args[argcnt++] = "../../eglibc-2.15/prefix/lib";

	if (!start_flag) {
	    sprintf (attach, "--attach_offset=%d,%u", edata->start_pid, edata->start_clock);
	    args[argcnt++] = attach;
	}
	if (edata->ckpt != 0) { 
	    sprintf (ckpt, "--from_ckpt=%u", edata->ckpt);
	    args[argcnt++] = ckpt;
	    fprintf(stderr, "ckpt: %s\n",ckpt);
	}
	if (edata->start_level == 'u' && edata->stop_level == 'u') {
	    sprintf (fake, "--fake_calls=%u,%u", edata->start_clock, edata->stop_clock);
	    args[argcnt++] = fake;
	} else if (edata->start_level == 'u') {
	    sprintf (fake, "--fake_calls=%u", edata->start_clock);
	    args[argcnt++] = fake;
	} else if (edata->stop_level == 'u') {
	    sprintf (fake, "--fake_calls=%u", edata->stop_clock);
	    args[argcnt++] = fake;
	}
	args[argcnt++] = NULL;
		
	rc = execv ("../../test/resume", (char **) args);
	fprintf (stderr, "execl of resume failed, rc=%d, errno=%d\n", rc, errno);
	return NULL;
    } else {
	//create our listening process
	waiting_on_rp_group = fork(); 
	if(waiting_on_rp_group == 0) { 
	    rc = -1;
	    while(rc < 0){
		rc = wait_for_replay_group(fd,cpid);
	    }
	    return NULL;
	}	    
    }

    // Now attach pin to all of the epoch processes
    bool executing = false;
    cerr << "running " << binary << " from " << edata->start_clock 
	 << " to " << edata->stop_clock << endl;
    do {
	rc = get_attach_status (fd, cpid);
	if (rc > 0) {
	    //sometimes we attach pin to a different process. 
	    //When we do this, we need to save the rc in case we are getting stats 
//	    attach_pid = rc; 

	    pid_t mpid = fork();
	    if (mpid == 0) {
		char cpids[80], syscalls[80], port[80], slice_ip[80], 
		    slice_location[80],	slice_clock[80];

		const char* args[256];
		int argcnt = 0;
			
		args[argcnt++] = "pin";
		args[argcnt++] = "-pid";
		sprintf (cpids, "%d", rc);
		args[argcnt++] = cpids;
		args[argcnt++] = "-t";
		args[argcnt++] = binary;

		sprintf (syscalls, "%d", edata->stop_clock);
		args[argcnt++] = "-l";
		args[argcnt++] = syscalls;			  

		if (!finish_flag) { 
		    args[argcnt++] = "-ao"; // Last epoch does not need to trace to final addresses
		    if (prd->as_flag &&	strlen(prd->ending_as) > 0) { 
			args[argcnt++] = "-ending_as";
			args[argcnt++] = prd->ending_as;
		    }
		}
		else { 
		    if (edata->slice_ip > 0) 
		    {
			sprintf (slice_ip, "%u", edata->slice_ip);

			args[argcnt++] = "-slice_ip";
			args[argcnt++] = slice_ip;
		    }
		    if (edata->slice_location > 0)
		    {
			sprintf (slice_location, "%u", edata->slice_location);
			args[argcnt++] = "-slice_location";
			args[argcnt++] = slice_location;
		    }

		    if (edata->slice_clock > 0 ) { 
			sprintf (slice_clock, "%u", edata->slice_clock);
			args[argcnt++] = "-slice_clock";
			args[argcnt++] = slice_clock;
		    }


		    args[argcnt++] = "-slice_size";
		    args[argcnt++] = "4";
		}

		//always 
		args[argcnt++] = "-so";
		if (prd->as_flag && strlen(prd->starting_as) > 0) { 
		    args[argcnt++] = "-starting_as";
		    args[argcnt++] = prd->starting_as;
		}


		if (prd->as_flag) {
		    args[argcnt++] = "-dift_file";
		    args[argcnt++] = prd->dift_file;			
		}


		args[argcnt++] = "-host";
		args[argcnt++] = edata->hostname;
		args[argcnt++] = "-port";
		sprintf (port, "%d", edata->port);
		args[argcnt++] = port;

		args[argcnt++] = NULL;
		rc = execv ("../../../../pin/pin", (char **) args);
		fprintf (stderr, "execv of pin tool failed, rc=%d, errno=%d\n", rc, errno);
		return NULL;
	    } else {
		gettimeofday (&tv_start_dift, NULL);					
		executing = true;
	    }
	}   
    } while (!executing);

      
    //wait to be done with replay
    int status; 
    pid_t wpid = waitpid (waiting_on_rp_group, &status, 0);
    if (wpid < 0) {
	fprintf (stderr, "waitpid returns %d, errno %d\n", rc, errno);
	return NULL;
    } 

    gettimeofday (&tv_done, NULL);
    printf ("replay start time: %ld.%06ld\n", tv_start.tv_sec, tv_start.tv_usec);
    printf ("PIN start time:    %ld.%06ld\n", tv_start_dift.tv_sec, tv_start_dift.tv_usec);
    printf ("Dift end time:     %ld.%06ld\n", tv_done.tv_sec, tv_done.tv_usec);

    if (tv_done.tv_usec >= tv_start.tv_usec) {
	printf ("Dift time: %ld.%06ld second\n", tv_done.tv_sec-tv_start.tv_sec, tv_done.tv_usec-tv_start.tv_usec);
    } else {
	printf ("Dift time: %ld.%06ld second\n", tv_done.tv_sec-tv_start.tv_sec-1, tv_done.tv_usec+1000000-tv_start.tv_usec);
    }
    
    fflush(stdout);

    return NULL;
}

void do_stream (struct epoch_hdr *ehdr, 
		long port, 
		bool start_flag, 
		bool finish_flag, 
		struct epoch_ctl *this_ectl, 
		struct epoch_ctl *next_ectl,
		const char* dirname)
{
    int rc;
    struct timeval tv_start, tv_done;

    pid_t spid;

    gettimeofday (&tv_start, NULL);

    // Start a stream processor for all epochs at once
	
    spid = fork ();
    if (spid == 0) {
	// Now start up a stream processor for this epoch
	const char* args[256];
	char p[80], parstring[80];//, dirname[80]
	int argcnt = 0;
	
	args[argcnt++] = "parallel_dift";
//	sprintf (dirname, "/tmp/%ld",offset);
	args[argcnt++] = dirname;
	sprintf (p, "%ld", port);		 
	args[argcnt++] = p;
	if (!finish_flag) { 
	    args[argcnt++] = "-iq";
	    args[argcnt++] = next_ectl->inputqhname;
	    args[argcnt++] = next_ectl->inputqbname;
	}
	if (!start_flag) { 
	    args[argcnt++] = "-oq";
	    args[argcnt++] = this_ectl->inputqhname;
	    args[argcnt++] = this_ectl->inputqbname;
	}
	
	args[argcnt++] = "-par";
	sprintf (parstring, "%d", ehdr->parallelize);
	args[argcnt++] = parstring;
	args[argcnt++] = NULL;
	
	rc = execv ("./parallel_dift", (char **) args);
	fprintf (stderr, "execv of stream failed, rc=%d, errno=%d\n", rc, errno);
	return;
    }

    int status;
    pid_t wpid = waitpid (spid, &status, 0);
    if (wpid < 0) {
	fprintf (stderr, "waitpid for stream returns %d, errno %d\n", wpid, errno);
	return;
    } 

    gettimeofday (&tv_done, NULL);

    printf ("Stream start time: %ld.%06ld\n", tv_start.tv_sec, tv_start.tv_usec);
    printf ("Stream end time: %ld.%06ld\n", tv_done.tv_sec, tv_done.tv_usec);
    if (tv_done.tv_usec >= tv_start.tv_usec) {
	printf ("Stream time: %ld.%06ld second\n", tv_done.tv_sec-tv_start.tv_sec, tv_done.tv_usec-tv_start.tv_usec);
    } else {
	printf ("Stream time: %ld.%06ld second\n", tv_done.tv_sec-tv_start.tv_sec-1, tv_done.tv_usec+1000000-tv_start.tv_usec);
    }
    
    fflush(stdout);   
}

void do_slice (struct epoch_hdr *ehdr, 
	       long port, 
	       bool start_flag, 
	       bool finish_flag,
	       struct epoch_ctl *this_ectl, 
	       struct epoch_ctl *next_ectl,
	       const char* dirname)
{
    int rc;
    struct timeval tv_start, tv_done;

    pid_t spid;

    gettimeofday (&tv_start, NULL);

    // Start a stream processor for all epochs at once
	
    spid = fork ();
    if (spid == 0) {
	// Now start up a stream processor for this epoch
	const char* args[256];
	char p[80], parstring[80];//, dirname[80]
	int argcnt = 0;
	
//	args[argcnt++] = "parallel_flowback";
	args[argcnt++] = "cmp_flowback";
//	sprintf (dirname, "/tmp/%ld",offset);
	args[argcnt++] = dirname;
	sprintf (p, "%ld", port);		 
	args[argcnt++] = p;
	if (!finish_flag) { 
	    args[argcnt++] = "-iq";
	    args[argcnt++] = next_ectl->inputqhname;
	    args[argcnt++] = next_ectl->inputqbname;
	}
	if (!start_flag) { 
	    args[argcnt++] = "-oq";
	    args[argcnt++] = this_ectl->inputqhname;
	    args[argcnt++] = this_ectl->inputqbname;
	}

	args[argcnt++] = "-par";
	sprintf (parstring, "%d", ehdr->parallelize);
	args[argcnt++] = parstring;
	args[argcnt++] = NULL;
	
	rc = execv ("./cmp_flowback", (char **) args);
	fprintf (stderr, "execv of stream failed, rc=%d, errno=%d\n", rc, errno);
	return;
    }

    int status;
    pid_t wpid = waitpid (spid, &status, 0);
    if (wpid < 0) {
	fprintf (stderr, "waitpid for stream returns %d, errno %d\n", wpid, errno);
	return;
    } 

    gettimeofday (&tv_done, NULL);

    printf ("Stream start time: %ld.%06ld\n", tv_start.tv_sec, tv_start.tv_usec);
    printf ("Stream end time: %ld.%06ld\n", tv_done.tv_sec, tv_done.tv_usec);
    if (tv_done.tv_usec >= tv_start.tv_usec) {
	printf ("Stream time: %ld.%06ld second\n", tv_done.tv_sec-tv_start.tv_sec, tv_done.tv_usec-tv_start.tv_usec);
    } else {
	printf ("Stream time: %ld.%06ld second\n", tv_done.tv_sec-tv_start.tv_sec-1, tv_done.tv_usec+1000000-tv_start.tv_usec);
    }
    
    fflush(stdout);   
}


void* do_epoch(void *arg) 
{
    //unpack 
    request_data *rd = (request_data *)arg; 
    int rc; 
    pthread_t tid;

/*
    rd->as_flag = false;
    sprintf(rd->binary, "../obj-ia32/dift.so"); 
    rc = pthread_create (&tid, NULL, do_pin_request, (void *) rd);
    if (rc < 0) {
	fprintf (stderr, "Cannot spawn stream thread,rc=%d\n", rc);
    }

    do_stream(rd->ehdr, rd->edata->port, rd->start_flag, rd->finish_flag, rd->this_ectl, 
	      rd->next_ectl, rd->dirname); 
*/
    //will wait until stream finished

    sprintf(rd->binary, "../obj-ia32/bw_slice.so"); 
    rd->as_flag = false;

    rc = pthread_create (&tid, NULL, do_pin_request, (void *) rd);
    if (rc < 0) {
	fprintf (stderr, "Cannot spawn stream thread,rc=%d\n", rc);
    }
    do_slice(rd->ehdr, rd->edata->port, rd->start_flag, rd->finish_flag, rd->this_ectl, rd->next_ectl, rd->dirname);

    return NULL;
}

void* do_request (void* arg) 
{
    long s = (long) arg;
    u_long i; 

    // Receive control data
    struct epoch_hdr ehdr;
    int rc = safe_read (s, &ehdr, sizeof(ehdr));
    if (rc != sizeof(ehdr)) {
	fprintf (stderr, "Cannot recieve header,rc=%d\n", rc);
	return NULL;
    }
    
    //receive epoch_data 
    u_long epochs = ehdr.epochs;
    struct epoch_data edata[epochs];
    rc = safe_read (s, edata, sizeof(struct epoch_data)*epochs);
    if (rc != (int) (sizeof(struct epoch_data)*epochs)) {
	fprintf (stderr, "Cannot recieve epochs,rc=%d\n", rc);
	fprintf(stderr, "expecting %lu epochs, size %lu\n",epochs,sizeof(struct epoch_data)*epochs);
	return NULL;
    }

    struct epoch_ctl ectl[epochs+1];
    pthread_t tids[epochs];
    request_data rds[epochs];
    rc = setup_shm(ectl, epochs, &ehdr);
    assert (!rc);


    for (i = 0; i < epochs; ++i) 
    {
	request_data *rd = (rds + i);

	rd->ehdr = &ehdr; 
	rd->edata = (edata + i); 
	rd->start_flag = (i == 0 && ehdr.start_flag);
	rd->finish_flag = (i == (epochs-1) && ehdr.finish_flag);
	rd->this_ectl = ectl+i;
	rd->next_ectl = ectl+i+1; 

	sprintf(rd->dirname, "/tmp/%ld/",i);
	sprintf(rd->starting_as, "/tmp/%ld/starting_as",i);

	if (!rd->finish_flag) 
	    sprintf(rd->ending_as, "/tmp/%ld/ending_as",i);
	else 
	    rd->ending_as[0] = '\0';
	
	sprintf(rd->dift_file, "/tmp/%ld/dift",i);

//	rd->s = s; needed? maybe later! 
	
	rc = pthread_create (tids + i, NULL, do_epoch, (void *) rd);
	if (rc < 0) {
	    fprintf (stderr, "Cannot spawn stream thread,rc=%d\n", rc);
	}
    }
    for (i = 0; i < epochs; ++i) 
    {
	void *rtn; 
	rc = pthread_join (tids[i], (void **) &rtn);
	if (rc < 0) {
	    fprintf (stderr, "Cannot join stream thread,rc=%d\n", rc);
	}
    }   

    rc = cleanup_shm(ectl, epochs, &ehdr);
    assert (!rc);
    return NULL;
}

int main (int argc, char* argv[]) 
{
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
	
	// Spawn a thread to handle this request
	pthread_t tid;
 	rc = pthread_create (&tid, NULL, do_request, (void *) s);
	if (rc < 0) {
	    fprintf (stderr, "Cannot spawn stream thread,rc=%d\n", rc);
	}
    }

    return 0;
}
