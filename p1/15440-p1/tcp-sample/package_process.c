#include "package_process.h"
#include "stdio.h"
#include "stdlib.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "math.h"
#include "../include/dirtree.h"
#define MAX_PKT_SIZE 1600
#define MAX_PATHNAME 200
#define rpc_fd_offset 1000

/*Global buffer used in the getdirtree syscall*/
char *buffer_pathname = NULL;

char* return_package_gen(int val, int err_number) {
	char* pkt = (char*)malloc(MAX_PATHNAME);
	sprintf(pkt,"RETURN:%d, ERRNO:%d\n", val, err_number);
	return pkt;
}

char* return_PL_package_gen(int val, int err_number, int size, char* payload) {
	char* pkt = (char*)malloc(MAX_PKT_SIZE);
	sprintf(pkt,"RETURN:%d, ERRNO:%d\n", val, err_number);
	memcpy(pkt+strlen(pkt), payload,size);
	return pkt;
}

int open_package(char* buf, int* err) {
    //var def
    char filename[MAX_PATHNAME]; 
	char buffer[MAX_PATHNAME]; 
	memset(buffer,0,MAX_PATHNAME);
	memset(filename,0,MAX_PATHNAME);
	int returnVal = 0;
	mode_t mode = 0;
	int flags;
	char* indexStart = 0;
	char* indexEnd = 0;

	// get filename
	indexStart = strchr(buf,' ')+1;
	indexEnd = strchr(indexStart, ' ');
	strncpy(buffer, indexStart, (int)(indexEnd-indexStart));
	strcpy(filename,buffer); 
	filename[(int)(indexEnd-indexStart)] = '\0';

	// get flags;
	memset(buffer,0,MAX_PATHNAME);
	indexStart = indexEnd+1;
	indexEnd = strchr(indexStart, ' ');
	strncpy(buffer, indexStart,(int)(indexEnd-indexStart));
	flags = atoi(buffer);

	// get mode;
	memset(buffer,0,MAX_PATHNAME);
	indexStart = indexEnd+1;
	indexEnd = strchr(indexStart, '\n');
	strncpy(buffer, indexStart,(int)(indexEnd-indexStart));
	mode = atoi(buffer);
	if( (flags & O_CREAT) && access(filename,F_OK) == -1)
		returnVal = open(filename,flags,mode);
	else
		returnVal = open(filename,flags);

	*err = errno;
	return returnVal;
}

int close_package(char* buf,int* err) {

	char buffer[MAX_PATHNAME]; 
	memset(buffer,0,MAX_PATHNAME);
	int fd;
	char* indexStart = 0;
	char* indexEnd = 0;
	int return_val = 0;

	// get file descriper
	indexStart = strchr(buf,' ')+1;
	indexEnd = strchr(indexStart, '\n'); 
	strncpy(buffer, indexStart, (int)(indexEnd-indexStart));
	fd = atoi(buffer);
	return_val = close(fd);
	*err = errno;
	return return_val;
}


int write_package(char* buf, int* err) {
	char buffer[MAX_PATHNAME]; // tmp buffer
	char content[MAX_PKT_SIZE]; 
	memset(buffer,0,MAX_PATHNAME);
	int return_val = 0;
	int fd;
	int size = 0;
	char* indexStart = 0;
	char* indexEnd = 0;

	// get file descripter
	indexStart = strchr(buf,' ')+1;
	indexEnd = strchr(indexStart, ' ');
	strncpy(buffer,indexStart, indexEnd-indexStart);
	fd = atoi(buffer); 
	memset(buffer,0,MAX_PATHNAME);

	// get write input size
	indexStart = indexEnd+1;
	indexEnd = strchr(indexStart, '\n');
	strncpy(buffer,indexStart, indexEnd-indexStart);
	size = atoi(buffer); 
	memset(buffer,0,MAX_PATHNAME);

	// get content
	indexStart = indexEnd+1;
	memcpy(content,indexStart,size);
	return_val = write(fd,content,size);
	*err = errno;
	return return_val;
}

int read_package(char* buf, int* err, int* payloadSize, char** content) {
	char buffer[MAX_PATHNAME]; 
	memset(buffer,0,MAX_PATHNAME);
	*content = (char*)malloc(MAX_PKT_SIZE);
	//content is a pointer (a reference) to char*, which can store value from read() at server 
	int size = 0;
	// size is the read size it should read, which is gived by client
	char* indexStart = 0;
	char* indexEnd = 0;
	int return_val = 0;
	int fd;
	
	// get file descripter
	indexStart = strchr(buf,' ')+1;
	indexEnd = strchr(indexStart, ' ');
	strncpy(buffer,indexStart, indexEnd-indexStart);
	fd = atoi(buffer); 
	memset(buffer,0,MAX_PATHNAME);

	// get file size
	indexStart = indexEnd+1;
	indexEnd = strchr(indexStart, '\n');
	strncpy(buffer,indexStart, indexEnd-indexStart);
	size = atoi(buffer); 
	memset(buffer,0,MAX_PATHNAME);

	// get content from file
	return_val = read(fd,*content,size);

	// set errno and payload size (note return_val might be -1, set payloadsize == 0 in this case)
	if(return_val == -1)
		*payloadSize = 0;
    else
    	*payloadSize = return_val;
	*err = errno;
	return return_val;
}

int lseek_package(char* buf, int* err) {
	char buffer[MAX_PATHNAME]; 
	memset(buffer,0,MAX_PATHNAME);
	int return_val = 0;
	int fd;
	off_t offset;
	int whence = 0;
	char* indexStart = 0;
	char* indexEnd = 0;


	// get file descripter
	indexStart = strchr(buf,' ')+1;
	indexEnd = strchr(indexStart, ' ');
	strncpy(buffer,indexStart, indexEnd-indexStart);
	fd = atoi(buffer); memset(buffer,0,MAX_PATHNAME);

	// get file offset
	indexStart = indexEnd+1;
	indexEnd = strchr(indexStart, ' ');
	strncpy(buffer,indexStart, indexEnd-indexStart);
	offset = atoi(buffer); memset(buffer,0,MAX_PATHNAME);

	// get glag
	indexStart = indexEnd+1;
	indexEnd = strchr(indexStart, '\n');
	strncpy(buffer,indexStart, indexEnd-indexStart);
	whence = atoi(buffer); memset(buffer,0,MAX_PATHNAME);

	// set payload size and err size
	return_val = lseek(fd,offset,whence);
	*err = errno;

	return return_val;
}

int unlink_package(char* buf, int* err) {
	char buffer[MAX_PATHNAME]; 
	memset(buffer,0,MAX_PATHNAME);
	char* indexStart = 0;
	char* indexEnd = 0;
	int return_val = 0;


	// get filename
	indexStart = strchr(buf,' ')+1;
	indexEnd = strchr(indexStart, '\n');
	strncpy(buffer, indexStart, (int)(indexEnd-indexStart));
	buffer[strlen(buffer)] = '\0';
	return_val = unlink(buffer);
	*err = errno;
	return return_val;
}

int stat_package(char* buf, int* err, int* payloadSize, char** payload) {
	char* indexStart = 0;
	char* indexEnd = 0;
	int reVal = 0;
	char buffer[MAX_PATHNAME]; // maximum file name
	memset(buffer,0,MAX_PATHNAME);
	struct stat* file_stat = malloc(sizeof(struct stat));
	*payload = (char*)malloc(sizeof(struct stat));

	// get filename
	indexStart = strchr(buf,' ')+1;
	indexEnd = strchr(indexStart, '\n');
	strncpy(buffer, indexStart, (int)(indexEnd-indexStart));
	buffer[strlen(buffer)] = '\0';
	
	// get stat
	reVal = stat(buffer,file_stat);

	// copy the bytes from structure to buffer
	memcpy(*payload,file_stat,sizeof(struct stat));
	*payloadSize = sizeof(file_stat);
	*err = errno;
	free(file_stat);
	return reVal;
}

int __xstat_package(char* buf, int* err, int* payloadSize, char** payload) {
	char* indexStart = 0;
	char* indexEnd = 0;
	int ver = 0;
	int reVal = 0;
	char filename[MAX_PATHNAME] = {0};
	char tmp[MAX_PATHNAME] = {0}; 
	memset(filename,0,MAX_PATHNAME);
	struct stat* file_stat = malloc(sizeof(struct stat));
	*payload = (char*)malloc(sizeof(struct stat));

	// get filename
	indexStart = strchr(buf,' ')+1;
	indexEnd = strchr(indexStart, ' ');
	strncpy(filename, indexStart, (int)(indexEnd-indexStart));
	filename[strlen(filename)] = '\0';

	// get version
	indexStart = indexEnd+1;
	indexEnd = strchr(indexStart, '\n');
	strncpy(tmp, indexStart, (int)(indexEnd-indexStart));
	tmp[strlen(tmp)] = '\0';
	ver = atoi(tmp);

	// get __xstat
	reVal = __xstat(ver,filename,file_stat);

	// copy the bytes from structure to buffer
	memcpy(*payload,file_stat,sizeof(struct stat));
	*payloadSize = sizeof(struct stat);
	*err = errno;
	free(file_stat);

	// reVal is zero is success, and reval is -1 if err, 
    // we need to change the reVal to represent the payload size for our own need!!
    // (recover the correct return val at client side)
    if(reVal != -1){
    	reVal = *payloadSize;
    }
	return reVal;
}

int gendirentries_package(char* buf, int* err, int* payloadSize, char** payload) {
	char* indexStart = NULL;
	char* indexEnd = NULL;
	int reVal = 0;
	int fd = 0;
	off_t *basep;
   
	size_t nbytes = 0;
	char buffer[MAX_PATHNAME]; 
	memset(buffer,0,MAX_PATHNAME);
	*payload = (char*)malloc(MAX_PKT_SIZE);
	memset(*payload,0,MAX_PKT_SIZE);

	// get fd
	indexStart = strchr(buf,' ')+1;
	indexEnd = strchr(indexStart, ' ');
	strncpy(buffer, indexStart, (int)(indexEnd-indexStart));
	buffer[strlen(buffer)] = '\0';
	fd = atoi(buffer); memset(buffer,0,MAX_PATHNAME);

	// get nbytes
	indexStart = indexEnd + 1;
	indexEnd = strchr(indexStart, ' ');
	strncpy(buffer, indexStart, (int)(indexEnd-indexStart));
	buffer[strlen(buffer)] = '\0';
	nbytes = atoi(buffer); memset(buffer,0,MAX_PATHNAME);

	// get base position
	indexStart = indexEnd + 1;
	indexEnd = strchr(indexStart, '\n');
	strncpy(buffer, indexStart, (int)(indexEnd-indexStart));
	buffer[strlen(buffer)] = '\0';
	//
	basep = malloc(sizeof(atoi(buffer)));
	*basep = atoi(buffer); 
    fprintf(stderr,"server: fd is %d, nbytes is %d, *basep is %d \n", fd,(int)nbytes, (int)*basep);
	// get direntries
	reVal = getdirentries(fd, *payload, nbytes,basep);
	fprintf(stderr,"server: reVal in getdirentries is %d \n", reVal);
	if(reVal == -1){
		*payloadSize = 0;
	} else {
		*payloadSize = reVal;
	}
	*err = errno;
    free(basep);
	return reVal;
}

void create_buffer(struct dirtreenode *root){
	static int size_count = 0; 
	int num_len = (root->num_subdirs == 0 ? 1 : (int)(log10(root->num_subdirs)+1));
	size_count += strlen(root->name) + 3 + num_len;
	char *tempbuf = malloc(strlen(root -> name) + 3 + num_len);
	buffer_pathname = realloc(buffer_pathname,size_count);
	sprintf(tempbuf, "%s\n%d\n",root->name, root->num_subdirs);
	strcat(buffer_pathname,tempbuf);
	if(root -> num_subdirs > 0){
		int i;
		for(i = 0; i < root->num_subdirs; i++){
			create_buffer(root->subdirs[i]);
		}
	}
	return;		
}

int dirtree_package(char* buf, int* err, int* payloadSize, char** payload) {
	char* indexStart = 0;
	char* indexEnd = 0;
	int reVal = 0;
	char buffer[MAX_PATHNAME];
	memset(buffer,0,MAX_PATHNAME);
	struct dirtreenode* root;
	*payload = (char*)malloc(MAX_PKT_SIZE);
	memset(*payload,0,MAX_PKT_SIZE);


	// get filename
	indexStart = strchr(buf,' ')+1;
	indexEnd = strchr(indexStart, '\n');
	strncpy(buffer, indexStart, (int)(indexEnd-indexStart));
	buffer[strlen(buffer)] = '\0';

	// call getdirtree
	root = getdirtree(buffer);
	if (root == NULL) {
		reVal = -1;
		*err = errno;
		printf("gendirtree fail at server\n");
		return reVal;
	}
	buffer_pathname = malloc(sizeof(char));
	buffer_pathname[0] = '\0';
	create_buffer(root);

	// copy the bytes from structure to buffer
	memcpy(*payload,buffer_pathname,strlen(buffer_pathname));
    *payloadSize = strlen(buffer_pathname);
    reVal = strlen(buffer_pathname);
	*err = errno;
	free(buffer_pathname);
	return reVal;
}

