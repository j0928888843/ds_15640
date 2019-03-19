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
#include <err.h>
#include <errno.h>

#define MAXMSGLEN 1500
#define MAX_PATHNAME 200

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
////////////

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
}

/** brief: repleacement for the open function from libc
 *	param fd, file descriptor pointed to open file
 *  The function returns 0 if successful, -1 to indicate an error, 
 *	with errno set appropriately.
 */
int open(const char *pathname, int flags, ...) {
	char* function_name = "open";
	mode_t m=0;
	if (flags & O_CREAT) {
		va_list a;
		va_start(a, flags);
		m = va_arg(a, mode_t);
		va_end(a);
	}

	// fprintf(stderr, "mylib: open called for path %s\n", pathname);
	// 
	// we send function name to server, then call through to the 
	// original open function (from libc)
	
	log_to_server(function_name);
	return orig_open(pathname, flags, m);
}



/** brief: repleacement for the close function from libc
 *	param fd, file descriptor pointed to opened file
 *  The function returns 0 if successful, -1 to indicate an error, 
 *	with errno set appropriately.
 */
int close(int fd){
	    if(fd < 0){
        errno = 9;
        return -1;
    }
    // fprintf(stderr,"close\n");
    // fprintf(stderr,"fd = %d\n",fd);
    char* function_name = "close";
    log_to_server(function_name);
    return orig_close(fd);
}


/** brief: repleacement for the read function from libc
 *	param fd, file descriptor pointed to opened file
 *	param buffer, the space where read content to
 *	param nbyte, the size which is about to read 
 *  The function returns 0 if successful, -1 to indicate an error, 
 *	with errno set appropriately.
 */
ssize_t read(int fd, void *buffer, size_t nbyte) {
	if(nbyte == 0)
		return 0; // return 0 if requested size was 0
	//if(nbyte > MAXMSGLEN-MAX_PATHNAME)
	//	nbyte = MAXMSGLEN-MAX_PATHNAME;
	if(fd < 0){
        errno = 9;
        return -1;
    }
	
	char * function_name = "read";
	log_to_server(function_name);
	return orig_read(fd, buffer, nbyte);
}

/** brief: repleacement for the write function from libc
 *	param fd, file descriptor pointed to opened file
 *	param buffer, the space where write content to
 *	param nbyte, the size which is about to write 
 */
int write(int fd, void* buffer, int nbyte) {
	// if(nbyte > MAXMSGLEN-MAX_PATHNAME)
	//	nbyte = MAXMSGLEN-MAX_PATHNAME;

	if(nbyte == 0)
		return 0; // return 0 if requested size was 0
	if(fd < 0){
        errno = 9;
        return -1;
    }
	
	char * function_name = "write";
	log_to_server(function_name);
	return orig_write(fd, buffer, nbyte);
}


/** brief: repleacement for the lseek function from libc
 *	param fd, file descriptor pointed to opened file
 *	param offset, the offsize
 *	param whence, the start position
 *	return the actual bytes that have been moved
 */
int lseek(int fd, off_t offset, int whence) {
	if(fd < 0){
        errno = 9;
        return -1;
    }
	
	char * function_name = "lseek";
	log_to_server(function_name);
	return orig_lseek(fd, offset, whence);
}

int stat(const char *path, struct stat* buf) {
	char * function_name = "stat";
	log_to_server(function_name);
	return orig_stat(path, buf);
}

int __xstat(int ver, const char *path, struct stat* buf) {
	char * function_name = "stat";
	log_to_server(function_name);
	return orig___xstat(ver, path, buf);
}

int getdirentries(int fd, char *buf, size_t nbytes, off_t *basep) {
	char * function_name = "getdirentries";
	log_to_server(function_name);
	return orig_getdirentries(fd, buf, nbytes, basep);
}

int unlink(const char *path) {
    char * function_name = "unlink";
	log_to_server(function_name);
	return orig_unlink(path);	
}

//this is helper function that send the input string to the sever
void log_to_server(char* function_name){
	char *serverip;
	char *serverport;
	unsigned short port;
	int sockfd, rv;
	struct sockaddr_in srv;

	// Get environment variable indicating the ip address of the server
	serverip = getenv("server15440");
	if (serverip) fprintf(stderr, "Got environment variable server15440: %s\n", serverip);
	else {
		// fprintf(stderr,"Environment variable server15440 not found.  Using 127.0.0.1\n");
		serverip = "127.0.0.1";
	}
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport) fprintf(stderr, "Got environment variable serverport15440: %s\n", serverport);
	else {
		// fprintf(stderr, "Environment variable serverport15440 not found.  Using 15440\n");
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
	if (rv<0) err(1,0);

	// send message to server
	//printf("client sending to server: %s\n", msg);
	send(sockfd, function_name, strlen(function_name), 0);	// send message; should check return value
	orig_close(sockfd);
    //void , dont have to return
}


