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

#define DEBUG

#define EVENT_SIZE ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN ( 1024 * ( EVENT_SIZE + 16 ) )

#define LIST 1 
#define GET 2 
#define PUT 3 

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

#ifdef DEBUG
void 
print_list (Node * list) 
{
    Node* itr = 0x0;
    printf("============= files ===============\n");
    for(itr = list->next; itr != 0x0; itr = itr->next) {
        printf("%s %d\n", itr->file_name, itr->ver);
    }
    printf("===================================\n");
}
#endif

void
append (Node * list, char * file_name, int ver)
{
	Node * new_node = (Node *) malloc(sizeof(Node) * 1) ;
	new_node->next = 0x0 ;
	new_node->ver = ver ;
    new_node->name_len = strlen(file_name) ;
    new_node->file_name = strdup(file_name) ;

	Node* itr = 0x0, *curr = 0x0;

    for (itr = list ; itr != 0x0 ; itr = itr->next) {
        curr = itr;
    }
    curr->next = new_node;
}

Node *
recv_meta_data (int sock_fd)
{
    int ver, len ;
    char * data ;

    Node * server_data = (Node *) malloc(sizeof(Node) * 1) ;

    while ((ver = recv_int(sock_fd)) >= 0) {
        len = recv_int(sock_fd) ;
        data = recv_n_message(sock_fd, len) ;
        append(server_data, data, ver) ;
    }

    return server_data ;
}

void
get (char * file_name)
{
    int sock_fd = make_connection(ip_addr, port_num) ;

    send_int(sock_fd, GET) ;

    send_message(sock_fd, file_name) ;
    shutdown(sock_fd, SHUT_WR) ;

    int resp_header = recv_int(sock_fd) ;
    if (resp_header == 1) {
        perror("ERROR: cannot request get\n") ;
        exit(1) ;
    }

    int new_version = recv_int(sock_fd) ;
    recv_and_write(sock_fd, file_name) ;

    if (is_exist(file_name)) {
        update_version(file_name, new_version) ;
    }
    else {
        append_meta_data(file_name, new_version) ;
    }

#ifdef DEBUG
    // printf("> get end! \n") ;
    // print_meta_data() ;
#endif

    close(sock_fd) ;
}

void
list ()
{
    int sock_fd = make_connection(ip_addr, port_num) ;

    send_int(sock_fd, LIST) ;
    shutdown(sock_fd, SHUT_WR) ;

    int resp_header = recv_int(sock_fd) ;

    if (resp_header == 1) {
        perror("ERROR: cannot request list\n") ;
        exit(1) ;
    }

    Node * server_data = recv_meta_data(sock_fd) ;
#ifdef DEBUG
    // printf("> server data!\n") ;
    // print_list(server_data) ;
#endif

    Node * s_itr = 0x0 ;
    for (s_itr = server_data->next; s_itr != 0x0; s_itr = s_itr->next) {
        int s_found = find_meta_data(s_itr) ;
        if (s_found == 0) {
            get(s_itr->file_name) ;
        }
    }

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

    send_int(sock_fd, PUT) ;

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

#ifdef DEBUG
    // printf("> put end!\n") ;
    // print_meta_data() ;
#endif

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
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }
}

void *
acquire_list ()
{
    while (1) {
        sleep(5) ; // proper time ?
        list() ;
    #ifdef DEBUG
        printf("> list called!\n") ;
        print_meta_data() ;
    #endif
    }
}

int
main (int argc, char ** argv) 
{
    get_parameters(argc, argv, ip_addr, &port_num) ;

    pthread_t list_thr ;
    if (pthread_create(&list_thr, NULL, acquire_list, NULL) != 0) {
		perror("ERROR - pthread_create: ") ;
		exit(1) ;
	}

    monitor_dir() ;

    /*
        BUG:
        when the client calls get() automatically,
        inotify() detects the file as a new file, thus calls put().
        So the version of the same file is updated two times.
    */

    return 0 ;
}