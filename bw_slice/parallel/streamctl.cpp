#define __USE_LARGEFILE64

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <atomic>
#include <vector>
#include <dirent.h>
#include <unordered_set>
#include <string>

using namespace std;

#include "streamserver.h"
#include "../../test/parseklib.h" //why? 
#include "streamnw.h"

struct server { 
    char hostname[256];
    u_int num_epochs;
    int s; // socket used for communication
};

/* An individual epoch */
struct epoch {
    struct server *server;
    struct epoch_data data;
};

/* All the configuration data */
struct config {
    vector<server *> servers;
    vector<epoch> epochs;
};
	
// One for each streamserver
struct epoch_ctl {
    u_long start;
    u_long num;
    int    s;
    pid_t wait_pid;
};


int connect_to_server (const char* hostname, int port)
{
    // Connect to streamserver
    struct hostent* hp = gethostbyname (hostname);
    if (hp == NULL) {
	fprintf (stderr, "Invalid host %s, errno=%d\n", hostname, h_errno);
	return -1;
    }
    
    int s = socket (AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
	fprintf (stderr, "Cannot create socket, errno=%d\n", errno);
	return s;
    }
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy (&addr.sin_addr, hp->h_addr, hp->h_length);
    
    // Receiver may not be started, so spin until connection is accepted
    long rc = connect (s, (struct sockaddr *) &addr, sizeof(addr));
    if (rc < 0) {
	fprintf (stderr, "Cannot connect to %s:%d, errno=%d\n", hostname, port, errno);
	return rc;
    }
    return s;
}

int fetch_results (char* top_dir, struct epoch_ctl ectl)
{
    char dir[512];

    for (u_long i = 0; i < ectl.num; i++) {
	sprintf (dir, "%s/%lu", top_dir, ectl.start+i);
	long rc = mkdir (dir, 0755);
	if (rc < 0) {
	    fprintf (stderr, "Cannot make dir %s\n", dir);
	    return rc;
	}
	// Fetch 4 files: results, addresses, input and output tokens
	for (int j = 0; j < 4; j++) {
	    if (fetch_file(ectl.s, dir) < 0) return -1;
	}
    }
    return 0;
}

void format ()
{
    fprintf (stderr, "format: streamctl <epoch description file> <host config file> [-w] [-s] [-c] [-lowmem]\n");
    fprintf (stderr, "                  [-filter_inet] [-v dest_dir cmp_dir] [-stats] [-seq/-seqpp]\n");
    exit (0);
}

int main (int argc, char* argv[]) 
{
    int rc;
    char dirname[80];
    int wait_for_response = 0,get_stats = 0;
    char* filter_output_after = NULL;

    struct config conf;

    if (argc < 3) {
	format();
    }

    const char* host_suffix = NULL, *slice_ip = NULL, 
	*slice_location = NULL, *slice_clock;

    const char* epoch_filename = argv[1];
    const char* config_filename = argv[2];

    for (int i = 3; i < argc; i++) {
	if (!strcmp (argv[i], "-w")) {
	    wait_for_response = 1;
	} else if (!strcmp (argv[i], "-filter_output_after")) {
	    filter_output_after = argv[i+1];
	    i++;
	} else if (!strcmp (argv[i], "-stats")) {
	    get_stats = 1;
	} else if (!strcmp (argv[i], "-hs")) {
	  host_suffix = argv[i+1];
	  i++;
	} else if (!strcmp (argv[i], "-slice_ip")){
	    slice_ip = argv[i+1];
	    i++;
	} else if (!strcmp (argv[i], "-slice_clock")){
	    slice_clock = argv[i+1];
	    i++;
	} else if (!strcmp (argv[i], "-slice_location")){
	    slice_location = argv[i+1];
	    i++;
	}  else {
	     format();
	 }
     }


     // Read in the epoch file
     FILE* file = fopen(epoch_filename, "r");
     if (file == NULL) {
	 fprintf (stderr, "Unable to open epoch description file %s, errno=%d\n", epoch_filename, errno);
	 return -1;
     }
     rc = fscanf (file, "%79s\n", dirname);
     if (rc != 1) {
	 fprintf (stderr, "Unable to parse header line of epoch descrtion file, rc=%d\n", rc);
	 return -1;
     }
     while (!feof(file)) {
	 char line[256];
	 if (fgets (line, 255, file)) {
	     struct epoch e;
	     e.data.slice_ip = 0;
	     e.data.slice_location = 0;
	     u_int unused;
	     char *end;
	     rc = sscanf (line, "%d %c %u %c %u %u %u %u\n", &e.data.start_pid, &e.data.start_level, &e.data.start_clock, &e.data.stop_level, &e.data.stop_clock, &unused, &e.data.ckpt, &e.data.fork_flags);
	     if (slice_ip)
		 e.data.slice_ip = strtoul(slice_ip, &end, 16);	     
	     if (slice_location)
		 e.data.slice_location = strtoul(slice_location, &end, 16);

	     if (slice_clock != NULL)
		 e.data.slice_clock = strtoul(slice_clock, &end, 16);
	     else 
		 e.data.slice_clock = 0;

	     if (rc != 8) {
		 fprintf (stderr, "Unable to parse line of epoch descrtion file: %s\n", line);
		 return -1;
	     }
	     conf.epochs.push_back(e);
	 }
     }
     fclose(file);

     // Read in the host configuration file
     file = fopen(config_filename, "r");
     if (file == NULL) {
	 fprintf (stderr, "Unable to open host configuration file %s, errno=%d\n", epoch_filename, errno);
	 return -1;

     }

     // Now parse the configuration file
     // Stop if # of epochs is exceeded (config is generic so may work for more epochs) 
     u_int epoch_cnt = 0;
     while (!feof(file) && epoch_cnt < conf.epochs.size()) {
	 char line[256];
	 if (fgets (line, 255, file)) {
	     int num_epochs;
	     char host[256];
	     rc = sscanf (line, "%d %s\n", &num_epochs, host);
	     if (rc != 2) {
		 fprintf (stderr, "Unable to parse line of server config file: %s\n", line);
		 return -1;
	     }
	     if (epoch_cnt + num_epochs > conf.epochs.size()) {
		 // Will not use all the epochs in this row
		 num_epochs -= (epoch_cnt + num_epochs) - conf.epochs.size();
		 printf ("Truncated num_epochs to %d\n", num_epochs);
	     }

	     struct server* s = new server; 
	     strcpy (s->hostname, host);
	     s->num_epochs = num_epochs;
	     s->s = -1;

	     conf.servers.push_back(s);

	     for (auto i = 0; i < num_epochs; i++) {
		 conf.epochs[epoch_cnt].server = s;
		 epoch_cnt++;
	     }
	 }
     }
     fclose(file);
     //contact each of the servers
     epoch_cnt = 0;
     for (u_int i = 0; i < conf.servers.size(); i++) {
	 //build the header: 
	 struct epoch_hdr ehdr;
	 ehdr.flags = 0;
	 if (wait_for_response) ehdr.flags |= SEND_ACK;
	 if (get_stats) ehdr.flags |= SEND_STATS;

	 if (filter_output_after) {
	     ehdr.filter_flags |= FILTER_OUT;
	     strcpy (ehdr.filter_output_after, filter_output_after);
	 }

	 strcpy (ehdr.dirname, dirname);
	 ehdr.epochs = conf.servers[i]->num_epochs;
	 if (i == 0) {
	     ehdr.start_flag = true;
	 } else {
	     ehdr.start_flag = false;
	     strcpy (ehdr.prev_host, conf.servers[i-1]->hostname);
	 }
	 if (i == conf.servers.size()-1) {
	     ehdr.finish_flag = true; 
	 } else {
	     ehdr.finish_flag = false;
	     strcpy (ehdr.next_host, conf.servers[i+1]->hostname);
	 }

	 ehdr.parallelize = conf.servers[i]->num_epochs;
	 ehdr.cmd_type = DO_SLICE;//DIFT; //we're just testing dift right meow. 

	 char real_host[1024];
	 if (host_suffix && strstr(conf.servers[i]->hostname, ".net") == NULL) 
	     sprintf(real_host,"%s%s",conf.servers[i]->hostname,host_suffix);
	 else
	   sprintf(real_host,"%s",conf.servers[i]->hostname);


	 conf.servers[i]->s = connect_to_server (real_host, STREAMSERVER_PORT);
	 if (conf.servers[i]->s < 0) return conf.servers[i]->s;

	 rc = safe_write (conf.servers[i]->s, &ehdr, sizeof(ehdr));
	 if (rc != sizeof(ehdr)) {
	     fprintf (stderr, "Cannot send header to streamserver, rc=%d\n", rc);
	     return rc;
	 }

	 // Now send them the epoch data
	 for (u_int j = 0; j < conf.servers[i]->num_epochs; j++) {
	     strcpy (conf.epochs[epoch_cnt].data.hostname, conf.servers[i]->hostname);
	     conf.epochs[epoch_cnt].data.port = AGG_BASE_PORT+j; 
	     rc = safe_write (conf.servers[i]->s, &conf.epochs[epoch_cnt].data, sizeof(struct epoch_data));
	     epoch_cnt++;
	     if (rc != sizeof(struct epoch_data)) {
		 fprintf (stderr, "Cannot send epoch data to streamserver, rc=%d\n", rc);
		 return rc;
	     }
	 }
     }

    if (wait_for_response) {
	for (u_int i = 0; i < conf.servers.size(); i++) {
	    struct epoch_ack ack;
	    rc = safe_read (conf.servers[i]->s, &ack, sizeof(ack));
	    if (rc != sizeof(ack)) {
		fprintf (stderr, "Cannot recv ack,rc=%d,errno=%d\n", rc, errno);
	    }
	}
    }

    return 0;
}
