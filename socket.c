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
#include <linux/limits.h>
#include <pthread.h>

#include "socket.h"

#define DEBUG

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER ;

Node meta_data = {0, 0, 0x0, 0x0} ; // Todo. list.c나 새로운 파일로 쪼개기

void 
print_meta_data () 
{
    Node* itr = 0x0;
    printf("============= files ===============\n");
    pthread_mutex_lock(&m);
    for(itr = meta_data.next; itr != 0x0; itr = itr->next) {
        printf("%s %d\n", itr->file_name, itr->ver);
    }
    pthread_mutex_unlock(&m);
    printf("===================================\n");
}

int
is_exist (char * file_name)
{
    int exist = 0 ;
    Node * itr = 0x0 ;

    pthread_mutex_lock(&m) ;
    for (itr = meta_data.next; itr != 0x0; itr = itr->next) {
        if (strcmp(itr->file_name, file_name) == 0) {
            exist = 1 ;
            break ;
        }
    }
    pthread_mutex_unlock(&m) ;

    return exist ;
}

void
append_meta_data (char * file_name, int ver)
{
	Node * new_node = (Node *) malloc(sizeof(Node) * 1) ;
	new_node->next = 0x0 ;
	new_node->ver = ver ;
    new_node->name_len = strlen(file_name) ;
    new_node->file_name = strdup(file_name) ;

	Node* itr = 0x0, *curr = 0x0;
    
	pthread_mutex_lock(&m);

    for (itr = &meta_data ; itr != 0x0 ; itr = itr->next) {
        curr = itr;
    }
    curr->next = new_node;

    pthread_mutex_unlock(&m);
}

int
increase_version (char * file_name) // Todo. mix w/ update_version
{
    Node * itr = 0x0 ;
    int new_version = -1 ;

    pthread_mutex_lock(&m);
    for (itr = meta_data.next; itr != 0x0; itr = itr->next) {
        if (strcmp(itr->file_name, file_name) == 0) {
            itr->ver ++ ;
            new_version = itr->ver ;
            break ;
        }
    }
    pthread_mutex_unlock(&m);

    return new_version ;
}

int
update_version (char * file_name, int ver)
{
    Node * itr = 0x0 ;
    int new_version = -1 ;

    pthread_mutex_lock(&m);
    for (itr = meta_data.next; itr != 0x0; itr = itr->next) {
        if (strcmp(itr->file_name, file_name) == 0) {
            itr->ver = ver ;
            new_version = ver ;
            break ;
        }
    }
    pthread_mutex_unlock(&m);

    return new_version ;
}

int 
get_version (char * file_name) 
{
    int ver = 0 ;
    Node * itr = 0x0 ;
    
    pthread_mutex_lock(&m) ;
    for (itr = meta_data.next; itr != 0x0; itr = itr->next) {
        if (strcmp(itr->file_name, file_name) == 0) {
            ver = itr->ver ;
            break ;
        }
    }
    pthread_mutex_unlock(&m) ;

    return ver ;
}

int
find_meta_data (Node * node)
{
    int s_found = 0 ;

    Node * m_itr = 0x0 ;
    pthread_mutex_lock(&m) ; 
    for (m_itr = meta_data.next; m_itr != 0x0; m_itr = m_itr->next) {
        if (strcmp(node->file_name, m_itr->file_name) == 0 && node->ver == m_itr->ver) {
        #ifdef DEBUG
            // printf("> %s found!\n", node->file_name) ;
        #endif
            s_found = 1 ;
            break ;
        }
    }
    pthread_mutex_unlock(&m) ; 

#ifdef DEBUG
    if (s_found == 0) {
        printf("> get %s\n",node->file_name) ;
    }
#endif

    return s_found ;
}

void
send_int (int sock, int h)  // Todo. unsigned int!!!!
{
    int s = 0 ;
    int len = 4 ;
    char * head_p = (char *) &h ;
    while (len > 0 && (s = send(sock, head_p, len, 0)) > 0) {
        head_p += s ;
        len -= s ;
    }
}

int
recv_int (int sock)
{   
    int s = 0 ; 
    int len = 4 ;
    int header ;
    char * head_p = (char *) & header ;

    while (len > 0) {
        s = recv(sock, head_p, len, 0) ;
        if (s == 0) {
            return -1 ;
        }
        head_p += s ;
        len -= s ;
    }

    return header ;
}

void
send_message(int sock, char * message) 
{
	int len = strlen(message) ;
	int s ;
	while (len > 0 && (s = send(sock, message, len, 0)) > 0) {
		message += s ;
		len -= s ;
	}
}

char * 
recv_message (int sock)
{
    char buf[1024] ;
    char * data = 0x0;
    int len = 0 ;
    int s ;
    while ((s = recv(sock, buf, 1023, 0)) > 0) {
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

    return data ;
}

char * 
recv_n_message (int sock, int n)
{
    char buf[1024] ;
    char * data = 0x0;
    int len = 0 ;
    int s ;
    if (len < n && (s = recv(sock, buf, n, 0)) > 0) {
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
    
    return data ;
}

void
send_meta_data (int sock)
{
    Node * itr = 0x0 ;
    pthread_mutex_lock(&m) ;
    for (itr = meta_data.next; itr != 0x0; itr = itr->next) {
        send_int(sock, itr->ver) ;
        send_int(sock, itr->name_len) ;
        send_message(sock, itr->file_name) ;
    }
    pthread_mutex_unlock(&m) ;
}

void
read_and_send (int sock, char * file_path)
{
    /*
        open file
        read and send the contents
        * err check for resp.header
    */
    FILE * r_fp = fopen(file_path, "rb") ;
    if (r_fp == NULL) {
        send_int(sock, 1) ;
    }

    while (feof(r_fp) == 0) {
        char buffer[1024] ;
        size_t r_len = fread(buffer, 1, sizeof(buffer), r_fp) ;
  
        char * buf_p = buffer ;
        // send
        int s ;
        while (r_len > 0 && (s = send(sock, buf_p, r_len, 0)) > 0) {
            buf_p += s ;
            r_len -= s ;
        }
    }

    fclose(r_fp) ;
}

void
recv_and_write (int sock, char * file_path)
{
    FILE * w_fp = fopen(file_path, "wb") ;
    if (w_fp == NULL) {
        send_int(sock, 1) ;
    }
    
    char buffer[1024] ;
    int s ;
    while ((s = recv(sock, buffer, 1024, 0)) > 0) {
        if(fwrite(buffer, 1, s, w_fp) != s) {
            // do something?
            printf("write wrong!\n") ;
        }
    }
    fclose(w_fp) ;
}