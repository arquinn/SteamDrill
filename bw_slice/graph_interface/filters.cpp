#include <stdlib.h>
#include <assert.h>
#include <regex.h>
#include <sys/mman.h>
#include <string.h>
#include <vector>

#include "filters.h"

using namespace std;

extern u_long* ppthread_log_clock;

/* List of filenames to create new taints from.
 * This list is for the entire replay group */
struct filter_input_file {
    char filename[256];
};

struct filter_ip {
    u_long startip;
    u_long endip; 
};

vector<filter_input_file> filter_input_files;
vector<filter_input_file> filter_input_partfiles;
vector<filter_ip> filter_ips;
bool filter_inputs_initialized = false; 


vector<nodeid> filter_nodeids;
bool filter_nodeids_initialized = false;
std::vector<nodeid>::iterator filter_nodeids_it;

u_long filter_outputs_before = 0;  // Only trace outputs starting at this value

//slice variables
extern u_long slice_clock;


void add_output_filters(u_long before)
{
    filter_outputs_before = before;
}

void add_filename_filter(const char *filter) 
{
    struct filter_input_file fif;      
    strncpy(fif.filename, filter, 256);
    filter_input_files.push_back(fif);
}

void add_partname_filter(const char* filter) 
{
    struct filter_input_file fif;
    strncpy(fif.filename, (char *) filter, 256);
    filter_input_partfiles.push_back(fif);
}

void add_ip_filter(u_long startip, u_long endip)
{
    struct filter_ip fip;
    fip.startip = startip;
    fip.endip = endip;
    filter_ips.push_back(fip);
}

void add_taint_num_filter(u_long taintid) 
{
    filter_nodeids.push_back(taintid);
}

void add_input_filters(FILE* file)
{   
    while (!feof(file)) {
	char line[256];
	if (fgets (line, 255, file)) {	   
	    char *type = strtok(line," ");
	    if (!strcmp(type, "partfile"))   {
		char *filename = strtok(NULL, " ");
		add_filename_filter(filename);

	    } else if (!strcmp(type, "filename")){
		char *filename = strtok(NULL, " ");
		add_partname_filter(filename);
	    }
	    else { 
		char *startip = strtok(NULL, " ");
		char *endip = strtok(NULL, " ");
		add_ip_filter(strtol(startip, NULL, 0), 
			      strtol(endip, NULL, 0));
	    }	    
	}
    }
    fclose(file);
}


void add_taint_num_filters(FILE *file)
{
    
    while (!feof(file)) {
	char line[256];
	if (fgets (line, 255, file)) {
	    add_taint_num_filter(strtoul(line, NULL, 10)); //for now
	}
    }    
    filter_nodeids_it = filter_nodeids.begin();
    filter_nodeids_initialized = true;
    cerr << "sizeof taint_num filters " << filter_nodeids.size() << endl;
}

void dump_input_filters() 
{
    for (auto fif : filter_input_files) {
	fprintf(stderr, "fif: %s\n",fif.filename);
    }

    for (auto fif : filter_input_partfiles) { 
	fprintf(stderr, "fip: %s\n",fif.filename);
    }
    for (auto fif : filter_ips) {
	fprintf(stderr, "ips: [0x%lx, 0x%lx]\n",fif.startip, fif.endip);
    }   
}



int filter_filename(char* filename) {

    //if there aren't any allowed ips, let everything through
    if (!filter_input_files.size()) return 1;


    for (auto fif : filter_input_files) 
    {
        if (!strcmp(fif.filename, filename)) {
            return 1;
        }
    }
    return 0;
}

int filter_partfilename(char* filename) {

    //if there aren't any allowed ips, let everything through
    if (!filter_input_partfiles.size()) return 1;

    for (auto fif : filter_input_partfiles) { 
        if (strstr(filename, fif.filename)) {
            return 1;
        }
    }
    return 0;
}

int filter_ip(u_long ip) {
    
    //if there aren't any allowed ips, let everything through
    if (!filter_ips.size()) return 1;

    for (auto fif : filter_ips) {
	if (ip >= fif.startip && ip <= fif.endip) {	   
	    return 1;
	}
    }
    return 0;
}

bool filter_nodeid(nodeid id) 
{
    if (!filter_nodeids_initialized) return 1;

    while (filter_nodeids_it != filter_nodeids.end() &&
	   *filter_nodeids_it < id)
    {
	++filter_nodeids_it; 
    }

//    cerr << "filter_nodeid nxt nodeid " 
//	 << *filter_nodeids_it << " curretly checking " 
//	 << id << endl;


    return (filter_nodeids_it != filter_nodeids.end() &&	   
	    *filter_nodeids_it == id); 
}

int filter_inputs(char *channel_name, u_long ip)
{
    int filter = 0;
    if (channel_name) { 
	filter = (filter_filename(channel_name) || 
		  filter_partfilename(channel_name));
    }
    if (ip) {
	filter = filter || filter_ip(ip); 
    }
    return filter;
}


int filter_outputs (void)
{
    if (*ppthread_log_clock >= filter_outputs_before) return 1;
    return 0;
}

int filter_slice() 
{
    if (!slice_clock || *ppthread_log_clock >= slice_clock)
	return 1;

    return 0;
}
