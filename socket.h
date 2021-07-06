typedef struct _node {
    unsigned int ver ;
    unsigned int name_len ;
    char * file_name ;
    struct _node * next ;
} Node ;

void print_meta_data () ;

void append (int ver, char * file_name) ;

void update_version (char * file_name) ;

void send_header (int sock, int h) ;

int recv_header (int sock) ;

void send_message(int sock, char * message) ;

char * recv_message (int sock) ;

char * recv_n_message (int sock, int n) ;

void recv_meta_data (int sock) ;

void read_and_send (int sock, char * file_path) ;

void recv_and_write (int sock, char * file_path) ;