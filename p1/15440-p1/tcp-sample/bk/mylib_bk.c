#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// #include <unistd.h>
#include <stdarg.h>

#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
//#include <unistd.h>
#include "package_gen.h"
#include "../include/dirtree.h"
#include <err.h>
#include <errno.h>

#define MAXMSGLEN 1600
#define MAX_PATHNAME 200
#define rpc_fd_offset 100000
// The following line declares a function pointer with the same prototype as the open function.
/////////////
int (*orig_open)(const char *pathname, int flags, ...);//mode_t mode is needed when flags includes O_CREAT    
int (*orig_close)(int fd);                               
ssize_t (*orig_read)(int fd, void* buffer, size_t nbyte); 
int (*orig_write)(int fd, void* buffer, int nbyte);
int (*orig_lseek)(int fd, off_t offset, int whence);
int (*orig___xstat)(int ver, const char * path, struct stat * stat_buf);
int (*orig_stat)(const char *path, struct stat *buf);
int (*orig_unlink)(const char *path);
int (*orig_getdirentries)(int fd, char *buf, size_t nbytes, off_t *basep);

struct dirtreenode* (*orig_getdirtree)( const char *path );
void (*orig_freedirtree)( struct dirtreenode* dt );

char *tree_buffer = NULL; //buffer for storing directory data
//////////// helper
void construct_tree(struct dirtreenode** root); //Function used to construct directory tree
void log_to_server(char* function_name); // helper function which send func_name to server

// This function is automatically called when program is started
void _init(void) {
	// set function pointer orig_xxx to point to the original xxx function
	orig_open = dlsym(RTLD_NEXT, "open");
	orig_close = dlsym(RTLD_NEXT, "close");
	orig_read = dlsym(RTLD_NEXT, "read");
	orig_write = dlsym(RTLD_NEXT, "write");
	orig_lseek = dlsym(RTLD_NEXT, "lseek");
	orig_stat = dlsym(RTLD_NEXT, "stat");
	orig___xstat = dlsym(RTLD_NEXT, "__xstat");
	orig_unlink = dlsym(RTLD_NEXT, "unlink");
	orig_getdirentries = dlsym(RTLD_NEXT, "getdirentries");
	orig_getdirtree = dlsym(RTLD_NEXT, "getdirtree");
	orig_freedirtree = dlsym(RTLD_NEXT, "freedirtree");
}


	// send message to server
int send_msg(int sockfd, char* msg) {
	send(sockfd, msg, strlen(msg), 0);
	return 0;
}


int read_msg(int sockfd, int* reVal, int* err) {
	char buffer[MAX_PATHNAME] = {0};
	char tmp[MAX_PATHNAME] = {0};
	int nbyte = 0;
	char* indexStart;
	char* indexEnd;
	
	// read message from server
	if((nbyte = recv(sockfd,buffer,MAX_PATHNAME,0)) > 0) {
		// get return value
		indexStart = strstr(buffer,"RETURN:") + strlen("RETURN:");
		indexEnd = strchr(indexStart,' ');
		strncpy(tmp,indexStart,indexEnd-indexStart); 
		tmp[strlen(tmp)] = '\0';
		*reVal = atoi(tmp);
		memset(tmp,0,MAX_PATHNAME);

		// get errno number
		indexStart = strstr(indexEnd+1,"ERRNO:")+strlen("ERRNO:");
		indexEnd = strchr(indexStart,'\n');
		strncpy(tmp,indexStart,indexEnd-indexStart); 
		*err = atoi(tmp);
	}
	return nbyte;
}

// connect to sever
int connect_server() {
	struct sockaddr_in srv;
	char *serverport;
	char *serverip;
	int sockfd, rv;
	unsigned short port;
	
	
	// Get environment variable indicating the ip address of the server
	serverip = getenv("server15440");
	if (serverip != NULL) {
		//printf("Got environment variable server15440: %s\n", serverip);
	}
	else {
		//printf("Environment variable server15440 not found.  Using 127.0.0.1\n");
		serverip = "127.0.0.1";
	}
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport != NULL) {
		//printf("Got environment variable serverport15440: %s\n", serverport);
	}
	else {
		//printf("Environment variable serverport15440 not found.  Using 15440\n");
		serverport = "15440";
	}
	port = (unsigned short)atoi(serverport);
	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);	// TCP/IP socket
	if (sockfd<0) err(1, 0);			// in case of error
	
	// setup address structure to point to server
	memset(&srv, 0, sizeof(srv));			// clear it first
	srv.sin_family = AF_INET;			// IP family
	srv.sin_addr.s_addr = inet_addr(serverip);	// IP address of server
	srv.sin_port = htons(port);			// server port

	// actually connect to the server
	rv = connect(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
	if (rv<0)  {
		fprintf(stderr,"failed to connect server!\n");
		err(1,0);
	}
	return sockfd;
}

/** brief: repleacement for the open function from libc
 *	param fd, file descriptor pointed to open file
 *  The function returns 0 if successful, -1 to indicate an error, 
 *	with errno set appropriately.
 */
int open(const char *pathname, int flags, ...) {
	//fprintf(stderr,"mylib: open called for path %s\n", pathname);
	mode_t m=0;
	if (flags & O_CREAT) {
		va_list a;
		va_start(a, flags);
		m = va_arg(a, mode_t);
		va_end(a);
	}

	// connect to server 
	int return_val = 0;
	int err = 0;
	int sockid = connect_server();
	// contruct package
	char* pkt = open_package_gen(pathname,flags,m);
	// send request
	send_msg(sockid,pkt); free(pkt);
	// get feedback
	read_msg(sockid,&return_val,&err);
	orig_close(sockid);
	errno = err;
	if(return_val < 0){ // if has error, just return -1
	    return return_val;
	}else{
		return_val = return_val + rpc_fd_offset; // if no error, add offset to seperate rpc fd
	}

	return return_val;
}

/** brief: repleacement for the close function from libc
 *	param fd, file descriptor pointed to opened file
 *  The function returns 0 if successful, -1 to indicate an error, 
 *	with errno set appropriately.
 */
int close(int fd) {
	if(fd < 0){
        errno = 9;
        return -1;
    }

    if(fd < rpc_fd_offset){ // not rpc fd => local fd
    	return orig_close(fd);
    }

    fd = fd - rpc_fd_offset; //rpc fd => get back to original fd (without offset) 
	int err = 0;
	int return_val;
	// connect to server
	int sock = connect_server();
	// contruct pkt
	char* pkt = close_package_gen(fd);
	// send request
	send_msg(sock,pkt); free(pkt);
	// get feedback
	read_msg(sock,&return_val,&err);
	orig_close(sock);
	errno = err;
	//fprintf(stderr,"client: close called, fid:%d\n", fd);
	return return_val;
}

int read_msg_and_payload(int sockfd, int* return_val, int*err, void* p) {

	char* buf = malloc(MAXMSGLEN);
	char* indexStart = NULL;
	char* indexEnd = NULL;
	int nbyte = 0;
	char tmp[20]; 
	
	
	if((nbyte = recv(sockfd,buf,MAXMSGLEN,0)) > 0) {

		// parse return value
		indexStart = strstr(buf,"RETURN:")+strlen("RETURN:");
		indexEnd = strchr(indexStart,' ');
		strncpy(tmp,indexStart, indexEnd-indexStart); tmp[strlen(tmp)] = '\0';
		*return_val = atoi(tmp); 
		memset(tmp,0,20);
        //fprintf(stderr,"client: read msg with load return val is %d\n", *return_val);
		
		// get errno
		indexStart = strstr(indexEnd+1,"ERRNO:")+strlen("ERRNO:");
		indexEnd = strchr(indexStart,'\n');
		strncpy(tmp,indexStart,indexEnd-indexStart);tmp[strlen(tmp)] = '\0';
		*err = atoi(tmp);
		memset(tmp,0,20);
        //fprintf(stderr,"client: read msg with load err is %d\n", *err);
        
		// get content
		indexStart = indexEnd+1;
		//if(*return_val != nbyte) {
		if(MAXMSGLEN != nbyte) {
			//fprintf(stderr,"client: Weird! nbyte diff from client to server!\n");
			fprintf(stderr,"client: Weird! nbyte diff from MAXMSGLE!\n");
		}
			if(*return_val != -1){ // if *return_val is -1 , memcpy will sag fault
                memcpy(p,indexStart,*return_val);
			}
			
	}
	free(buf);
	return nbyte;
}
/** brief: repleacement for the read function from libc
 *	param fd, file descriptor pointed to opened file
 *	param buffer, the space where read content to
 *	param nbyte, the size which is about to read 
 *  The function returns 0 if successful, -1 to indicate an error, 
 *	with errno set appropriately.
 */
int read(int fd, void *buffer, int nbyte) {
	//fprintf(stderr,"client: read called start, fid:%d, bytes: %d\n", fd, nbyte);
	if(fd < 0){errno = 9;return -1;}// return -1 if fd is invalid
	if(nbyte == 0) return 0;// if read 0 byte, return zero

	if(fd < rpc_fd_offset){ // local call -> run local read
    return orig_read(fd, buffer, nbyte);
	}

	fd = fd - rpc_fd_offset; // rpc call  -> get to original fd
	if(nbyte > MAXMSGLEN-MAX_PATHNAME) nbyte = MAXMSGLEN-MAX_PATHNAME;
	// if we cannot read nbyte, just use the max value we can read, 
	// and tell the caller how many bytes we actually read
	
	// init network
	int sock = connect_server();
	int reVal = 0;
	int err = 0;


	// send request
	char* pkt = read_package_gen(fd,nbyte);

	send_msg(sock, pkt); free(pkt);

	read_msg_and_payload(sock, &reVal, &err, buffer);

	orig_close(sock);
	errno = err;
	return reVal;
}

/** brief: repleacement for the write function from libc
 *	param fd, file descriptor pointed to opened file
 *	param buffer, the space where write content to
 *	param nbyte, the size which is about to write 
 */
int write(int fd, void* buffer, int nbyte) {
	//fprintf(stderr,"client: write called start, fid:%d, bytes: %d\n", fd, nbyte);
	if(fd < 0){errno = 9;return -1;}// return -1 if fi is invalid
	if(nbyte == 0) return 0;// if write 0 byte, return zero

	if(fd < rpc_fd_offset){ // local call -> run local read
    return orig_write(fd, buffer, nbyte);
	}

	fd = fd - rpc_fd_offset; // rpc call  -> get to original fd

    if(nbyte > MAXMSGLEN-MAX_PATHNAME) nbyte = MAXMSGLEN-MAX_PATHNAME;
    // if we cannot write nbyte, just use the max value we can write, 
	// and tell the caller how many bytes we actually write
	
	// connect to server
	int sock = connect_server();
	int err = 0;
	int return_val = 0;

	// send request
	char* pkt = write_package_gen(fd,buffer,nbyte);
	send(sock,pkt,nbyte+strchr(pkt,'\n')-pkt+1,0); free(pkt);

	// get feedback
	read_msg(sock,&return_val,&err);

	orig_close(sock);
	errno = err; // set errno
	
	return return_val;
}

/** brief: repleacement for the lseek function from libc
 *	param fd, file descriptor pointed to opened file
 *	param offset, the offsize
 *	param whence, the start position
 *	return the actual bytes that have been moved
 */
int lseek(int fd, off_t offset, int whence){
	if(fd < 0){
        errno = 9;
        return -1;
    }

    if(fd < rpc_fd_offset){ // local call -> run local read
    return orig_lseek(fd, offset, whence);
	}

	fd = fd - rpc_fd_offset; // rpc call  -> get to original fd

    int err = 0;
	int returnVal = 0;
	//connect to network
	int sock = connect_server();
	char* pkt = lseek_package_gen(fd, offset, whence);

	// send request and get return value
	send_msg(sock,pkt); free(pkt);
	read_msg(sock,&returnVal, &err);
	
	orig_close(sock);
	errno = err;
	return returnVal;
}

int unlink(const char *path) {
	int err = 0;
	int return_val = 0;
	
	// init network
	int sock = connect_server();
	char* pkt = unlink_package_gen(path);

	// send request
	send_msg(sock,pkt); 
	free(pkt);
	// get retur value and errno
	read_msg(sock,&return_val, &err);
	
	orig_close(sock);
	errno = err;
	return return_val;	
}

int stat(const char *path, struct stat* buf) {
	int reVal = 0;
	int err =0;
	// init network
	int sock = connect_server();
	char* pkt = stat_package_gen(path);
	// send request
	send_msg(sock,pkt);

	// get return value and errno
	read_msg_and_payload(sock, &reVal, &err, buf);

	orig_close(sock);
	//printf("mylib: stat called\n");
	errno = err;
	return reVal;
}

int __xstat(int ver, const char * path, struct stat * stat_buf) {
	int err = 0;
	int return_val = 0;
	// init network
	
	//fprintf(stderr,"client:__xstat path: %s \n", path);
	int sock = connect_server();
	char* pkt = __xstat_package_gen(ver, path);
	// send request
	send_msg(sock,pkt);

	// get return value, errno and content
	read_msg_and_payload(sock, &return_val, &err, stat_buf);


    //fprintf(stderr,"client:__xstat size of stat: %zu \n", sizeof(struct stat)); 

	//fprintf(stderr,"client:__xstat  st_time: %lld \n", (long long)stat_buf->st_ctime); 
    //fprintf(stderr,"client:__xstat  U_id: %zu \n", (size_t)stat_buf->st_uid); 
	orig_close(sock);
	errno = err;
    //fprintf(stderr,"client:__xstat return_val: %d \n", return_val);
    //change reVal back to correct value, since we use it as packagesize indicator
    if (return_val != -1){ // keep return_val the same if eqauls to -1
    	return_val = 0;
    }
    //fprintf(stderr,"client:__xstat path: %s \n", path);
    //fprintf(stderr,"client:__xstat ver: %d \n", ver);
	//return return_val;
	//
	//
    //orig___xstat(ver, path, stat_buf);////
    //fprintf(stderr,"client:__xstatr st_time: %lld \n", (long long)stat_buf->st_ctime);
    //fprintf(stderr,"client:__xstatr U_id: %zu \n", (size_t) stat_buf->st_uid);  
	 return return_val;
}

int getdirentries(int fd, char *buf, size_t nbytes, off_t *basep) {
    if(fd < 0){
        errno = 9;
        return -1;
    }

    if(fd < rpc_fd_offset){ // local call -> run local read
    return orig_getdirentries(fd, buf, nbytes, basep);
	}

	fd = fd - rpc_fd_offset; // rpc call  -> get to original fd


	int err = 0;
	int return_val = 0;
	// init network
	int sock = connect_server();
	char* pkt = getdirentries_package_gen(fd, nbytes, *basep);

	// send request
	send_msg(sock,pkt);
	
	//get return msg
	read_msg_and_payload(sock, &return_val, &err, buf);
	*basep = *basep + return_val;
	orig_close(sock);
	
	errno = err;
	return return_val;
}


struct dirtreenode* getdirtree( const char *path ) {
	//struct dirtreenode* reVal = NULL;
	int return_val = 0;
	int err =0;
	// init network
	int sock = connect_server();
	char* pkt = getdirtree_package_gen(path);
	// send request
	send_msg(sock,pkt);
	// get return value and errno
	//char* buf = malloc(MAXMSGLEN);
	tree_buffer = malloc(MAXMSGLEN);
	read_msg_and_payload(sock, &return_val, &err, tree_buffer);//

	
	//fprintf(stderr,"client: getdirtree start \n");
	tree_buffer[return_val] = 0;
    fprintf(stderr,"%s\n", tree_buffer);

    struct dirtreenode* root = NULL;
    construct_tree(&root);
    struct dirtreenode* root2 = orig_getdirtree(path);

    //fprintf(stderr,"client: getdirtree %s , end\n",path);
	//fprintf(stderr,"client: getdirtree return val: %d , end\n",return_val);


    //fprintf(stderr,"client: getdirtree root->name: |%s| \n", root->name);
    //fprintf(stderr,"client: getdirtree root->num: |%d| \n", root->num_subdirs);
    //if(root->subdirs == NULL) fprintf(stderr,"client: getdirtree subtree is  NULL \n");
	
	//fprintf(stderr,"client: getdirtree root2->name: |%s| \n", root2->name);
    //fprintf(stderr,"client: getdirtree root2->num: |%d| \n", root2->num_subdirs);
    //if(root2->subdirs == NULL) fprintf(stderr,"client: getdirtree subtree2 is  NULL \n");

	free(tree_buffer);
	orig_close(sock);

	errno = err;

	return root2;
}

/*Function to delete the directory tree on the client*/ 
void free_tree(struct dirtreenode* root){
	int i;
	if(root -> subdirs){
		for(i = 0; i < root->num_subdirs; i++){
			free_tree(root->subdirs[i]);
		}
	}
	else {
		free(root);
	}
}

void freedirtree( struct dirtreenode* dt ) { 
    free_tree(dt);
}

void freedirtree2( struct dirtreenode* dt ) { //old
    char * function_name = "freedirtree";
	log_to_server(function_name);
    orig_freedirtree( dt );
	return;
}

/*function to create directory tree on client*/
void construct_tree(struct dirtreenode** root){
    char* name = NULL;
    if(*root == NULL) {
	name = strtok(tree_buffer, "\n");
    }
    else {
	name = strtok(NULL, "\n");
    }

    char *num = strtok(NULL, "\n");
    if(*root == NULL){
	    *root = (struct dirtreenode*)malloc(sizeof(struct dirtreenode));
    }
    (*root) -> num_subdirs = atoi(num);
    (*root) -> name = name;
    int i;
    if(atoi(num)){
        (*root) -> subdirs = malloc((*root)->num_subdirs*sizeof(struct dirtreenode));
        for(i = 0; i < (*root)->num_subdirs; i++){
            (*root) -> subdirs[i] = (struct dirtreenode*)malloc(sizeof(struct dirtreenode));
            construct_tree(&((*root)->subdirs[i]));
        }
    }
    else {
	   (*root)->subdirs = NULL;
    }
    return;   
}

