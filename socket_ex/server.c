// Partly taken from https://www.geeksforgeeks.org/socket-programming-cc/

#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <pthread.h>

void
send_message(int conn, char * message) 
{
	int len = strlen(message) ;
	int s ;
	while (len > 0 && (s = send(conn, message, len, 0)) > 0) {
		message += s ;
		len -= s ;
	}
	shutdown(conn, SHUT_WR) ;	// is it better to position shutdown here ?
}

void 
send_file(int conn, char * file_name) 
{
	/* 
		file이 존재하는지 확인
		file이 존재하지 않을 경우 -> 메세지
		file이 존재할 경우 -> 해당 파일을 열어서 send
			: 파일을 '읽으면서' 보낸다 -> 어느 정도 크기의 파일일지 모르니까.
	*/
	if (access(file_name, R_OK) == -1) {
		send_message(conn, "File does not exist!") ;
	}
	else {
		FILE * r_fp ;
		r_fp = fopen(file_name, "rb") ;
		if (r_fp == NULL) {
			perror("ERROR - fopen: ") ;
			exit(1) ;
		}

		while (feof(r_fp) == 0) {
			char buf[1024] ;
			size_t r_len = fread(buf, 1, sizeof(buf), r_fp) ;
			
			int s ;
			char * buf_p = buf ;
			while (r_len > 0 && (s = send(conn, buf, r_len, 0)) > 0) {
				buf_p += s ;
				r_len -= s ;
			}
		}
		shutdown(conn, SHUT_WR) ;
	}
	
} 

void *
child_thr(void * ptr)
{
	int conn = * ((int *) ptr) ;	
 
	char buf[1024] ;
	char * data = 0x0, * orig = 0x0 ;
	int len = 0 ;
	int s ;

	while ( (s = recv(conn, buf, 1023, 0)) > 0 ) {
		buf[s] = 0x0 ;
		if (data == 0x0) {
			data = strdup(buf) ;
			len = s ;
		}
		else {
			data = realloc(data, len + s + 1) ;
			strncpy(data + len, buf, s) ;
			data[len + s] = 0x0 ;
			len += s ;
		}

	}
	printf(">%s\n", data) ;
	
	send_file(conn, data) ;

	close(conn) ;
	free(ptr) ;
}

int 
main(int argc, char const *argv[]) 
{ 
	int listen_fd, new_socket ; 
	struct sockaddr_in address ; 
	// int opt = 1; 
	int addrlen = sizeof(address) ; 

	char buffer[1024] = {0} ; 

	listen_fd = socket(AF_INET /*IPv4*/, SOCK_STREAM /*TCP*/, 0 /*IP*/) ;
	if (listen_fd == 0)  { 
		perror("socket failed : ") ; 
		exit(EXIT_FAILURE) ; 
	}
	
	memset(&address, 0, sizeof(address)) ; 
	address.sin_family = AF_INET ; 
	address.sin_addr.s_addr = INADDR_ANY /* the localhost*/ ; 
	address.sin_port = htons(8888) ; 
	if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed : ") ; 
		exit(EXIT_FAILURE); 
	} 

	while (1) {
		if (listen(listen_fd, 16 /* the size of waiting queue*/) < 0) { 
			perror("listen failed : ") ; 
			exit(EXIT_FAILURE) ; 
		} 

		new_socket = accept(listen_fd, (struct sockaddr *) &address, (socklen_t*)&addrlen) ;
		if (new_socket < 0) {
			perror("accept") ; 
			exit(EXIT_FAILURE) ; 
		} 

		int * sock_addr = (int *) malloc(sizeof(int) * 1) ;
		*sock_addr = new_socket ;
		pthread_t * thr ;
		if (pthread_create(thr, NULL, child_thr, (void *) sock_addr) != 0) {
			perror("ERROR - pthread_create: ") ;
			exit(1) ;
		}
	}
} 
