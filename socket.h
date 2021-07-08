typedef struct _node {
    unsigned int ver ;
    unsigned int name_len ;
    char * file_name ;
    struct _node * next ;
} Node ;

void print_meta_data () ;

int is_exist (char * file_name) ;

void append_meta_data (char * file_name, int ver) ;

int increase_version (char * file_name) ;

int update_version (char * file_name, int ver) ;

int get_version (char * file_name) ;

int find_meta_data (Node * node) ;

void send_int (int sock, int h) ;

int recv_int (int sock) ;

void send_message(int sock, char * message) ;

char * recv_message (int sock) ;

char * recv_n_message (int sock, int n) ;

void send_meta_data (int sock) ;

void read_and_send (int sock, char * file_path) ;

void recv_and_write (int sock, char * file_path) ;