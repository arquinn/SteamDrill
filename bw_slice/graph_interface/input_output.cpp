#include <stdlib.h>
#include <assert.h>
#include <regex.h>
#include <sys/mman.h>
#include <cstring>
#include <fcntl.h>

#include "../taint_nw.h"
#include "filters.h"
#include "graph_interface.h"

int input_fd = -1; 
int output_fd = -1;

#ifdef USE_SHMEM
#define INPUTBUF_SIZE 0x1000
static struct input_info* inputbuf;
static u_long inputindex = 0;
static u_long input_total_count = 0;

#define OUTBUF_SIZE 0x1000
static char* outputbuf = NULL;
static u_long outputindex = 0;
static u_long output_total_count = 0;

//forward declare function
void static inline flush_inputbuf();
#endif

extern struct thread_data *current_thread;
extern nodeid taint_num;
extern u_long slice_location;
extern u_long slice_size;
#ifdef TAINT_STATS
extern struct taint_stats_profile tsp;
#endif

extern uint64_t instcount;

void static inline open_buffer(const char *prefix, char *group_dir, int &fd, u_long max_size)
{
    char filename[256];
    int rc; 

    snprintf(filename, 256, "/%s_shm%s", prefix, group_dir);    
    for (u_int i = 1; i < strlen(filename); i++) {
	if (filename[i] == '/') filename[i] = '.';
    }
    
    fd = shm_open(filename, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
	fprintf(stderr, "could not open shmem %s, errno %d\n", filename, errno);
	assert(0);
    }

    rc = ftruncate64 (fd, max_size);
    if (rc < 0) {
	fprintf(stderr, "could not truncate tokens %d, errno %d\n", fd, errno);
	assert(0);
    }   
}

void static inline make_input_info(struct input_info &ii, 
				   uint_fast32_t taint_num_start, 
				   uint_fast32_t size,
				   uint_fast32_t location = 0) 
{
    ii.rg_id = current_thread->rg_id;
    ii.record_pid = current_thread->record_pid;
    ii.syscall_cnt = current_thread->syscall_cnt; //should be clock maybe? (I prefer clock...)
    ii.nodeid_start = taint_num_start;
    ii.size = size;    
    ii.location = location;
}

void static inline make_syscall_input_info(struct input_info &ii, input_info_type type, int read_fileno, 
				      uint_fast32_t taint_num_start, uint_fast32_t size) { 
	
    make_input_info(ii, taint_num_start, size);
    ii.type = type;
    ii.syscall_info.fileno = read_fileno;   

#ifdef VERBOSE
    if (VERBOSE(taint_num_start)) 
	log_f << std::hex << "sii " << ii.record_pid << "," << ii.nodeid_start << "," << ii.size << ","
	  << ii.syscall_cnt << endl;
#endif
}

void static inline  make_instruction_input_info(struct input_info &ii, 
						uint32_t ip, 
						uint_fast32_t taint_num_start, 
						uint_fast32_t size,
						uint_fast32_t location = 0) 
{ 
    make_input_info(ii, taint_num_start, size, location);
    ii.type = input_info_type::IMM;
    ii.imm_info.ip = ip;
    ii.imm_info.instcount = instcount; 

#ifdef VERBOSE
    if (ip == 0x8054e26) 
	log_f << std::hex << "iii " << ii.record_pid << "," << ii.nodeid_start << "," << ii.size << ","
	      << ii.imm_info.ip << endl;

    if (VERBOSE(taint_num_start)) 
	log_f << std::hex << "iii " << ii.record_pid << "," << ii.nodeid_start << "," << ii.size << ","
	      << ii.imm_info.ip << endl;
#endif
}

void static inline write_input_info(int outfd, struct input_info &ii) 
{
#ifdef USE_SHMEM
    assert(outfd == input_fd);

    if (inputindex == INPUTBUF_SIZE) flush_inputbuf();
    inputbuf[inputindex++] = ii;

#else    
    int rc;
    uint32_t written = 0; 
    while (written < sizeof(struct input_info)) 
    {
	rc = write(outfd, (&ii) + written, sizeof(struct input_info) - written);
	if (rc < 0) { 
	    fprintf(stderr, "[ERROR] Could not write token to file, got %d, expected %d, errno %d\n", 
		    rc, sizeof(struct input_info), errno);	    
	    assert (0);
	}
	written += rc; 
    }
#endif
}


#ifdef USE_SHMEM
void static inline flush_inputbuf()
{    			       
    input_total_count += inputindex;

    // Check for overflow
    if (input_total_count >= MAX_INPUTS_SIZE/sizeof(struct input_info)) {
	fprintf (stderr, "Cannot allocate any more token buffer than %ld\n", (u_long) input_total_count);
	assert (0);
    }

    // Unmap the current region
    if (munmap (inputbuf, INPUTBUF_SIZE*sizeof(struct input_info)) < 0) {
	fprintf (stderr, "could not munmap token buffer, errno=%d\n", errno);
	assert (0);
    }

    // Map in the next region
    inputbuf = (struct input_info*) mmap (0, INPUTBUF_SIZE*sizeof(struct input_info), PROT_READ|PROT_WRITE, 
					  MAP_SHARED, input_fd, input_total_count*sizeof(struct input_info));
    if (inputbuf == MAP_FAILED) {
	fprintf (stderr, "could not map token buffer, errno=%d\n", errno);
	assert (0);
    }
    inputindex = 0;
}


void static inline flush_outputbuf()
{
    // Unmap the current region
    if (munmap (outputbuf, OUTBUF_SIZE) < 0) {
	fprintf (stderr, "could not munmap token buffer, errno=%d\n", errno);
	assert (0);
    }

    // Map in the next region
    outputbuf = (char *) mmap (0, OUTBUF_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, output_fd, output_total_count);
    if (outputbuf == MAP_FAILED) {
	fprintf (stderr, "could not map token buffer, errno=%d\n", errno);
	fprintf (stderr, "size %d fd %d count 0x%lx\n",OUTBUF_SIZE, output_fd, output_total_count);
	assert (0);
    }
    outputindex = 0;
}


static void fill_outbuf(char *pout, struct input_info *ii, void *buf, int size) { 
    memcpy(pout, ii, sizeof(struct input_info));
    pout += sizeof(struct input_info);
    memcpy(pout, &buf, sizeof(void *));
    pout += sizeof(void *);
    memcpy(pout, &size, sizeof(int));
    pout += sizeof(int);


//    cerr << std::hex << "writing output for " << buf << "," << size << "@" << ii->imm_info.ip
//	 << "(clock: " << *ppthread_log_clock << ")\n";
	
    if ((u_long)buf <= 0x30) { 
	nodeid *reg_nodes = get_reg_nodes((int)buf, 0);
//	for (int i = 0; i< size; ++i) { 
//	    cerr << std::hex << " " << reg_nodes[i];
//	}
//	cerr << endl;
	memcpy(pout, reg_nodes, sizeof(nodeid) * size);
	pout += sizeof(nodeid) * size;
    }
    else { 
	for (int i = 0; i < size; i++) {
	    u_long addr = ((u_long) buf) + i;
	    nodeid value = 0; 
	    nodeid *mem_nodes;

	    get_mem_nodes(addr, 1, &mem_nodes);

	    if (mem_nodes)	value = mem_nodes[0]; 
	
	    memcpy(pout, &value, sizeof(nodeid));
	    pout += sizeof(nodeid);
//	    cerr << std::hex << " " << value;
	}
    }
//    cerr << endl;
}

#else
void static inline  write_output_header(struct input_info* ii,
					void* buf, int buf_size){

    int rc;
    write_input_info(output_fd, *ii); 


    rc = write(output_fd, &buf, sizeof(u_long));
    if (rc != sizeof(u_long)) {
        fprintf(stderr, "write_outpu_header: write buf expected to write int size %d, got %d, errno %d\n",
                sizeof(u_long), rc, errno);
        return;
    }
    rc = write(output_fd, &buf_size, sizeof(int));
    if (rc != sizeof(int)) {
        fprintf(stderr, "taint_byte_creation: write_output_header expected to write int size %d, got %d\n",
                sizeof(int), rc);
        return;
    }
}

void static inline  write_output_nodes (void* buf, int size)
{
    int i;
    nodeid *mem_nodes;

    for (i = 0; i < size; i++) {
        u_long addr = ((u_long) buf) + i;
	int rc; 
	nodeid value = 0; 

        get_mem_nodes(addr, 1, &mem_nodes);

	if (mem_nodes)	value = mem_nodes[0]; 
	cerr << std::hex << value << " "; 
	
	rc = write(output_fd, &value, sizeof(nodeid));
	if (rc != sizeof(nodeid)) {
	    fprintf(stderr, "Could not write taint value\n");
	}
    }
    cerr << endl;
}
#endif


void init_input_output(char *group_dir)
{
#ifdef USE_SHMEM
    open_buffer("inputs", group_dir, input_fd, MAX_INPUTS_SIZE);
    open_buffer("outputs", group_dir, output_fd, MAX_OUT_SIZE);

    inputbuf = (struct input_info *) mmap (0, INPUTBUF_SIZE*sizeof(struct input_info), 
				      PROT_READ|PROT_WRITE, MAP_SHARED, input_fd, 0);
    if (inputbuf == MAP_FAILED) {
	fprintf (stderr, "could not map output buffer, errno=%d\n", errno);
	assert (0);
    }

    outputbuf = (char *) mmap (0, OUTBUF_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, output_fd, 0);
    if (outputbuf == MAP_FAILED) {
	fprintf (stderr, "could not map output buffer, errno=%d\n", errno);
	assert (0);
    }


#else
    char name[256];
    snprintf(name, 256, "%s/inputs", group_dir);
    input_fd = open(name, O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE, 0644);
    if (input_fd < 0) {
	fprintf(stderr, "Could not open tokens file %s\n", name);
	exit(-1);
    }
    char output_file_name[256];
    snprintf(output_file_name, 256, "%s/outputs", group_dir);
    output_fd = open(output_file_name, O_CREAT | O_TRUNC | O_LARGEFILE | O_RDWR, 0644);
    if (output_fd < 0) {
	fprintf(stderr, "could not open output file %s, errno %d\n", output_file_name, errno);
	exit(-1);
    }
#endif
}

void finish_input_output()
{
#ifdef USE_SHMEM
    int rc = ftruncate(input_fd, (input_total_count + inputindex) * sizeof(struct input_info));
    if (rc < 0) 
	assert(0);
    rc = ftruncate(output_fd, output_total_count);
    if (rc < 0) 
	assert(0);

#endif
}


void syscall_input(void *buf, uint32_t size, input_info_type type, int read_fileno, 
		   char *channel_name) 
{
    u_int i = 0;
    nodeid start = taint_num;
    assert(start < NODE_OFFSET);
    int filter;    
    input_info ii;
    u_long buf_addr = (u_long) buf;

#ifdef VERBOSE
    if (0xb77a045b >= (u_long ) buf && 0xb77a045b <= (u_long)buf + size)
	log_f << hex << "syscall input of [" << buf << ", " << (u_long)buf + size
	      << "] \n ";
#endif


    if (size <= 0) return;
    if (!buf) return;
    filter = filter_inputs(channel_name, 0);
    if (!filter) {
#ifdef VERBOSE
	if (VERBOSE(start))
	    log_f << "si filtered " << start << "," << size << endl;
#endif
	return;
    }

    bool created = false;
    for (i = 0; i < size; ++i) { 
	bool c = create_node(buf_addr + i, 1);
#ifdef VERBOSE
	if (VERBOSE(taint_num) || V_ADDR(buf_addr + i))
	{
	    if (c)
		log_f << "si created " << taint_num << "," << size << endl;
	    else 
		log_f << "si not created " << taint_num << "," << size << endl;
	}
#endif		
	created = created || c;
    }
    if (!created) { 
	return;
    }
    make_syscall_input_info(ii, type, read_fileno, start, size);
    write_input_info(input_fd, ii);

}

void immval_input_mem(void* buf, int size, uint32_t ip)
{
    nodeid start = taint_num;
    int filter;
    input_info ii;
    if (size <= 0) return;
    if (!buf) return;
    filter = filter_inputs(NULL, ip);
    if (!filter) {
#ifdef VERBOSE
	if (VERBOSE(start))
	    log_f << "iim filtered out " << instcount << endl;
#endif
	return;
    }
    
    bool created = create_node((u_long)buf,size);  
    if (!created) { 
#ifdef VERBOSE
	if (VERBOSE(start))
	    log_f << std::hex << "iim nodeid filtered " << instcount << "," << start << endl;
#endif
#ifdef TAINT_STATS
    tsp.imm_filtered ++;
#endif
	return;
    }    

#ifdef VERBOSE
    if(VERBOSE(start))
	log_f << "iim " << instcount << "," << start
	      << " " <<  buf << ", " << size << endl;
#endif
    
    make_instruction_input_info(ii, ip, start, 1, (uint_fast32_t)buf);
    write_input_info(input_fd, ii);
#ifdef TAINT_STATS
    tsp.immvals ++;
#endif
}

void immval_input_reg(int reg, uint32_t size, uint32_t offs,
		      uint32_t ip)
{
    nodeid start = taint_num;
    int filter;
    input_info ii;
    if (size <= 0) return;
    filter = filter_inputs(NULL, ip);
    if (!filter) {
#ifdef VERBOSE
	if (VERBOSE(start))
	    log_f << "iir filtered out " << instcount << endl;
#endif
	return;
    }

    bool created =  create_node_reg(reg, size, offs);
    //add a new node for the register

    if (!created) { 
#ifdef VERBOSE
	if (VERBOSE(start))
	    log_f << "iir nodeid filtered " << instcount << ","  << start << endl;
#endif
#ifdef TAINT_STATS
    tsp.imm_filtered ++;
#endif
	return;
    }    
    
#ifdef VERBOSE
    if (VERBOSE(start))
	log_f << std::hex << "iir " << instcount << "," << start << endl;
#endif

    make_instruction_input_info(ii, ip, start, 1, (uint_fast32_t)reg);
    write_input_info(input_fd, ii);
#ifdef TAINT_STATS
    tsp.immvals ++;
#endif
}

nodeid immval_input_none(uint32_t ip)
{
    nodeid start = taint_num;
    int filter;
    input_info ii;
    filter = filter_inputs(NULL, ip);
    if (!filter) {
#ifdef VERBOSE
	if (VERBOSE(start))
	    log_f << "iin filtered out " << instcount << endl;
#endif
	return NULL_NODE;
    }

    start = get_next_nodeid(); 
    //add a new node for the register

    if (start == NULL_NODE) { 
	
#ifdef TAINT_STATS
	tsp.imm_filtered ++;
#endif
    }    
    else { 
	make_instruction_input_info(ii, ip, start, 1);
	write_input_info(input_fd, ii);
#ifdef TAINT_STATS
	tsp.immvals ++;
#endif
    }
    return start;
}

void instruction_check_output(u_long ip)
{
    int filter = filter_slice();
    if (filter)
    {
	struct input_info ii;
	make_instruction_input_info(ii,ip,0,0); //last two couldn't matter less! 
	output((void *)slice_location, slice_size,
	       &ii, true);
    }
}


void instruction_output(u_long ip, void *location, int size)
{
    struct input_info ii;
    make_instruction_input_info(ii,ip,0,0); //last two couldn't matter less! 
    output(location, size, &ii, true);
}

void output(void* buf, int size,
	    struct input_info* ii,
	    bool pass_filter)
{

#ifdef USE_SHMEM
    if (pass_filter || filter_outputs()) {

	u_long datasize = sizeof(struct input_info) + sizeof(u_long) + sizeof(void *) + size * (sizeof(nodeid));

	// Check for overflow
	if (output_total_count+datasize >= MAX_OUT_SIZE) {
	    fprintf (stderr, "Cannot allocate any more output buffer than %ld\n", (u_long) output_total_count+datasize);
	    assert (0);
	}
	if (outputindex+datasize >= OUTBUF_SIZE) {

	    char* outbuf = (char *) malloc(datasize);
	    u_long bytes_left = OUTBUF_SIZE - outputindex;
//	    cerr << dec << "start output_total_count " << output_total_count
//		 << " datasize " << datasize << endl;

	    assert (outbuf);
	    fill_outbuf (outbuf, ii, buf, size);
	    memcpy (outputbuf+outputindex, outbuf, bytes_left);

	    //update counters
	    output_total_count += bytes_left; 


	    flush_outputbuf(); 
//	    cerr << dec << "after flush output_total_count " << output_total_count 
//		 << " bytes_left " << bytes_left << endl;

	    // Now copy last bytes to new shmem
	    memcpy (outputbuf, outbuf + bytes_left, datasize - bytes_left);
	    free(outbuf);

	    //update counters
	    output_total_count += datasize - bytes_left;
	    outputindex = datasize - bytes_left;

//	    cerr << dec << "finished output_total_count " << output_total_count 
//		 << " outputindex " << outputindex << endl;

//	    assert(outputindex + datasize <= OUTBUF_SIZE);
	}
	else { 
	    fill_outbuf (outputbuf+outputindex, ii, buf, size);
	    outputindex += datasize;
	    output_total_count += datasize;
	}
	
    }
#else
    if (pass_filter || filter_outputs()) {
//	fprintf(stderr, "mark_outputs, ii is: %d, size %d, buf %p\n", 
//		ii->type, size, buf);
	write_output_header(ii, buf, size); 
	write_output_nodes(buf, size);
    }
#endif
}
