#ifndef _INPUT_OUTPUT_H
#define _INPUT_OUTPUT_H

//need this definition here b/c postprocess uses it as well
enum input_info_type {READ, PREAD, MMAP, WRITE, IMM};

struct syscall_input {
    int32_t fileno;
};

struct imm_info { 
    uint32_t ip;
    uint32_t instcount;    
};

struct input_info { 
    int32_t record_pid;
    uint64_t rg_id;
    uint32_t syscall_cnt;    
    uint32_t nodeid_start; 
    uint32_t size;     
    uint32_t location;
    uint32_t value; //hard to get, should be an array (actually)

  
    enum input_info_type type;
    //want a larger list 
    union { 
	struct syscall_input syscall_info;
	struct imm_info      imm_info;
    };
};

void immval_input_mem(void* buf, int size, uint32_t ip);
void immval_input_reg(int reg, uint32_t size, uint32_t offs,
		      uint32_t ip);
nodeid immval_input_none(uint32_t ip);

void syscall_input(void *buf, uint32_t size, input_info_type type, int read_fileno, 
		   char *channel_name);


void output(void*, int,  struct input_info*, bool);
void instruction_check_output(u_long ip);
void instruction_output(u_long ip, void *location, int size);

#endif
