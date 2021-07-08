#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>

#define EVENT_SIZE ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN ( 1024 * ( EVENT_SIZE + 16 ) )

int fd ;
int wd ;

void
handler ( int sig )
{
    if ( sig == SIGINT ) {
        inotify_rm_watch( fd, wd ) ;
        close( fd ) ;
        exit(0) ;
    }
}

int 
main ()
{
    signal(SIGINT, handler) ;
    
    fd = inotify_init() ;
    if ( fd < 0 ) {
        perror( "inotify_init" ) ;
    }
    wd = inotify_add_watch( fd, "./tmp", IN_CREATE | IN_MOVED_TO | IN_MODIFY );

    while (1) {
        char buffer[EVENT_BUF_LEN];
        int length = read( fd, buffer, EVENT_BUF_LEN ); 
        if ( length < 0 ) {
            perror( "read" );
        }  

        int i = 0;
        while ( i < length ) {     
            struct inotify_event * event = ( struct inotify_event * ) &buffer[i] ;     
            if ( event->len ) {    
                if (event->name[0] != '.' && strcmp(event->name, "4913") != 0 && strstr(event->name, "/") == NULL && strstr(event->name, "~") == NULL) {
                    if ( event->mask & IN_CREATE || event->mask & IN_MOVED_TO ) {
                        if ( event->mask & IN_ISDIR ) {
                            printf( "> New directory %s created.\n", event->name );
                        }
                        else {
                            printf( "> New file %s created.\n", event->name );
                        }
                    }
                    else if ( event->mask & IN_MODIFY ) {
                        if ( event->mask & IN_ISDIR ) {
                            printf( "> Directory %s modified.\n", event->name );
                        }
                        else {
                            printf( "> File %s modified.\n", event->name );
                        }
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }
}
