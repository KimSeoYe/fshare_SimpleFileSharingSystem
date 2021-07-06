/*This is the sample program to notify us for the file creation and file deletion takes place in “/tmp” directory*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
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
        /* removing the “/tmp” directory from the watch list. */
        inotify_rm_watch( fd, wd ) ;
        /* closing the INOTIFY instance */
        close( fd ) ;
        exit(0) ;
    }
}

int 
main ()
{
    signal(SIGINT, handler) ;
    
    /* creating the INOTIFY instance */
    fd = inotify_init();
    if ( fd < 0 ) {
        perror( "inotify_init" );
    }

    /* 
        adding the “/tmp” directory into watch list. 
        Here, the suggestion is to validate the existence of the directory before adding into monitoring list.
    */
    wd = inotify_add_watch( fd, "./tmp", IN_CREATE | IN_MOVED_TO | IN_MODIFY );

    while (1) {
    /* 
        read to determine the event change happens on “./tmp” directory. 
        Actually this read blocks until the change event occurs
    */ 
        char buffer[EVENT_BUF_LEN];
        int length = read( fd, buffer, EVENT_BUF_LEN ); 
        if ( length < 0 ) {
            perror( "read" );
        }  

        /* 
            actually read return the list of change events happens. 
            Here, read the change event one by one and process it accordingly.
        */
        int i = 0;
        while ( i < length ) {     
            struct inotify_event * event = ( struct inotify_event * ) &buffer[i] ;     
            if ( event->len ) {
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
            i += EVENT_SIZE + event->len;
        }
    }
}



/*
     struct inotify_event {
        int      wd;       // Watch descriptor 
        uint32_t mask;     // Mask describing event 
        uint32_t cookie;   // Unique cookie associating related events (for rename(2)) 
        uint32_t len;      // Size of name field (4byte)
        char     name[];   // Optional null-terminated name 
    }
*/