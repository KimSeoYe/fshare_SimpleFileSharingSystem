void send_header (int sock, int h) ;

int recv_header (int sock) ;

void send_message(int sock, char * message) ;

char * recv_message (int sock) ;
char * recv_n_message (int sock, int n) ;

void read_and_send (int sock, char * file_path) ;

void recv_and_write (int sock, char * file_path) ;