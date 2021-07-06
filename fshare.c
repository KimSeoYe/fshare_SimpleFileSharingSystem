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

int 
get_parameters (int argc, char ** argv, char * ip_addr, int * port_num, char * file_name) 
{
    /*
        Command Line Interface
        ./fshare <ip_addr> <command> (<file_name>)
        command: 
            list
            get <file_name>
            put <file_name>
    */

    int option ;

    if (argc < 3 || (strcmp(argv[2], "list") != 0 && strcmp(argv[2], "get") != 0 && strcmp(argv[2], "put")) != 0)
        goto err_exit ;

    /*
        Q. invalid ip addr ? 
            - 정규표현식...? https://info-lab.tistory.com/294 
            - sscanf...? -> %d.%d.%d.%d:%d
        Q. ip 주소와 port 넘버로 split! -> invalid form일 경우는..?
        ! atoi는 숫자가 처음 시작한 부분부터 문자 혹은 \0이 나오는 지점까지를 int로 변환한다.
            ::8080:: -> 8080
            ::80::80:: -> 80
    */

    char * next_ptr ;
    char * ptr = strtok_r(argv[1], ":", &next_ptr) ;
    strcpy(ip_addr, ptr) ;
    *port_num = atoi(next_ptr) ; 

    if (strcmp(argv[2], "list") == 0) {
        if (argc != 3) 
            goto err_exit ;

        option = 1 ;
    }

    if (strcmp(argv[2], "get") == 0) {
        if (argc != 4) 
            goto err_exit ;

        option = 2 ;
        strcpy(file_name, argv[3]) ;
    }

    if (strcmp(argv[2], "put") == 0) {
        if (argc != 4) 
            goto err_exit ;
        
        if (access(argv[3], R_OK) == -1) {
            perror("ERROR: ") ;
            exit(1) ;
        }

        option = 3 ;
        strcpy(file_name, argv[3]) ;
    }

    

    return option ;

err_exit:
    perror("./fshare <ip_addr> <command> (<file_name>)\ncommand:\n\tlist\n\tget <file_name>\n\tput <file_name>\n") ;
    exit(1) ;
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
list (char * ip_addr, int port_num, int option)
{
    int sock_fd = make_connection(ip_addr, port_num) ;

    send_header(sock_fd, option) ;
    shutdown(sock_fd, SHUT_WR) ;

    int resp_header = recv_header(sock_fd) ;
    if (resp_header == 1) {
        perror("ERROR: cannot request list\n") ;
        exit(1) ;
    }

    recv_meta_data(sock_fd) ;
    print_meta_data() ;

    close(sock_fd) ;
}

void
get (char * ip_addr, int port_num, int option, char * file_name)
{
    int sock_fd = make_connection(ip_addr, port_num) ;
    
    send_header(sock_fd, option) ;

    // send file name
    send_message(sock_fd, file_name) ;
    shutdown(sock_fd, SHUT_WR) ;

    int resp_header = recv_header(sock_fd) ;
    if (resp_header == 1) {
        perror("ERROR: cannot request get\n") ;
        exit(1) ;
    }

    recv_and_write(sock_fd, file_name) ;
}

void
put (char * ip_addr, int port_num, int option, char * file_name)
{
    int sock_fd = make_connection(ip_addr, port_num) ;
    
    send_header(sock_fd, option) ;

    // Todo. check if filename is valid
    if (strstr(file_name, "/") != NULL)
        goto err_exit ;

    if (access(file_name, R_OK) == -1)
        goto err_exit ;

    struct stat st ;
    stat(file_name, &st) ;
    if (!S_ISREG(st.st_mode))
        goto err_exit ;
 
    // need to send the length of the filename
    send_header(sock_fd, strlen(file_name)) ;
    send_message(sock_fd, file_name) ;

    read_and_send(sock_fd, file_name) ;
    shutdown(sock_fd, SHUT_WR) ;

    int resp_header = recv_header(sock_fd) ;
    
    if (resp_header == 1) {
        perror("ERROR: cannot request put\n") ;
        exit(1) ;
    }

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
    char file_name[PATH_MAX] ; // path_max is to long...

    int option = get_parameters(argc, argv, ip_addr, &port_num, file_name) ;

    switch (option) {
        case 1:
            list(ip_addr, port_num, option) ;
            break ;
        case 2:
            get(ip_addr, port_num, option, file_name) ;
            break ;
        case 3:
            put(ip_addr, port_num, option, file_name) ;
            break ;
    }

    return 0 ;
}