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

#include "socket.h"

void
send_header (int sock, int h)
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
recv_header (int sock)
{
    int header ;
    int s = 0 ;
    int len = 0 ;
    while (s < 4 && (s = recv(sock, &header, 4, 0)) > 0) {
        len += s ;
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
read_and_send (int sock, char * file_path)
{
    /*
        open file
        read and send the contents
        * err check for resp.header
    */
    FILE * r_fp = fopen(file_path, "rb") ;
    if (r_fp == NULL) {
        send_header(sock, 1) ;
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
        send_header(sock, 1) ;
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