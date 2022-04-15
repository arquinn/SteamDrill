#include "node.h"

#ifndef _FILTERS_H_
#define _FILTERS_H_



int filter_inputs (char *channel_name, u_long ip);
int filter_outputs (void);
int filter_slice(void);
bool filter_nodeid(nodeid id);

void add_input_filters(FILE* file);
void add_taint_num_filters(FILE *file);
void add_output_filters (u_long before);
void dump_input_filters(void);

#endif
