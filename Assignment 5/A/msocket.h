#ifndef MSOCKET_H 
#define MSOCKET_H 

#include <netinet/in.h>
#include <errno.h>
#include <semaphore.h>// Global shared memory key and size
#include <pthread.h>

// Define the socket type for MTP
#define SOCK_MTP 3

// Maximum length for IP addresses
#define MAX_IP_LENGTH 16

// Maximum message length
#define MAX_MSG_LENGTH 1024

// Global shared memory key and size msocket.o
#define SHM_KEY 1234
#define SOCK_KEY 5678
#define NUM_MTP_SOCKETS 25
// extern sem_t *Sem1, *Sem2;

// Define the error numbers
#define M_EPERM 1

// Define semaphore names
#define SEM1_KEY 9876
#define SEM2_KEY 9877
// set T to 5 seconds
#define T 5
extern float probability_of_dropping =0.5;
//structure for the messagebuffer which has message,sequence number ,timer,acknowledgement
typedef struct {
    int in_use;//to check if the buffer is in use
    char message[MAX_MSG_LENGTH];
    int seqnum;
    time_t timer;
    int ack;
    int packet_sent;//whether the packet is sent or not

} sender_buffer_element;

// structure for the receiver buffer element
typedef struct {
    int in_use; // to check if the buffer is in use
    char message[MAX_MSG_LENGTH];
    int seqnum; // sequence number of received message
    int ack; // whether received packet is acknowledged or not
} receiver_buffer_element;

// structure foe swnd(window information sender side)
typedef struct {
    int base;
    int nextseqnum;
    int window_size;
    // int *ack;//acknowledgement received;
    // int *timer;

} swnd;


// structure foe rwnd(window information receiver side)
typedef struct {
    // int expectedseqnums[5];
    int window_size;  // if window size ==0 then no space in window or reciever buffer
    int nospace;
    int last_delivered_seqnum;
    // intsem_t *Sem1, *Sem2; *packet; // 
} rwnd;


// Structure for MTP socket information
typedef struct {
    int is_free;
    pid_t creator_pid;
    int udp_socket_id;
    char other_end_IP[MAX_IP_LENGTH];
    int other_end_port;
    sender_buffer_element send_buffer[10];
    int last_free_sender_buffer_index;
    swnd sender_window;
    rwnd receiver_window;
    receiver_buffer_element receive_buffer[5];
    // Add sender and receiver window structures here
} MTP_Socket_Info;





typedef struct {
    int sock_id;
    char src_ip[MAX_IP_LENGTH];
    char dst_ip[MAX_IP_LENGTH];
    int src_port;
    int dst_port;
    int error_number;
    pid_t creator_pid;
} SOCK_INFO;

// Function prototypes
int m_socket(int domain, int type, int protocol);
// int m_bind(int sockfd, const struct sockaddr *src_addr, socklen_t addrlen, const char *dst_ip, int dst_port);
// ssize_t m_sendto(int sockfd, const void *buf, size_t len, int flags, const char *dst_ip, int dst_port);
// ssize_t m_recvfrom(int sockfd, void *buf, size_t len, int flags);
int m_close(int sockfd);
ssize_t m_sendto(int sockfd, const void *buf, size_t len, int flags, const char *dst_ip, int dst_port);
ssize_t m_recvfrom(int sockfd, void *buf, size_t len, int flags, char *src_ip, int *src_port);
int m_bind(int sockfd, const char *src_ip, int src_port, const char *dst_ip, int dst_port);
// Global error variable
extern int m_errno;
// In your header file (e.g., msocket.h)
// extern sem_t Sem1;
// extern sem_t Sem2;
void initialize_semaphores();
void destroy_semaphores();
void semaphore_wait(int s);
void semaphore_signal(int s);
MTP_Socket_Info* access_shared_memory();
void print_SM_table();
void print_sender_buffer_element(const sender_buffer_element *sender_buffer) ;
void print_receiver_buffer_element(const receiver_buffer_element *receiver_buffer) ;
void print_sender_receiver_buffers(const MTP_Socket_Info *SM, int index) ;
void detach_shared_memory(MTP_Socket_Info *SM) ;
void detach_sockinfo_memory(SOCK_INFO *sock_info) ;

SOCK_INFO* access_sockinfo_memory() ;
void signal_sem1_wait_sem2() ;
void wait_sem1_signal_sem2();
// SOCK_INFO *sock_info;
extern pthread_mutex_t mutex;

// MTP_Socket_Info *SM;

#endif /* MSOCKET_H */
