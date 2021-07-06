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

char dir_name[PATH_MAX] ; // pointer로? PATH_MAX로?

void
get_parameters (int argc, char ** argv, int * port_num, char * dir_name)
{
    /*
        Command Line Interface
        ./fshared -p <port_num> -d <dir_name>

        * argc 확인 
        * getopt p:d:
            * p -> int로 저장 (int가 아니면...?)
            * d -> string으로 저장
        * 디렉토리 유무 확인
            1. 디렉토리가 존재하지 않는 경우 (access 사용)
            2. 존재하지만 디렉토리가 아닌 경우 (stat 사용)
            3. 디렉토리가 존재하는 경우
    */

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
    DIR * dp = opendir(dir_name) ;
    
    if (dp != NULL) {
        send_header(conn, 0) ;

        struct dirent * sub ;
        for ( ; sub = readdir(dp); ) {
            if (sub->d_type == DT_REG) { 

                int len = strlen(sub->d_name) + 1 ;
                char * file_name = (char *) malloc(sizeof(char) * len) ;
                strcpy(file_name, sub->d_name) ;
                strcat(file_name, "\n") ;   

                int s = 0 ;
                while (len > 0 && (s = send(conn, file_name, len, 0)) > 0) {
                    file_name += s ;
                    len -= s ;
                }
            }   
        }
        shutdown(conn, SHUT_WR) ;
    }
    else {
        perror("opendir: ") ;
        send_header(conn, 1) ;
        shutdown(conn, SHUT_WR) ;
        return ;
    }
}   

void
get_d (int conn) 
{
    /*
        1. recv file name  
        2. check if the file name is valid!
        * err check for resp.header
        4. read and send the file
    */
    char * file_name = recv_message(conn) ;

    if (strstr(file_name, "/") != NULL)     // . ..
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

    send_header(conn, 0) ;
    read_and_send(conn, file_path) ;
    shutdown(conn, SHUT_WR) ;

    return ;

err_send:
    send_header(conn, 1) ;
    shutdown(conn, SHUT_WR) ;
}

void
put_d (int conn) 
{
    /*
        1. recv file name
        2. make proper path
        3. recv and write
    */

    // need to recv the length of the file first
    // then recv the file name

    unsigned int name_len = recv_header(conn) ;
    char * file_name = recv_n_message(conn, name_len) ;

    char file_path[PATH_MAX] ;
    strcpy(file_path, dir_name) ; // Todo. check if the dir_name has /
    strcat(file_path, file_name) ;

    // if it is exist..
    if (access(file_path, F_OK) == 0) {
        send_header(conn, 1) ; // Todo. unsigned! -> success:0 / ...
        shutdown(conn, SHUT_WR) ;
        return ;
    }

    recv_and_write(conn, file_path) ;

    send_header(conn, 0) ;
    shutdown(conn, SHUT_WR) ;

    return ;
}

void * 
child_thr (void * ptr)
{
    int conn = * ((int *) ptr) ;
    
    unsigned int cmd_id ;
    cmd_id = recv_header(conn) ;

    switch (cmd_id) {
        case 1:
            list_d(conn) ;
            break ;
        case 2:
            get_d(conn) ;
            break ;
        case 3:
            put_d(conn) ;
            break ;
    }

	close(conn) ;
	free(ptr) ;

}

int
main (int argc, char ** argv)
{
    /*
        * parameter 받기 -> port_num & dir_name
        * socket 연결하기
        Q. 디렉토리를 언제 열지?
    */

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
		if (pthread_create(&thr, NULL, child_thr, (void *) sock_addr) != 0) {
			perror("ERROR - pthread_create: ") ;
			exit(1) ;
		}
	}

    return 0;
}