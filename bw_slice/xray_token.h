#ifndef XRAY_TOKEN_H
#define XRAY_TOKEN_H

#include <stdlib.h>
#include <sys/file.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib.h>
#include <string.h>
#include <assert.h>
#include "token.h"

#ifdef __cplusplus
extern "C" {
#endif

struct byte_result_header {
    int output_type;
    int output_fileno;
    uint64_t rg_id;
    int record_pid;
    int syscall_cnt;
    int size;
    int ip; 
};


/* Operations for tokens */
struct token* create_new_token (int type, u_long token_num, u_long size, int syscall_cnt, int byte_offset, uint64_t rg_id, int record_pid, int fileno);
    void set_new_token (struct token* tok, int type, u_long token_num, u_long size, int syscall_cnt, int byte_offset, uint64_t rg_id, int record_pid, int fileno, int ip);
const char* get_token_type_string(int token_type);
void write_token_to_file(int outfd, struct token* token);
int read_token_from_file(FILE* fp, struct token* ptoken);



#ifdef __cplusplus
}
#endif

#endif // XRAY_TOKEN_H
