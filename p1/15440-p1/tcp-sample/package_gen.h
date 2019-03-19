#ifndef _PKTGEN_H
#define _PKTGEN_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>


#define MAX_PKT_SIZE 1600 // need to match at server side 
#define MAX_PATHNAME 200

char* open_package_gen(const char *pathname, int flags, mode_t);
char* close_package_gen(int fd);
char* write_package_gen(int fd, void* buffer, int nbyte);
char* read_package_gen(int fd,int nbyte);
char* lseek_package_gen(int fd, off_t offset, int whence);
char* unlink_package_gen(const char* pathname);
char* __xstat_package_gen(int ver, const char* path);
char* stat_package_gen(const char* path);
char* getdirentries_package_gen(int fd, size_t nbytes, off_t basep);
char* getdirtree_package_gen(const char *path);
#endif
