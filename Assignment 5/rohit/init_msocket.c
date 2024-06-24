#include "msocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
// header for usleep()
#include <unistd.h>
#include <semaphore.h>
#include <sys/ipc.h>  // Include for ftok()
#include <sys/sem.h>

#define MAX_IP_LENGTH 16

SOCK_INFO *sock_info1;
MTP_Socket_Info *SM1;
struct sembuf pop1, vop1 ;

//Define global semaphore variables
int Sem1, Sem2;

pthread_t thread_S, thread_R;

/*The thread S behaves in the following manner. It sleeps for some time ( < T/2 ), and wakes
up periodically. On waking up, it first checks whether the message timeout period (T) is over(by computing the time difference between the current time and the time when the messages
within the window were sent last) for the messages sent over any of the active MTP sockets.
If yes, it retransmits all the messages within the current swnd for that MTP socket. It then
checks the current swnd for each of the MTP sockets and determines whether there is a
pending message from the sender-side message buffer that can be sent. If so, it sends that
message through the UDP sendto() call for the corresponding UDP socket and updates the
send timestamp*/


bool is_timeout(int last_time, int curr_time){
    return curr_time - last_time > T;
}

// write a function which retransmits all messages withih the current swnd for that MTP socket
  void retransmit(int i){
    // create a strunbc sockaddr_in dest_addr
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SM1[i].other_end_port);
    dest_addr.sin_addr.s_addr = inet_addr(SM1[i].other_end_IP);
    time_t current_time; 
    //transmit all the messages within the current swnd for that MTP socket
      for (int j = SM1[i].sender_window.base; j < SM1[i].sender_window.base + SM1[i].sender_window.window_size; j++) {
            sender_buffer_element *element = &SM1[i].send_buffer[j];
            //send the udp message from this socket to the destination ip,port mentioned in sm[i]
            int msg_sent = sendto(SM1[i].udp_socket_id, element->message, strlen(element->message), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (msg_sent == -1) {
                perror("sendto");
                exit(EXIT_FAILURE);
            }
            time(&current_time); //get the current time
            //change the packet sent flag to 1
            element->packet_sent = 1;
            element->timer = current_time; // Update the send timestamp

        }
  }                          



// Function to access shared memory
 void access_shared_memory_1() {
    int shmid;
  

    // Get the shared memory segment
    if ((shmid = shmget(SHM_KEY, sizeof(MTP_Socket_Info) * NUM_MTP_SOCKETS, 0777|IPC_CREAT)) == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach to the segment to get a pointer to it
    if ((SM1 = shmat(shmid, NULL, 0)) == (MTP_Socket_Info *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    
}

void access_sockinfo_memory_1() {
    int shmid;
    

    // Get the shared memory segment
    if ((shmid = shmget(SOCK_KEY, sizeof(SOCK_INFO), 0777|IPC_CREAT)) == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach to the segment to get a pointer to it
    if ((sock_info1 = shmat(shmid, NULL, 0)) == (SOCK_INFO *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

}


// Function to initialize semaphores


void initialize_semaphores_1() {

   // use semget
    Sem1 = semget(SEM1_KEY, 1, 0777|IPC_CREAT);
    Sem2 = semget(SEM2_KEY, 1, 0777|IPC_CREAT);
    if (Sem1 == -1 || Sem2 == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }
    // use semctl to initialize the value of the semaphore to 0
   

  

    semctl(Sem1, 0, SETVAL, 0);
    semctl(Sem2, 0, SETVAL, 0);
    pop1.sem_num = vop1.sem_num = 0;
    pop1.sem_flg = vop1.sem_flg = 0;
    pop1.sem_op = -1 ; vop1.sem_op = 1 ;

 }

 void semaphore_wait1(int s){
    printf("semaphore wait\n");

    semop(s, &pop1, 1);
}
void semaphore_signal1(int s){
    semop(s, &vop1, 1);
}


int main() {
    // Step 1: Create shared memory structure SOCK_INFO
    int shmid;
    printf("Creating shared memory for SOCK_INFO\n");
    access_sockinfo_memory_1();
    access_shared_memory_1();
    
    
       // Create threads S and R
    // pthread_create(&thread_S, NULL, thread_S_function, NULL);
    // pthread_create(&thread_R, NULL, thread_R_function, NULL);
    //printf("Threads S and R created\n");
   

    // Initialize SOCK_INFO fields to 0
    memset(sock_info1, 0, sizeof(SOCK_INFO));
    memset(SM1, 0, sizeof(MTP_Socket_Info) * NUM_MTP_SOCKETS);
    
    
    for(int i = 0; i < NUM_MTP_SOCKETS; i++){
        SM1[i].udp_socket_id = -1;
        SM1[i].is_free = 1;
        SM1[i].sender_window.base = 0;
        SM1[i].sender_window.nextseqnum = 0;
        SM1[i].sender_window.window_size = 5;
        SM1[i].receiver_window.window_size = 0;
        SM1[i].receiver_window.nospace = 0;
        SM1[i].receiver_window.last_delivered_seqnum = 0;
        SM1[i].last_free_sender_buffer_index = 0;
        // for(int j = 0; j < 5; j++){
        //     SM[i].receiver_window.expectedseqnums[j] = j+1;
        // }
        for(int j = 0; j < 5; j++){
            SM1[i].send_buffer[j].in_use = 0;
            SM1[i].receive_buffer[j].in_use = 0;
        }

    }

    // Step 2: Create and initialize semaphores Sem1 and Sem2

    initialize_semaphores_1();
    initialize_semaphores();
    //wait on sem1

    // printf("value of sem1 is %d\n",semctl(Sem1, 0, GETVAL, 0));
    // printf("value of sem2 is %d\n",semctl(Sem2, 0, GETVAL, 0));
    // semaphore_wait1(Sem1);

    // printf("value of sem1 is %d\n",semctl(Sem1, 0, GETVAL, 0));
    // printf("value of sem2 is %d\n",semctl(Sem2, 0, GETVAL, 0));
    // sleep(15);
    // Step 3: Main thread logic
    while (1) {
        // Step 3(a): Wait on Sem1
        printf("Waiting on Sem1\n");
        semaphore_wait1(Sem1);

        // Step 3(b): Look at SOCK_INFO
        if (sock_info1->sock_id == 0 && sock_info1->src_ip == 0 && sock_info1->src_port==0 && sock_info1->dst_ip == 0 && sock_info1->dst_port==0&& sock_info1->error_number == 0) {
            // It's an m_socket call
            // Create a UDP socket
            int udp_sock_id = socket(AF_INET, SOCK_DGRAM, 0);
            if (udp_sock_id == -1) {
                // If error, put -1 in sock_id field and errno in errno field
                sock_info1->sock_id = -1;
                sock_info1->error_number = errno;
            } else {
                // Put the socket id returned in the sock_id field of SOCK_INFO
                sock_info1->sock_id = udp_sock_id;
            }
        } else {
            // It's an m_bind call
            // Make a bind() call on the sock_id value, with the IP and port given
            int sockfd = sock_info1->sock_id;
            const char *src_ip = sock_info1->src_ip;
            int src_port = sock_info1->src_port; 

            const char *des_ip = sock_info1->dst_ip;
            int des_port = sock_info1->dst_port;
            int pid = sock_info1->creator_pid;

            struct sockaddr_in src_addr;
            src_addr.sin_family = AF_INET;
            src_addr.sin_port = htons(src_port);
            src_addr.sin_addr.s_addr = inet_addr(src_ip);

            // Perform the bind call
            if (bind(sockfd, (struct sockaddr *)&src_addr, sizeof(src_addr)) <0) {
                sock_info1->error_number = errno;
                sock_info1->sock_id = -1;
            }
            else{
                for (int i = 0; i < NUM_MTP_SOCKETS; i++) {
                    if (SM1[i].udp_socket_id == sockfd) {
                        strncpy(SM1[i].other_end_IP, des_ip, MAX_IP_LENGTH);
                        SM1[i].other_end_port = des_port;
                        SM1[i].is_free = 0;
                        SM1[i].creator_pid=pid;
                        break;
                        // Return the index if found
                    }
            }
            }
            
        }

        // Step 3(c): Signal on Sem2
        semaphore_signal1(Sem2);
    }

    // Clean up: Detach shared memory
    if (shmdt(sock_info1) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    //  pthread_join(thread_S, NULL);
    // pthread_join(thread_R, NULL);


    return 0;
}




