#define _LARGEFILE64_SOURCE
//doesn't even matter ^^

#include <cstring>

#include "node.h"
//#include "input_output.h"
#include "filters.h"
#include "../bw_slice.h"
#include "../taint_nw.h"
#include "graph_interface.h"

#include <assert.h>
#include <glib-2.0/glib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <vector>
#include <unordered_set>

//#define TRACE_ALL 
//#define TRACE_INFO(x) (x == 0xe001798d || x == 0xe001797d)

extern int splice_output;
extern unsigned long global_syscall_cnt;
extern u_long* ppthread_log_clock;

bool nodeids_initialized = false; //indicates that starting taints have been handled already
nodeid* mem_root[ROOT_TABLE_COUNT];  // Top-level table for memory taints

#ifdef TAINT_STATS
struct taint_stats_profile tsp;
#endif

struct graph g;
nodeid taint_num; 
int node_num_fd; 
uint64_t instcount = 0;

unordered_set<nodeid> starting_reg_nodeids; 

vector<nodeid> ending_nodeids; 
vector<nodeid>::iterator ending_nodeid_it; //prolly need an iterator
bool ending_nodeids_initialized = false;

unsigned long find_static_address(ADDRINT ip)
{
	PIN_LockClient();
	IMG img = IMG_FindByAddress(ip);
	if (!IMG_Valid(img)) return ip;
	ADDRINT offset = IMG_LoadOffset(img);
	PIN_UnlockClient();
	return ip - offset;
}


void flush_graph_nodes(void)
{
#ifdef USE_SHMEM
    if (instcount == 17217870) 
	cerr << "huh... flushing @ " << instcount << endl;
    // Check for overflow
    if ((g.total_count-NODE_OFFSET) >= MAX_MERGES) {
	fprintf (stderr, "Cannot allocate any more merges than %ld\n", 
		 (u_long) (g.total_count-NODE_OFFSET));
	fprintf(stderr,"sycall_cnt %ld clock %ld\n", 
		global_syscall_cnt, *ppthread_log_clock);
	assert (0);
    }

    // Unmap the current region
    if (munmap (g.nodes, MERGE_FILE_CHUNK) < 0) {
	fprintf (stderr, "could not munmap merge buffer, errno=%d\n", 
		 errno);
	assert (0);
    }

    // Map in the next region
//    g.nodes = (node_t*) mmap64 (0,  MERGE_FILE_CHUNK, PROT_READ|PROT_WRITE, 
//				MAP_SHARED, node_num_fd, 
//				(g.total_count-NODE_OFFSET) * sizeof(node_t));

    off_t page_offset = ((g.total_count - NODE_OFFSET) * sizeof(node_t)) >> 12;

    g.nodes = (node_t*) syscall(SYS_mmap2, 0,  MERGE_FILE_CHUNK, PROT_READ|PROT_WRITE, 
				MAP_SHARED, node_num_fd, page_offset);



	

    if (g.nodes == MAP_FAILED) {
	fprintf (stderr, "could not map merge buffer, errno=%d\n", errno);
	assert (0);
    }
    g.buffer_count = 0;



#else
    long bytes_written = 0;
    long size = g.buffer_count*sizeof(node_t);
    
    while (bytes_written < size) {
	long rc = write (node_num_fd, (char *) g.nodes + bytes_written, 
			 size-bytes_written);	
	if (rc <= 0) {
	    fprintf (stderr, 
		     "Canot write to merge log, rc=%ld, errno=%d\n", 
		     rc, errno);
	    assert (0);
	}
	bytes_written += rc;
    }
    g.buffer_count = 0;
#endif
}



void init_graph(char* group_dir)
{
#ifdef USE_MERGE_HASH
    memset(&simple_hash,0,sizeof(simple_hash));
#endif

#ifdef TAINT_STATS
    memset(&tsp, 0, sizeof(tsp));
#endif

#ifdef USE_SHMEM
    char node_num_shmemname[256];
    int rc;
    u_int i;
    
    snprintf(node_num_shmemname, 256, "/node_nums_shm%s", group_dir);
    for (i = 1; i < strlen(node_num_shmemname); i++) {
	if (node_num_shmemname[i] == '/') node_num_shmemname[i] = '.';
    }
    node_num_fd = shm_open(node_num_shmemname, O_TRUNC | O_RDWR | O_LARGEFILE | O_CREAT, 0644);

    if (node_num_fd < 0) {
	fprintf(stderr, "could not open node num shmem %s, errno %d\n",
		node_num_shmemname, errno);
	assert(0);
    }
    rc = ftruncate64 (node_num_fd, MAX_MERGE_SIZE);
    if (rc < 0) {
	fprintf(stderr, "could not truncate shmem %s, errno %d\n",
		node_num_shmemname, errno);
	assert(0);
    }
    g.nodes = (node_t *) mmap64 (0, MERGE_FILE_CHUNK, PROT_READ|PROT_WRITE, MAP_SHARED, 
			       node_num_fd, 0);
    
    if (g.nodes == MAP_FAILED) {
	fprintf (stderr, "could not map merge buffer, errno=%d\n", errno);
	assert (0);
    }

#else
    char node_num_filename[256];
    snprintf(node_num_filename, 256, "%s/node_nums", group_dir);
    node_num_fd = open(node_num_filename, O_CREAT | O_TRUNC | O_RDWR | O_LARGEFILE, 0644);
    if (node_num_fd < 0) {
	fprintf(stderr, "could not open node num file %s, errno %d\n",
 		node_num_filename, errno);
	assert(0);
    }
    g.nodes = (node_t *) malloc(MERGE_FILE_CHUNK);
    if (g.nodes == NULL) {
	fprintf (stderr, "Cannnot allocate file write buffer\n");
	assert (0);
    }

#endif
//    init_slab_allocs();
//    new_slab_alloc((char *)"LEAF_TABLE_ALLOC", &leaf_table_alloc, 
//		   LEAF_TABLE_SIZE * sizeof(taint_t), 10000);

    //init regardless of type 
    g.buffer_count = 0;
    g.total_count = NODE_OFFSET;


    if (splice_output) {
	taint_num = LOCAL_SOURCE_OFFSET;
    } else {
	taint_num = 0x1;
    }
    memset(mem_root, 0, ROOT_TABLE_COUNT * sizeof(nodeid *));
}

void initialize_starting_nodeids(FILE *file)
{
    //important, acknowledge that we've initialized already
    assert(file);
    
    nodeids_initialized = true; 
    nodeid *leaf = NULL;
    uint32_t leaf_start = 0; 
    uint32_t index = 0;
    uint32_t ASsize = 0;
    char line[256];

    while (!feof(file)) {
	if (fgets (line, 255, file)) {
	    uint32_t mem_loc = strtoul(line, NULL, 10);
	    if (mem_loc < LOWEST_MEM_ADDR) {
		starting_reg_nodeids.insert(mem_loc); 
		continue;
	    }
	    assert (mem_loc >= leaf_start);

	    if (!leaf || (mem_loc >> LEAF_TABLE_BITS) != index) { 
		index = mem_loc >> LEAF_TABLE_BITS;
		assert(ROOT_TABLE_COUNT > index); 
		leaf = new_leaf_table(mem_loc);
		mem_root[index] = leaf;
	    }

	    leaf[mem_loc & LEAF_INDEX_MASK] = mem_loc; 
	    ASsize++;
	}
    }

    fseek(file, 0, SEEK_SET); //restart the input file

    while (!feof(file)) {
	if (fgets (line, 255, file)) {
	    uint32_t mem_loc = strtoul(line, NULL, 10);
	    //skip the little guise
	    if (mem_loc < LOWEST_MEM_ADDR) 
		continue; 

	    get_mem_nodes(mem_loc, 1, &leaf);
	    if (!leaf) 
		cerr << "oops... " << mem_loc << " has no leaf? \n";
	    assert(leaf);
	    if (leaf[0] != mem_loc) { 
		cerr << "oops... " << mem_loc << " has value " << leaf[0] << endl;
	    }
	    assert(leaf && leaf[0] == mem_loc);
	}
    }
    

    cerr << "init starting as size " << ASsize << endl;
}

void initialize_endingAS(FILE *file)
{
    //important, acknowledge that we've initialized already

    ending_nodeids_initialized = true;
    while (!feof(file)) {
	char line[256];
	if (fgets (line, 255, file)) {
	    uint32_t mem_loc = strtoul(line, NULL, 10);
	    ending_nodeids.push_back(mem_loc);
	}
    }

    

    cerr << getpid() << " added " << ending_nodeids.size() << " ending nodes\n";
    ending_nodeid_it = ending_nodeids.begin();
    cerr << "first ending_node " << *ending_nodeid_it << endl; 
}


void initialize_startingReg(int thread_ndx, struct thread_data *ptdata) 
{
    int base = (NUM_REGS*REG_SIZE)*thread_ndx;


    for (int i = 0; i < NUM_REGS * REG_SIZE; i++) {
	if (!nodeids_initialized && splice_output)
	    ptdata->shadow_reg_table[i] = base+i+1; // For small number of threads, not part of AS

	else if (starting_reg_nodeids.count(base+i+1))
	    ptdata->shadow_reg_table[i] = base+i+1; // For small number of threads, not part of AS
    
    }
}


void finish_and_print_graph_stats(FILE* fp)
{
#ifdef USE_SHMEM
    uint64_t truncate = ((uint64_t)g.total_count-NODE_OFFSET)*sizeof(node_t);
    int rc = ftruncate64 (node_num_fd, truncate);
    
    if (rc < 0) {
	cerr << "ftrunacte of merge file failed,rc="<<rc<<",errno="<<errno<<endl;
    }

    close (node_num_fd);
#else
    flush_graph_nodes ();
#endif

#ifdef TAINT_STATS
#ifdef DIFT    
    fprintf(fp, "Taint statistics:\n");
#else
    fprintf(fp, "Slice statistics:\n");
#endif
    fprintf(fp, "Second tables allocated: %lu\n", tsp.num_second_tables);
    fprintf(fp, "Num merges:              %lu\n", tsp.merges);
    fprintf(fp, "Num merges saved:        %lu\n", tsp.merges_saved);
    fprintf(fp, "Num copies:              %lu\n", tsp.copies);
    fprintf(fp, "taint_num:               %llu\n",  taint_num);  //we've created this many taints (minus 1)
    fprintf(fp, "total count:             %llu\n",  g.total_count);
    fprintf(fp, "immvals:                 %lu\n", tsp.immvals);
    fprintf(fp, "immfiltered:             %lu\n", tsp.imm_filtered);
    fprintf(fp, "instcount:               %llu\n", instcount);
    fflush(fp);
#endif

}

nodeid get_next_nodeid() { 
    nodeid t = taint_num++; 
    if (!filter_nodeid(t))
    {
	t = NULL_NODE;
    }
    return t; 
}



bool create_node(u_long mem_addr, u_int size)
{
    u_long offset = 0,count;
    nodeid t = get_next_nodeid();
    
    while (offset < size) { 
	count = set_mem_nodes_repeat(mem_addr + offset, size - offset, t);
	offset += count;
    }

    return (t != NULL_NODE);
}


bool create_node_reg(int reg, u_int size, u_int offd) { 
    nodeid t = get_next_nodeid();

    set_reg_nodes_repeat(reg, offd, size, t);
    return t != NULL_NODE;
}


int structures_fd = -1;
#define DUMPBUFSIZE 0x100000
static addrstruct* dumpbuf = NULL; 
static u_long dumpindex = 0;
static u_long dump_total_count = 0;

static void flush_dumpbuf()
{
    dump_total_count += dumpindex * sizeof(struct addrstruct);

    // Check for overflow
    if (dump_total_count >= MAX_DUMP_SIZE) {
	fprintf (stderr, "Cannot allocate any more dump buffer than %lu bytes\n", 
		 (u_long) dump_total_count);
	assert (0);
    }

    // Unmap the current region
    if (munmap (dumpbuf, DUMPBUFSIZE*sizeof(struct addrstruct)) < 0) {
	fprintf (stderr, "could not munmap dump buffer, errno=%d\n", errno);
	assert (0);
    }

    // Map in the next region
    dumpbuf = (struct addrstruct *) mmap (0, DUMPBUFSIZE*sizeof(struct addrstruct), 
					  PROT_READ|PROT_WRITE, MAP_SHARED, 
					  structures_fd, dump_total_count);
    if (dumpbuf == MAP_FAILED) {
	fprintf (stderr, "could not map dump buffer, errno=%d\n", errno);
	assert (0);
    }
    dumpindex = 0;
}


static inline void dump_value (u_long addr, nodeid nid)
{
    if (dumpindex == DUMPBUFSIZE) flush_dumpbuf();

    struct addrstruct ds; 
    ds.addr = addr;
    ds.nid = nid;
    dumpbuf[dumpindex++] = ds; 
}

void init_dump(char *group_dir)
{
    char structures_file[256];
    snprintf(structures_file, 256, "/ending_as_shm%s", group_dir);
    for (u_int i = 1; i < strlen(structures_file); i++) {
	if (structures_file[i] == '/') structures_file[i] = '.';
    }

    structures_fd = shm_open(structures_file, O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (structures_fd < 0) {
	fprintf(stderr, "could not open taint shmem %s, errno %d\n", structures_file, errno);
	assert(0);
    }
    int rc = ftruncate64 (structures_fd, MAX_DUMP_SIZE);
    if (rc < 0) {
	fprintf(stderr, "could not truncate shmem %s, errno %d\n", structures_file, errno);
	assert(0);
    }
    // Map in the next region
    dumpbuf = (struct addrstruct *) mmap (0, DUMPBUFSIZE*sizeof(struct addrstruct), 
					  PROT_READ|PROT_WRITE, MAP_SHARED, 
					  structures_fd, dump_total_count);
    if (dumpbuf == MAP_FAILED) {
	fprintf (stderr, "could not map dump buffer, errno=%d\n", errno);
	assert (0);
    }
    dumpindex = 0;
}

void finish_dump()
{
    if (ftruncate (structures_fd, dump_total_count+(dumpindex*sizeof(struct addrstruct)))) {
	fprintf (stderr, "Cound not truncate dump mem to %ld\n", 
		 dump_total_count*sizeof(struct addrstruct));
	assert (0);
    }
    close (structures_fd);
}
void dump_reg_taints(nodeid *regs, int thread_ndx) 
{
    u_long i;
    u_long base = thread_ndx*(NUM_REGS*REG_SIZE);

    // Increment by 1 because 0 is reserved for "no taint"
    for (i = 0; i < NUM_REGS*REG_SIZE; i++) {

	if (ending_nodeids_initialized &&
	    base+i+1 == *ending_nodeid_it && 
	    regs[i] != base+i+1) {
	    
	    dump_value(base+i+1, regs[i]);
	    
//	    while (*ending_nodeid_it < base+i+2) {
		++ending_nodeid_it; 
//	    }
	}
	    
	else if (!ending_nodeids_initialized && 
		 regs[i] != base+i+1) {

	    dump_value (base+i+1, regs[i]);
	}
    }
}
void dump_reg_taints_start(nodeid *regs, int thread_ndx) 
{
    u_long i;
    u_long base = thread_ndx*(NUM_REGS*REG_SIZE);

    // Increment by 1 because 0 is reserved for "no taint"
    for (i = 0; i < NUM_REGS*REG_SIZE; i++) {

	if (ending_nodeids_initialized &&
	    base+i+1 == *ending_nodeid_it &&
	    regs[i]) {

//	    cerr << hex << "found ending addr " 
//		 << base+i+1 << " " << regs[i] << endl;

	    dump_value(base+i+1, regs[i]);
	    
//	    while (*ending_nodeid_it < base+i+2) {
		++ending_nodeid_it; 
//	    }
	}

	else if (!ending_nodeids_initialized &&
		 regs[i]) {
	    dump_value (base+i+1, regs[i]);
	}
    }
}


void dump_mem_taints()
{
    u_long addr;
    int index, low_index;

    for (index = 0; index < ROOT_TABLE_COUNT; index++) {
	nodeid* leaf = mem_root[index];
	if (leaf) {
	    for (low_index = 0; low_index < LEAF_TABLE_COUNT; low_index++) {
		addr = (index<<LEAF_TABLE_BITS) + low_index;

		if (ending_nodeids_initialized &&
		    addr == *ending_nodeid_it &&
		    leaf[low_index] != addr) {

		    dump_value (addr, leaf[low_index]);
	    
//		    while (*ending_nodeid_it < addr) {
			++ending_nodeid_it; 
//		    }
		}		

		else if (!ending_nodeids_initialized &&
			 leaf[low_index] != addr) {
		    dump_value (addr, leaf[low_index]);
		}
	    }
	}
    }
}


void dump_mem_taints_start()
{
    u_long addr;
    int index, low_index;

    for (index = 0; index < ROOT_TABLE_COUNT; index++) {
	nodeid* leaf = mem_root[index];
	if (leaf) {
	    for (low_index = 0; low_index < LEAF_TABLE_COUNT; low_index++) {
		addr = (index<<LEAF_TABLE_BITS) + low_index;

		
		if (ending_nodeids_initialized &&
		    addr == *ending_nodeid_it &&
		    leaf[low_index]) {

		    dump_value (addr, leaf[low_index]);
	    
		    ++ending_nodeid_it; 

		}		

		else if (!ending_nodeids_initialized &&
			 leaf[low_index])
		    dump_value (addr, leaf[low_index]);
	    }
	}
    }
}

#ifdef VERBOSE
void debug_dump_mem()
{
    u_long addr;
    int index, low_index;

    for (index = 0; index < ROOT_TABLE_COUNT; index++) {
	nodeid* leaf = mem_root[index];
	if (leaf) {
	    for (low_index = 0; low_index < LEAF_TABLE_COUNT; low_index++) {
		addr = (index<<LEAF_TABLE_BITS) + low_index;
	    
		if (debug_set.count(leaf[low_index])) { 
		    log_f << addr << " id is " << leaf[low_index] << endl;
		}
	    }
	}
    }
}
#endif
