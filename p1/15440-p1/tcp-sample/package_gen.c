#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "package_gen.h"
#define rpc_fd_offset 1000
char* open_package_gen(const char *pathname, int flags, mode_t m) {
	char* pkt = (char*)malloc(MAX_PATHNAME);
	memset(pkt,0,MAX_PATHNAME);
	strcpy(pkt,"OPEN ");
	strcpy(pkt+5,pathname);
	strcat(pkt," ");
	sprintf(pkt+strlen(pkt), "%d", flags);
	strcat(pkt," ");
	sprintf(pkt+strlen(pkt), "%d", m);
	strcat(pkt,"\n\n");
	//printf("OPEN PKT:\n%s\n",pkt);
	return pkt;
}

char* close_package_gen(int fd){
    char* pkt = (char*)malloc(MAX_PATHNAME);
	memset(pkt,0,MAX_PATHNAME);
	strcpy(pkt,"CLOSE ");
	sprintf(pkt+strlen(pkt), "%d", fd);
	strcat(pkt,"\n\n");
	//printf("CLSOE PKT:\n%s\n",pkt);
	return pkt;
}

char* write_package_gen(int fd, void* buffer, int nbyte){
	char* pkt = (char*)malloc(MAX_PKT_SIZE);
	memset(pkt,0,MAX_PKT_SIZE);
	strcpy(pkt, "WRITE ");
	sprintf(pkt+strlen(pkt), "%d", fd);
	strcat(pkt," ");
	sprintf(pkt+strlen(pkt), "%d", nbyte);
	strcat(pkt,"\n");
	// copy content
	memcpy(strlen(pkt)+pkt,buffer,nbyte);
	//printf("WRITE PKT:\n%s\n",pkt);
	return pkt;

}

char* read_package_gen(int fd,int nbyte){
	char* pkt = (char*)malloc(MAX_PKT_SIZE);
	memset(pkt,0,MAX_PKT_SIZE);
	strcpy(pkt,"READ ");
	sprintf(pkt+strlen(pkt), "%d", fd);
	strcat(pkt, " ");
	sprintf(pkt+strlen(pkt), "%d", nbyte);
	strcat(pkt, "\n\n");
	return pkt;

}

char* lseek_package_gen(int fd, off_t offset, int wh) {
	char* pkt = (char*)malloc(MAX_PKT_SIZE);
	memset(pkt,0,MAX_PKT_SIZE);
	strcpy(pkt,"LSEEK ");
	sprintf(pkt+strlen(pkt), "%d", fd);
	strcat(pkt, " ");
	sprintf(pkt+strlen(pkt), "%ld", offset);
	strcat(pkt, " ");
	sprintf(pkt+strlen(pkt), "%d", wh);
	strcat(pkt, "\n\n");
	return pkt;
}

char* unlink_package_gen(const char* pathname) {
	char* pkt = (char*)malloc(MAX_PATHNAME);
	memset(pkt,0,MAX_PATHNAME);
	strcpy(pkt,"UNLINK ");
	strcat(pkt,pathname);
	strcat(pkt, "\n\n");
	return pkt;
}

char* __xstat_package_gen(int ver, const char* path) {
	char* pkt = (char*)malloc(MAX_PKT_SIZE);
	memset(pkt,0,MAX_PKT_SIZE);
	strcpy(pkt,"__XSTAT ");
	strcat(pkt, path);
	strcat(pkt," ");
	sprintf(pkt+strlen(pkt), "%d", ver);
	strcat(pkt, "\n\n");
	return pkt;
}

char* stat_package_gen(const char* path) {
	char* pkt = (char*)malloc(MAX_PKT_SIZE);
	memset(pkt,0,MAX_PKT_SIZE);
	strcpy(pkt,"STAT ");
	strcat(pkt, path);
	strcat(pkt, "\n\n");
	return pkt;
}
char* getdirentries_package_gen(int fd, size_t nbytes, off_t basep) {
	char* pkt = (char*)malloc(MAX_PKT_SIZE);
	memset(pkt,0,MAX_PKT_SIZE);
	strcpy(pkt,"GETDIRENTRIES ");
	sprintf(pkt+strlen(pkt), "%d %ld %ld\n\n", fd, nbytes, basep);
	//printf("GETDIRENTRIES PKT:\n%s\n",pkt);
	return pkt;
}

char* getdirtree_package_gen(const char *path){
    char* pkt = (char*)malloc(MAX_PKT_SIZE);
	memset(pkt,0,MAX_PKT_SIZE);
	strcpy(pkt,"GETDIRTREE ");
	strcat(pkt, path);
	strcat(pkt, "\n\n");
	return pkt;

}