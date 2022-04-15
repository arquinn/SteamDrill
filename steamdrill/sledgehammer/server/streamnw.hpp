#ifndef __STREAMNW_H__
#define __STREAMNW_H__

long init_socket (int port);
long connect_to_host(const char *hostname, int port);
long safe_read (int s, void* buf, u_long size);
long safe_write (int s, void* buf, u_long size);
long fetch_file (int s, const char* output_prefix, const char *output_postfix);
long send_file (int s, const char* pathname, const char* filename);

#endif
