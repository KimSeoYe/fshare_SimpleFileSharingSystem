#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/inotify.h>
#include <linux/limits.h>

#include "socket.h"

#define EVENT_SIZE ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN ( 1024 * ( EVENT_SIZE + 16 ) )

int i_fd ;
int i_wd ;

char ip_addr[32] ; // max 12 ip address + dot(.)
int port_num ;

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
list ()
{
    int sock_fd = make_connection(ip_addr, port_num) ;

    send_int(sock_fd, 1) ;
    shutdown(sock_fd, SHUT_WR) ;

    int resp_header = recv_int(sock_fd) ;

    if (resp_header == 1) {
        perror("ERROR: cannot request list\n") ;
        exit(1) ;
    }

    int ver, len ;
    char * data ;

    while ((ver = recv_int(sock_fd)) >= 0) {
        len = recv_int(sock_fd) ;
        data = recv_n_message(sock_fd, len) ;
        printf("> %d %s\n", ver, data) ;
    }

    close(sock_fd) ;
}

void
get (char * file_name)
{
    int sock_fd = make_connection(ip_addr, port_num) ;

    send_int(sock_fd, 2) ;

    send_message(sock_fd, file_name) ;
    shutdown(sock_fd, SHUT_WR) ;

    int resp_header = recv_int(sock_fd) ;
    if (resp_header == 1) {
        perror("ERROR: cannot request get\n") ;
        exit(1) ;
    }

    recv_and_write(sock_fd, file_name) ;

    close(sock_fd) ;
}

void
put (char * file_name)
{
    int sock_fd = make_connection(ip_addr, port_num) ;

    if (strstr(file_name, "/") != NULL)
        goto err_exit ;

    if (access(file_name, R_OK) == -1)
        goto err_exit ;

    struct stat st ;
    stat(file_name, &st) ;
    if (!S_ISREG(st.st_mode))
        goto err_exit ;

    send_int(sock_fd, 3) ;

    send_int(sock_fd, strlen(file_name)) ;
    send_message(sock_fd, file_name) ;
    read_and_send(sock_fd, file_name) ;
    shutdown(sock_fd, SHUT_WR) ;

    int resp_header = recv_int(sock_fd) ;
    if (resp_header == 1) {
        perror("ERROR: cannot request put\n") ;
        exit(1) ;
    }

    int version = recv_int(sock_fd) ;

    int exist = update_version(file_name, version) ;
    if (exist == -1) {
        append_meta_data(file_name, version) ;
    }

    // debug
    print_meta_data() ;

    close(sock_fd) ;
    free(file_name) ;
    return ;

err_exit:
    perror("ERROR: invalid file name") ;
    close(sock_fd) ;
    free(file_name) ;
    exit(1) ;
}

void 
handler (int sig)
{
    if (sig == SIGINT) {
        inotify_rm_watch(i_fd, i_wd) ;
        close(i_fd) ;
        exit(0) ;
    }
}

void 
monitor_dir ()
{
    signal(SIGINT, handler) ;

    i_fd = inotify_init() ;
    if (i_fd < 0) {
        perror("inotify_init") ;
    }
    i_wd = inotify_add_watch(i_fd, "./", IN_CREATE | IN_MOVED_TO) ;
    
    // Todo. 2.0 IN_MODIFY ? -> nano, ...

    while (1) {
        char buffer[EVENT_BUF_LEN] ;
        int length = read(i_fd, buffer, EVENT_BUF_LEN) ; 
        if (length < 0) {
            perror("read") ;
        }  

        int i = 0;
        while (i < length) {     
            struct inotify_event * event = (struct inotify_event *) &buffer[i] ;     
            if (event->len) {
                if (event->mask & IN_CREATE || event->mask & IN_MOVED_TO) {
                    struct stat buf ;
                    stat(event->name, &buf) ;
                    if (S_ISREG(buf.st_mode) && event->name[0] != '.' && strcmp(event->name, "4913") != 0 && strstr(event->name, "~") == NULL) {
                        char * file_name = strdup(event->name) ;
                        put(file_name) ;
                        list() ;
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }
}

int
main (int argc, char ** argv) 
{
    get_parameters(argc, argv, ip_addr, &port_num) ;

    monitor_dir() ;

    return 0 ;
}