
#ifndef __MAPUTIL_H__
#define __MAPUTIL_H__

int map_file (const char* filename, int* pfd, u_long* pdatasize, u_long* pmapsize, char** pbuf);
void unmap_file (char* buf, int fd, u_long mapsize);

void* map_buffer (const char* prefix, const char* group_directory, u_long& datasize, int& fd);


#endif
