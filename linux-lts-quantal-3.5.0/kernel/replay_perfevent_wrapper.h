#ifndef REPLAY_PERF_EVENT_WRAPPER_H
#define REPLAY_PERF_EVENT_WRAPPER_H

#include <linux/perf_event.h>

#define PERF_OUTBUF_ENTRIES 10000
#define BUFFER_SIZE 512

struct perfevent_sampler {
	int perf_fd;
	int overflow_count;
	u_long num_syscalls;
	u_int data_size;
	int  bufcnt; //index within above buffer

	char *logdir; //pointer to the log_dir for this wrapper's replay
	__u32 *outbuf; //pointer to buffer of things to write to file
	loff_t outpos; //index within our output file

	struct perf_event_mmap_page *mapping;
};


int init_perfevent_sampler(struct perfevent_sampler *wrapper,
			     char *logdir,
			     unsigned int sample_type,
			     unsigned int  config,
			     unsigned int sample_period,
			     unsigned int data_size);

void destroy_perfevent_sampler(struct perfevent_sampler *wrapper);
void perfevent_sampler_start(struct perfevent_sampler *wrapper);
void perfevent_sampler_stop(struct perfevent_sampler *wrapper);
void perfevent_sampler_iterate(struct perfevent_sampler *wrapper);


struct perfevent_counter {
        int perf_fd;
};

int init_perfevent_counter(struct perfevent_counter**,
                           unsigned int counter_type,
                           unsigned int counter_value);

void destroy_perfevent_counter(struct perfevent_counter*);

void perfevent_counter_start(struct perfevent_counter*);
void perfevent_counter_stop(struct perfevent_counter*);
long long perfevent_counter_getval(struct perfevent_counter*);
#endif /* REPLAY_PERF_EVENT_WRAPPER_H */
