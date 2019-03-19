#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <math.h>
#include "package_process.h"
#include "../include/dirtree.h"

#define MAXMSGLEN 1600 // need to match at client side 
#define rpc_fd_offset 1000
int main(int argc, char**argv) {
	char buf[MAXMSGLEN+1];
	char *serverport;
	unsigned short port;
	int sockfd, sessfd, rv;
	struct sockaddr_in srv, cli;
	socklen_t sa_size;
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport) port = (unsigned short)atoi(serverport);
	else port=15440;
	
	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);	// TCP/IP socket
	if (sockfd<0) err(1, 0);			       // in case of error
	
	// setup address structure to indicate server port
	memset(&srv, 0, sizeof(srv));			// clear it first
	srv.sin_family = AF_INET;			    // IP family
	srv.sin_addr.s_addr = htonl(INADDR_ANY);// don't care IP address(accpet all)
	srv.sin_port = htons(port);			    // server port

	// bind to our port
	rv = bind(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
	if (rv<0) err(1,0);
	
	// start listening for connections
	rv = listen(sockfd, 5);
	if (rv<0) err(1,0);
	
	// main server loop, handle clients one at a time
	while(1) {
		
		// wait for next client, get session socket
		sa_size = sizeof(struct sockaddr_in);
		sessfd = accept(sockfd, (struct sockaddr *)&cli, &sa_size);
		if (sessfd<0) err(1,0);
		
		if ( (rv=recv(sessfd, buf, MAXMSGLEN, 0)) > 0) {

            // print the receive buffer data to stderr
			fprintf(stderr,"%s\n", buf);

			char* payload = NULL;
			char* return_package = NULL;
			int return_val = 0;
			int err = 0;
			int payload_size = 0;

			// check first line
			if(strstr(buf,"\n") != NULL) {
				//comfirm package type
				if(strncmp(buf, "OPEN",4) == 0) 
					return_val = open_package(buf,&err);
				if(strncmp(buf, "WRITE",5) == 0) 
					return_val = write_package(buf,&err);
				if(strncmp(buf, "CLOSE",5) == 0) 
					return_val = close_package(buf,&err);
				if(strncmp(buf, "READ", 4) == 0)
					return_val = read_package(buf,&err, &payload_size, &payload);
				if(strncmp(buf, "LSEEK", 5) == 0)
					return_val = lseek_package(buf, &err);
				if(strncmp(buf, "UNLINK", 6) ==0)
					return_val = unlink_package(buf, &err);
				if(strncmp(buf, "__XSTAT", 7) == 0) 
					return_val = __xstat_package(buf, &err, &payload_size, &payload);
				if(strncmp(buf, "STAT", 4) == 0)
					return_val = stat_package(buf, &err, &payload_size, &payload);
				if(strncmp(buf, "GETDIRENTRIES", strlen("GETDIRENTRIES")) ==0 )	
					return_val = gendirentries_package(buf, &err, &payload_size, &payload);
				if(strncmp(buf, "GETDIRTREE", strlen("GETDIRTREE")) ==0 )	
					return_val = dirtree_package(buf, &err, &payload_size, &payload);
			}

			// send return data back
			if(payload == NULL) { // without payload
				return_package = return_package_gen(return_val,err);
				send(sessfd,return_package,strlen(return_package),0);
				free(return_package);
			} else {             // with payload
				return_package = return_PL_package_gen(return_val,err,payload_size,payload);
				send(sessfd,return_package,payload_size+(int)(strstr(return_package,"\n\n")-return_package) 
				+ strlen("\n\n"),0);			
				free(return_package);
				free(payload);

			}
			memset(buf,0,MAXMSGLEN);
		}


		// either client closed connection, or error
		if (rv<0) err(1,0);
		close(sessfd);
	}
	
	printf("server shutting down cleanly\n");
	// close socket
	close(sockfd);

	return 0;
}

