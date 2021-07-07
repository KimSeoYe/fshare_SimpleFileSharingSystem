#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
// #include <regex.h>
#include <linux/limits.h>

#include "socket.h"

void
get_parameters (int argc, char ** argv, char * ip_addr, int * port_num) 
{
    if (argc != 2) {
        perror("./fshare <ip_addr>:<port_num>\n") ;
        exit(1) ;
    }

    char * next_ptr ;
    char * ptr = strtok_r(argv[1], ":", &next_ptr) ;
    strcpy(ip_addr, ptr) ;
    *port_num = atoi(next_ptr) ; 

    return ;
}

int
make_connection (char * ip_addr, int port_num) {
    int sock_fd ;
    struct sockaddr_in serv_addr ; 
	
	sock_fd = socket(AF_INET, SOCK_STREAM, 0) ;
	if (sock_fd <= 0) {
		perror("socket failed : ") ;
		exit(1) ;
	} 

	memset(&serv_addr, 0, sizeof(serv_addr)) ; 
	serv_addr.sin_family = AF_INET ; 
	serv_addr.sin_port = htons(port_num) ; 
	if (inet_pton(AF_INET, ip_addr, &serv_addr.sin_addr) <= 0) {
		perror("inet_pton failed : ") ; 
		exit(EXIT_FAILURE) ;
	} 

	if (connect(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("connect failed : ") ;
		exit(EXIT_FAILURE) ;
	}

    return sock_fd ;
}

void
list (int sock_fd)
{
    send_int(sock_fd, 1) ;
    shutdown(sock_fd, SHUT_WR) ;

    int resp_header = recv_int(sock_fd) ;
    if (resp_header == 1) {
        perror("ERROR: cannot request list\n") ;
        exit(1) ;
    }

    recv_meta_data(sock_fd) ;
    print_meta_data() ;

    close(sock_fd) ;
}

void
get (int sock_fd, char * file_name)
{
    send_int(sock_fd, 2) ;

    // send file name
    send_message(sock_fd, file_name) ;
    shutdown(sock_fd, SHUT_WR) ;

    int resp_header = recv_int(sock_fd) ;
    if (resp_header == 1) {
        perror("ERROR: cannot request get\n") ;
        exit(1) ;
    }

    recv_and_write(sock_fd, file_name) ;
}

void
put (int sock_fd, char * file_name)
{
    send_int(sock_fd, 3) ;

    if (strstr(file_name, "/") != NULL)
        goto err_exit ;

    if (access(file_name, R_OK) == -1)
        goto err_exit ;

    struct stat st ;
    stat(file_name, &st) ;
    if (!S_ISREG(st.st_mode))
        goto err_exit ;

    send_int(sock_fd, strlen(file_name)) ;
    send_message(sock_fd, file_name) ;

    read_and_send(sock_fd, file_name) ;
    shutdown(sock_fd, SHUT_WR) ;

    int resp_header = recv_int(sock_fd) ;
    
    if (resp_header == 1) {
        perror("ERROR: cannot request put\n") ;
        exit(1) ;
    }

    int ver = recv_int(sock_fd) ;
    /*
        Todo. 2.0
        만들어졌을 때 Put 하는 경우 -> linked list에 append한다.
        이미 있는데 수정되어서 Put 하는 경우 -> linked list에서 찾아서 update한다.

        Temporary
        linked list에서 찾아서 update한다.
        -> linked list에 없어서 실패한 경우 append한다.

        *** server가 종료될때마다 list 정보가 날아가긴 한다...
    */
    int exist = update_version(file_name, ver) ;
    if (exist == -1) {
        append(file_name, ver) ;
    }

    print_meta_data() ;

    return ;

err_exit:
    perror("ERROR: invalid file name") ;
    exit(1) ;
}

int
main (int argc, char ** argv) 
{
    char ip_addr[32] ; // max 12 ip address + dot(.)
    int port_num ;

    get_parameters(argc, argv, ip_addr, &port_num) ;
    int sock_fd = make_connection(ip_addr, port_num) ;

    return 0 ;
}