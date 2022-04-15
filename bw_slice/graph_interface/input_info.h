struct syscall_input {
    int32_t offset;
    int32_t fileno;
};

struct imm_info { 
    u_long ip;
    u_long padding; //to make it same size as input_info! 
};


struct input_info { 
    int32_t record_pid;
    uint64_t rg_id;
    uint32_t syscall_cnt;    
    uint32_t taint_num_start; 
    uint32_t size;       

    //want a larger list here
    enum type{SYSCALL, IMM} type;     
    union { 
	struct syscall_input syscall_info;
	struct imm_info      imm_info;
    };
};
