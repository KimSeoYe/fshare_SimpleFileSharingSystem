#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <linux/limits.h>
#include <pthread.h>

#include "socket.h"

/*
    Todo. 2.0
    If the server is first opened, and it already has some files?
*/

#define LIST 1 
#define GET 2 
#define PUT 3 

char dir_name[PATH_MAX] ;

void
get_parameters (int argc, char ** argv, int * port_num, char * dir_name)
{
    if (argc != 5) {
        perror("ERROR: ./fshared -p <port_num> -d <dir_name>") ;
        exit(1) ;
    }
    
    char option ;
    while ((option = getopt(argc, argv, "p:d:")) != -1) {
        switch (option) {
            case 'p':
                *port_num = atoi(optarg) ;
                break ;
            case 'd':
                strcpy(dir_name, optarg) ;
                break ;
        }
    }

    if (access(dir_name, R_OK) == -1) {
        perror("ERROR: ") ;
        exit(1) ;
    }
    
    struct stat st ; 
    stat(dir_name, &st) ;
    if (!S_ISDIR(st.st_mode)) {
        perror("ERROR: not a directory") ;
        exit(1) ;
    }

    return ;  
}

void
list_d (int conn)
{
    send_int(conn, 0) ; // success
    send_meta_data(conn) ;
    shutdown(conn, SHUT_WR) ;

    print_meta_data() ;
}

void
get_d (int conn) 
{
    char * file_name = recv_message(conn) ;

    if (strstr(file_name, "/") != NULL)     // Todo. . ..
        goto err_send ;

    char file_path[PATH_MAX] ;
    strcpy(file_path, dir_name) ;
    strcat(file_path, "/") ;
    strcat(file_path, file_name) ;

    if (access(file_path, R_OK) == -1)  /* not exist */
        goto err_send ;

    struct stat st ;
    stat(file_path , &st) ;
    if (!S_ISREG(st.st_mode))
        goto err_send ;

    int version = get_version(file_name) ;

    send_int(conn, 0) ;
    send_int(conn, version) ;
    read_and_send(conn, file_path) ;
    shutdown(conn, SHUT_WR) ;

    return ;

err_send:
    send_int(conn, 1) ;
    shutdown(conn, SHUT_WR) ;
}

void
put_d (int conn) 
{
    unsigned int name_len = recv_int(conn) ;
    char * file_name = recv_n_message(conn, name_len) ;

    char file_path[PATH_MAX] ;
    strcpy(file_path, dir_name) ; // Todo. check if the dir_name has /
    strcat(file_path, file_name) ;

    int new_version ;
    if (access(file_path, F_OK) == 0) {
        new_version = increase_version(file_name) ;
        if (new_version == -1) {
            send_int(conn, 1) ; // failed
            shutdown(conn, SHUT_WR) ;
            return ;
        }
    }
    else {
        append_meta_data(file_name, 0) ;
    }

    send_int(conn, 0) ; // success
    send_int(conn, new_version) ;
    shutdown(conn, SHUT_WR) ;
 
    recv_and_write(conn, file_path) ;

    return ;
}

void * 
worker (void * ptr)
{
    int conn = * ((int *) ptr) ;
    
    unsigned int cmd_id ;
    cmd_id = recv_int(conn) ;
    
    switch (cmd_id) {
        case LIST:
            list_d(conn) ;
            break ;
        case GET:
            get_d(conn) ;
            break ;
        case PUT:
            put_d(conn) ;
            break ;
    }

	close(conn) ;
	free(ptr) ;

}

int
main (int argc, char ** argv)
{
    int port_num ;
    get_parameters(argc, argv, &port_num, dir_name) ;

    printf("SUCCESS! port # %d / %s\n", port_num, dir_name) ;

    int listen_fd, new_socket ; 
	struct sockaddr_in address ; 
	int addrlen = sizeof(address) ; 

	listen_fd = socket(AF_INET , SOCK_STREAM , 0) ;
	if (listen_fd == 0)  { 
		perror("socket failed : ") ; 
		exit(1) ; 
	}
	
	memset(&address, 0, sizeof(address)) ; 
	address.sin_family = AF_INET ; 
	address.sin_addr.s_addr = INADDR_ANY ; 
	address.sin_port = htons(port_num) ; 
	if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed : ") ; 
		exit(1); 
	} 

	while (1) {
		if (listen(listen_fd, 16) < 0) { 
			perror("listen failed : ") ; 
			exit(1) ; 
		} 

		new_socket = accept(listen_fd, (struct sockaddr *) &address, (socklen_t*)&addrlen) ;
		if (new_socket < 0) {
			perror("accept") ; 
			exit(EXIT_FAILURE) ; 
		} 

		int * sock_addr = (int *) malloc(sizeof(int) * 1) ;
		*sock_addr = new_socket ;

		pthread_t thr ;
		if (pthread_create(&thr, NULL, worker, (void *) sock_addr) != 0) {
			perror("ERROR - pthread_create: ") ;
			exit(1) ;
		}
	}

    return 0;
}