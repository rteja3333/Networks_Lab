#include "msocket.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/select.h>
#include <semaphore.h>
 #include <sys/sem.h>
#include <sys/ipc.h>  

// Global semaphore variables
int initialized = 0;
MTP_Socket_Info *SM;
SOCK_INFO *sock_info;


struct sembuf pop, vop ;
void semaphore_wait(int s){
    printf("semaphore wait\n");
        // printf("value of s %d\n",s);

    semop(s, &pop, 1);
    // printf("value of s %d\n",s);
}

void semaphore_signal(int s){
    printf("semaphore signal\n");
    semop(s, &vop, 1);
}

//function to print  SM (all fields of the SM table) in table format
void print_SM_table(MTP_Socket_Info *SM){
    printf("Printing SM table\n");
    printf("Index\tUDP Socket ID\tIs Free\n");
    for(int i = 0; i < NUM_MTP_SOCKETS; i++){
        printf("%d\t%d\t%d\n", i, SM[i].udp_socket_id, SM[i].is_free);
    }
}


// Function to initialize semaphores

void initialize_semaphores() {
    int Sem1, Sem2;
   // use semget
    Sem1 = semget(SEM1_KEY, 1, 0777|IPC_CREAT);
    Sem2 = semget(SEM2_KEY, 1, 0777|IPC_CREAT);
    if (Sem1 == -1 || Sem2 == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }
    // use semctl to initialize the value of the semaphore to 0
   

    // semctl(Sem1, 0, SETVAL, 0);
    // semctl(Sem2, 0, SETVAL, 0);
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1 ; vop.sem_op = 1 ;

 }

// Function to destroy semaphores
void destroy_semaphores() {
    int Sem1, Sem2;
   // use semget
    Sem1 = semget(SEM1_KEY, 1, 0777|IPC_CREAT);
    Sem2 = semget(SEM2_KEY, 1, 0777|IPC_CREAT);
    if (Sem1 == -1 || Sem2 == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }
    
    // use semctl to destroy the semaphores
    semctl(Sem1, 0, IPC_RMID,0);
    semctl(Sem2, 0, IPC_RMID,0);
    
}


// Function to access shared memory
 void access_shared_memory() {
    int shmid;
  

    // Get the shared memory segment
    if ((shmid = shmget(SHM_KEY, sizeof(MTP_Socket_Info) * NUM_MTP_SOCKETS, 0777|IPC_CREAT)) == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach to the segment to get a pointer to it
    if ((SM = shmat(shmid, NULL, 0)) == (MTP_Socket_Info *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    
}

void access_sockinfo_memory() {
    int shmid;
    

    // Get the shared memory segment
    if ((shmid = shmget(SOCK_KEY, sizeof(SOCK_INFO), 0777|IPC_CREAT)) == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach to the segment to get a pointer to it
    if ((sock_info = shmat(shmid, NULL, 0)) == (SOCK_INFO *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

}

// Function to signal Sem1 and wait on Sem2
void signal_sem1_wait_sem2() {
    int Sem1, Sem2;
   // use semget
    Sem1 = semget(SEM1_KEY, 1, 0777|IPC_CREAT);
    Sem2 = semget(SEM2_KEY, 1, 0777|IPC_CREAT);

    printf("value of sem1 %d\n",semctl(SEM1_KEY, 0, GETVAL, 0));
    printf("value of sem2 %d\n",semctl(SEM2_KEY, 0, GETVAL, 0));
    if (Sem1 == -1 || Sem2 == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }
    // Signal on Sem1 and wait on Sem2
    semaphore_signal(Sem1);
    semaphore_wait(Sem2);
}

// Function to wait on Sem1 and signal Sem2
void wait_sem1_signal_sem2() {
    int Sem1, Sem2;
   // use semget
    Sem1 = semget(SEM1_KEY, 1, 0777|IPC_CREAT);
    Sem2 = semget(SEM2_KEY, 1, 0777|IPC_CREAT);
    if (Sem1 == -1 || Sem2 == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }
    // Wait on Sem1 and signal on Sem2
    semaphore_wait(Sem1);
    semaphore_signal(Sem2);
}

// Function to create a socket


int m_socket(int domain, int type, int protocol) {
    int Sem1, Sem2;
   // use semget
    Sem1 = semget(SEM1_KEY, 1, 0777|IPC_CREAT);
    Sem2 = semget(SEM2_KEY, 1, 0777|IPC_CREAT);
    if (Sem1 == -1 || Sem2 == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }
    if(!initialized){
        access_shared_memory();
        access_sockinfo_memory();
        initialized = 1;
    }
   
    int i;

    // Find a free entry in SM
    for (i = 0; i < NUM_MTP_SOCKETS; i++) {
        if (SM[i].is_free) {
            SM[i].is_free = 0;
            break;
        }
    }

    if (i == NUM_MTP_SOCKETS) {
        errno = ENOBUFS;
        return -1;
    }

    // Signal on Sem1 and wait on Sem2
    //print values of sem1 and sem2 values of semaphores
    // printf("value of sem1 %d\n",semctl(SEM1_KEY, 0, GETVAL, 0));
    // printf("value of sem2 %d\n",semctl(SEM2_KEY, 0, GETVAL, 0));


    signal_sem1_wait_sem2();
    printf("value of sem1 %d\n",semctl(SEM1_KEY, 0, GETVAL, 0));
    printf("value of sem2 %d\n",semctl(SEM2_KEY, 0, GETVAL, 0));

    // Check if the sockP, port,, e_id field of SOCK_INFO is -1
    if (sock_info->sock_id == -1) {
        errno = sock_info->error_number;
        memset(sock_info, 0, sizeof(SOCK_INFO)); // Reset all fields of SOCK_INFO to 0
        return -1;
    }

    // Put UDP socket id returned in that field in SM table
    SM[i].udp_socket_id = sock_info->sock_id;

    // Reset all fields of SOCK_INFO to 0
    memset(sock_info, 0, sizeof(SOCK_INFO));

    printf("printing in m_socket\n");
    printf("Socket created with UDP socket ID: %d\n", SM[i].udp_socket_id);
    print_SM_table(SM);
    return i;
}


int m_bind(int sockfd, const char *src_ip, int src_port, const char *dst_ip, int dst_port) {
    if(!initialized){
        access_shared_memory();
        access_sockinfo_memory();
        initialized = 1;
    }

    int udp_sock_id = sockfd;

    // Find the corresponding actual UDP socket id from SM table
    // Assume sockfd is the index of the entry in SM table
    if (sockfd < 0 || sockfd >= NUM_MTP_SOCKETS) {
        errno = EINVAL;
        return -1;
    }

    // Put UDP socket ID, IP, and port in SOCK_INFO table
    strcpy(sock_info->dst_ip, dst_ip);
    strcpy(sock_info->src_ip, src_ip);

    sock_info->src_port = src_port;
    sock_info->dst_port= dst_port;
    
    sock_info->sock_id = SM[sockfd].udp_socket_id;
    sock_info->creator_pid = getpid();
    // Signal Sem1 and wait on Sem2
    signal_sem1_wait_sem2();
   
    // Check if the sock_id field of SOCK_INFO is -1
    if (sock_info->sock_id == -1) {
        errno = sock_info->error_number; 
         memset(&sock_info, 0, sizeof(sock_info));
        return -1;
    }
    memset(&sock_info, 0, sizeof(sock_info));
      // Reset all fields of SOCK_INFO to 0
    printf("printing in m_bind\n");
    print_SM_table(SM);
    return 1;
}


// Function to send data to the other end , it just writes in the sender buffer (not socketinfo) of corressponding socket(the s thread does the actual sending)
ssize_t m_sendto(int sockfd, const void *buf, size_t len, int flags, const char *dst_ip, int dst_port) {
    
    int udp_sock_id = sockfd;

    // Find the corresponding udp socket id from SM table using sockfd
    // Assume sockfd is the index of the entry in SM table
    if (sockfd < 0 || sockfd >= NUM_MTP_SOCKETS) {
        errno = EINVAL;
        return -1;
    }

    // Write data to the sender buffer at the last free index(and if not in use) of the correspondiong sender buffer in SM table and increase the index of that last free index, if >10 again start from 0
    int i = SM[sockfd].last_free_sender_buffer_index;
    //write the data to the sender buffer in that index
    if(SM[sockfd].send_buffer[i].in_use == 1){
        errno = ENOBUFS;
        return -1;
    }
    SM[sockfd].send_buffer[i].in_use = 1;
    SM[sockfd].send_buffer[i].packet_sent = 0;
    memcpy(SM[sockfd].send_buffer[i].message, buf, len);
    // SM[sockfd].send_buffer[i]. = len;
    SM[sockfd].last_free_sender_buffer_index = (i + 1) % 10;
    return len;
}

// Function to receive data from the other end
ssize_t m_recvfrom(int sockfd, void *buf, size_t len, int flags, char *src_ip, int *src_port) {
   
    int udp_sock_id = sockfd;

    // Find the corresponding udp socket id from SM table using sockfd
    // Assume sockfd is the index of the entry in SM table
    if (sockfd < 0 || sockfd >= NUM_MTP_SOCKETS) {
        errno = EINVAL;
        return -1;
    }
    //take the least seqno packet from the receiver buffer and copy it to the buf and delete it from the receiver buffer and update the last_delivered_seqnum and rwnd size
    int i = SM[sockfd].receiver_window.last_delivered_seqnum;
    if(SM[sockfd].receiver_window.window_size == 0){
        errno = ENOBUFS;
        return -1;
    }
    SM[sockfd].receiver_window.window_size++;
    SM[sockfd].is_free = 1;
    SM[sockfd].receiver_window.last_delivered_seqnum =SM[sockfd].receive_buffer[i].seqnum;
    memcpy(buf, SM[sockfd].receive_buffer[i].message, len);

    return len;
}

//function to close the socket
int m_close(int sockfd) {
    
    int udp_sock_id = sockfd;

    // Find the corresponding udp socket id from SM table using sockfd
    // Assume sockfd is the index of the entry in SM table
    if (sockfd < 0 || sockfd >= NUM_MTP_SOCKETS) {
        errno = EINVAL;
        return -1;
    }

    // Reset all fields of the corresponding entry in SM table
    SM[sockfd].is_free = 1;
    SM[sockfd].udp_socket_id = -1;
    SM[sockfd].last_free_sender_buffer_index = 0;
    SM[sockfd].sender_window.base = 0;
    SM[sockfd].sender_window.nextseqnum = 0;
    SM[sockfd].sender_window.window_size = 0;
    SM[sockfd].receiver_window.window_size = 0;
    SM[sockfd].receiver_window.nospace = 0;
    SM[sockfd].receiver_window.last_delivered_seqnum = 0;

    return 0;
}

// Implement other functions...
