#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "xray_token.h"

int main(int argc, char** argv)
{
    int fd;
    int rc;
    struct stat buf;
    off_t count = 0;

    fd = open(argv[1], O_RDONLY);
    if (fd <= 0) {
        fprintf(stderr, "could not open %s, %d\n", argv[1], fd);
        return 1;
    }

    rc = fstat(fd, &buf);
    if (rc < 0) {
        fprintf(stderr, "could not stat byte result file, %d\n", fd);
        return -1;
    }

    if (buf.st_size % (sizeof(struct byte_result)) != 0) {
        fprintf(stderr, "size of file is %lu\n", (unsigned long) buf.st_size);
        fprintf(stderr, "size of byte result is %u\n", sizeof(struct byte_result));
        assert (buf.st_size % (sizeof(struct byte_result)) == 0);
    }
    assert (buf.st_size % (sizeof(struct byte_result)) == 0);

    while (count < buf.st_size) {
        struct byte_result* result;
        result = (struct byte_result *) malloc(sizeof(struct byte_result));
        assert(result);
        if (read_byte_result_from_file(fd, result)) {
            fprintf(stderr, "could not read byte_result, count is %lu\n", count);
        }
        fprintf(stdout, "%d %d %llu %d %d %d %d %c\n",
                result->output_type,
                result->output_fileno,
                result->rg_id,
                result->record_pid,
                result->syscall_cnt,
                result->offset,
                result->token_num,
		result->value);
        free(result);
        count += sizeof(struct byte_result);
    }
    return 0;
}
