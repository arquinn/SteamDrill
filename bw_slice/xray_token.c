#include "xray_token.h"

struct token* create_new_token (int type, u_long token_num, u_long size, int syscall_cnt, int byte_offset, uint64_t rg_id, int record_pid, int fileno)
{
    struct token* tok; 
    tok = (struct token *) malloc(sizeof(struct token));
    if (tok == NULL) {
	fprintf (stderr, "Unable to malloc token\n");
	assert (0);
    }
    memset(tok, 0, sizeof(struct token));
    tok->type = type;
    tok->token_num = token_num;
    tok->size = size;
    tok->syscall_cnt = syscall_cnt;
    tok->byte_offset = byte_offset;
    tok->rg_id = rg_id;
    tok->record_pid = record_pid;
    tok->fileno = fileno;

    return tok;
}

void set_new_token (struct token* tok, int type, u_long token_num, u_long size, int syscall_cnt, int byte_offset, uint64_t rg_id, int record_pid, int fileno, int ip)
{
    tok->type = type;
    tok->token_num = token_num;
    tok->size = size;
    tok->syscall_cnt = syscall_cnt;
    tok->byte_offset = byte_offset;
    tok->rg_id = rg_id;
    tok->record_pid = record_pid;
    tok->fileno = fileno;
    tok->ip = ip;
}


void write_token_to_file(int outfd, struct token* token)
{
    int rc;
    int written = 0; 
    while (written < sizeof(struct token)) 
    {
	rc = write(outfd, token, sizeof(struct token));
	if (rc == 0) { 
	    fprintf(stderr, "[ERROR] Could not write token to file, got %d, expected %d, errno %d\n", 
		    rc, sizeof(struct token), errno);	    
	    assert (rc == sizeof(struct token)); // fail
	}
	written += rc; 
    }
}

int read_token_from_file(FILE* fp, struct token* ptoken)
{
    int rc;
    rc = read(fileno(fp), ptoken, sizeof(struct token));
    if (rc != sizeof(struct token)) {
        fprintf(stderr, "[ERROR] Could not read token from file, got %d\n", rc);
        return -1;
    }
    return 0;
}



