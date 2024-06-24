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
#include <unistd.h>
#include <semaphore.h>
#include <sys/ipc.h>  
#include <sys/sem.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
// pthread_mutex_t mutex;

pthread_t thread_S, thread_R,gc_thread;

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
    int Sem1, Sem2;
   // use semget
    Sem1 = semget(SEM1_KEY, 1, 0777|IPC_CREAT);
    Sem2 = semget(SEM2_KEY, 1, 0777|IPC_CREAT);
    if (Sem1 == -1 || Sem2 == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }
        
    MTP_Socket_Info* SM=   access_shared_memory();
    SOCK_INFO* sock_info=   access_sockinfo_memory();
     
    // create a strunbc sockaddr_in dest_addr
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SM[i].other_end_port);
    dest_addr.sin_addr.s_addr = inet_addr(SM[i].other_end_IP);
    time_t current_time; 
    //transmit all the messages within the current swnd for that MTP socket
      for (int j = SM[i].sender_window.base; j < SM[i].sender_window.base + SM[i].sender_window.window_size; j++) {
            sender_buffer_element *element = &SM[i].send_buffer[j];
            //send the udp message from this socket to the destination ip,port mentioned in sm[i]
            int msg_sent = sendto(SM[i].udp_socket_id, element->message, strlen(element->message), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
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


int dropMessage(float p) {
    // Generate a random number between 0 and 1
    float random_num = ((float)rand()) / RAND_MAX;
    
    // Check if the random number is less than the given probability
    if (random_num < p) {
        // If yes, return 1 to indicate message drop
        return 1;
    } else {
        // If not, return 0 to indicate message acceptance
        return 0;
    }
}

void* thread_S_function(void* arg) {
    int Sem1, Sem2;
   // use semget
    Sem1 = semget(SEM1_KEY, 1, 0777|IPC_CREAT);
    Sem2 = semget(SEM2_KEY, 1, 0777|IPC_CREAT);
    if (Sem1 == -1 || Sem2 == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }
        
    MTP_Socket_Info* SM=   access_shared_memory();
    SOCK_INFO* sock_info=   access_sockinfo_memory();
     

    while (1) {
        // Sleep for some time (< T/2)
        usleep(5000000); //sleep time: 5 seconds

        // Iterate over all MTP sockets
        for (int i = 0; i < NUM_MTP_SOCKETS; i++) {
            if (!SM[i].is_free) {
                printf("STHREAD: sending all messages for socket:%d\n",i);
                // Iterate over the sender-side window
                int j;
                for ( j = SM[i].sender_window.base; j < SM[i].sender_window.base + SM[i].sender_window.window_size; j++) {
                    sender_buffer_element *element = &SM[i].send_buffer[j];
                    if (element->in_use) {
                        if(!element->packet_sent){
                            break;
                        }
                        // Check if the timeout period is over for this message
                        time_t current_time;
                        time(&current_time);//get the current time
                        double time_diff = difftime(current_time, element->timer);
                        if (time_diff >= T && !element->ack) {
                            
                            // Retransmit all the messages within the current swnd for that MTP socket
                            retransmit(i);
                            break;
                        }
                    }
                    }
                // Check for pending messages in the sender-side message buffer
                for (int k = j; k < SM[i].sender_window.base + SM[i].sender_window.window_size; k++) {
                    
                    sender_buffer_element *element = &SM[i].send_buffer[k];
                     struct sockaddr_in dest_addr;
                    dest_addr.sin_family = AF_INET;
                    dest_addr.sin_port = htons(SM[i].other_end_port);
                    dest_addr.sin_addr.s_addr = inet_addr(SM[i].other_end_IP);
                    time_t current_time; 
                    //send message only if packeet was not sent and the message is not acknowledged
                    if (element->in_use&&!element->packet_sent && !element->ack) {
                    //send the udp message from this socket to the destination ip,port mentioned in sm[i]
                    int msg_sent = sendto(SM[i].udp_socket_id, element->message, strlen(element->message), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                    printf("s:message sent from socket%d , message:%s\n ",i,element->message);
                    if (msg_sent == -1) {
                        perror("sendto");
                        exit(EXIT_FAILURE);
                    }
                    time(&current_time); //get the current time
                    element->timer = current_time; // Update the send timestamp
                    }
              }


               
            }
        }
    }

    return NULL;
}

void* thread_R_function(void* arg) {
    int Sem1, Sem2;
   // use semget
    Sem1 = semget(SEM1_KEY, 1, 0777|IPC_CREAT);
    Sem2 = semget(SEM2_KEY, 1, 0777|IPC_CREAT);
    if (Sem1 == -1 || Sem2 == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }
        
    MTP_Socket_Info* SM=   access_shared_memory();
    SOCK_INFO* sock_info=   access_sockinfo_memory();
     
    fd_set readfds;
    int max_fd = 0;
    struct timeval timeout;
    timeout.tv_sec = 5; // Timeout value in seconds
    timeout.tv_usec = 0;

    while (1) {
        FD_ZERO(&readfds);

        // Add all UDP sockets to the read set
        for (int i = 0; i < NUM_MTP_SOCKETS; i++) {
            if (!SM[i].is_free) {
                FD_SET(SM[i].udp_socket_id, &readfds);
                if (SM[i].udp_socket_id > max_fd) {
                    max_fd = SM[i].udp_socket_id;
                }
            }
        }

        // Call select to wait for incoming messages
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            continue;
            //chance of socket closure (because of process termination , garbage process closed it)
            // perror("select error");
            // exit(EXIT_FAILURE);
        } else if (activity == 0) {
            // Timeout occurred

            // Check for space availability in the receive buffer
            // forr each socket chjeck the recieve window,If size >0 or buffer is available and nospace flag was set, send a duplicate ACK
            for (int i = 0; i < NUM_MTP_SOCKETS; i++) {
                if (!SM[i].is_free) {
                    if (SM[i].receiver_window.window_size > 0 && SM[i].receiver_window.nospace) {
                        // Send a duplicate ACK
                       
                    char ack[2];
                    ack[0] = '0';
                    //add lastdelivered seqnum to ack[1] ,ack[2]
                    ack[1] =( SM[i].receiver_window.last_delivered_seqnum)/10 + '0';
                    ack[2] = (SM[i].receiver_window.last_delivered_seqnum)%10 + '0';
                    //add size of receiver window to ack[3]
                    ack[3] = SM[i].receiver_window.window_size + '0';
                        struct sockaddr_in dest_addr;
                        dest_addr.sin_family = AF_INET;
                        dest_addr.sin_port = htons(SM[i].other_end_port);
                        dest_addr.sin_addr.s_addr = inet_addr(SM[i].other_end_IP);
                        sendto(SM[i].udp_socket_id, ack, 4, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                    }
                }
            }

            // Code for checking and sending duplicate ACK goes here...
        } else {
            // Incoming message on one of the UDP sockets
            for (int i = 0; i < NUM_MTP_SOCKETS; i++) {
                if (!SM[i].is_free && FD_ISSET(SM[i].udp_socket_id, &readfds)) {
                    struct sockaddr_in other_end_addr;
                    socklen_t addr_len = sizeof(other_end_addr);
                    char buffer[MAX_MSG_LENGTH];

                    // Receive message
                    ssize_t recv_len = recvfrom(SM[i].udp_socket_id, buffer, MAX_MSG_LENGTH, 0,
                                                (struct sockaddr *)&other_end_addr, &addr_len);
                    if (recv_len < 0) {
                        perror("recvfrom error");
                        exit(EXIT_FAILURE);
                    }
                    // Check if the message should be dropped
                    
                    if (dropMessage(probability_of_dropping)) {
                        // Message dropped, so continue to receive the next message*******************************************
                        continue;
                    }

                    // Check if the message is an ACK or a data message if data message then extract the sequence number and send an ack
                    // first bit in buffer is 0 then it is an ack, else it is a data message
                    if (buffer[0] == '0') {
                        // Extract the sequence number from the message
                        // sequence number is in buffer[1] and buffer[2]
                         int seqnum = (buffer[1] - '0')*10 + (buffer[2] - '0');
                        
                        // Update the sender window
                        // find the index of the corresponding sequence number in sender buffer by iterating over the sender buffer
                        int index;
                        for (int j = SM[i].sender_window.base; j < SM[i].sender_window.base + SM[i].sender_window.window_size; j++) {
                            sender_buffer_element *element = &SM[i].send_buffer[j];
                            if (element->seqnum == seqnum) {
                                index = j;
                                element->ack = 1;
                                if(j == SM[i].sender_window.base){
                                    SM[i].sender_window.base++;
                                    SM[i].sender_window.window_size--;
                                }
                                break;
                            }
                        }
                        //check if received ack is duplicate by checking j value if j =  SM[i].sender_window.base + SM[i].sender_window.window_size then it is a duplicate ack
                         


                    } 
                else {
                        // Extract the sequence number from the message
                        int seqnum = (buffer[1] - '0')*10 + (buffer[2] - '0');
                        
                //assuming that message seqnum is in expected sequence number
                // copy this received message(remove all headers) to the first empty slot in the receiver buffer
                // find the first empty slot in the receiver buffer and copy the message to that slot
                for (int j = 0; j < 5; j++) {
                    receiver_buffer_element *element = &SM[i].receive_buffer[j];
                    if (!element->in_use) {
                        element->in_use = 1;
                        element->seqnum = seqnum;
                        strcpy(element->message, buffer+3);
                        break;
                    }
                }
                //check how many buffer elements are free by iterating from 0 to 5 and update receiver window size
                SM[i].receiver_window.window_size = 0;   
                for(int j = 0; j < 5; j++){
                    if(SM[i].receive_buffer[j].in_use){
                        SM[i].receiver_window.window_size++;
                    }
                }
                //set nospace flag to 0 if the receiver window is full
                if(SM[i].receiver_window.window_size == 0){
                    SM[i].receiver_window.nospace = 1;
                }

                // now check using the last delivered seqnumber, how many inorder messages are there in the receiver
                // buffer  deliver them to user and remove them from the buffer and update the receiver window 
                // and atlast send ack for the last inordered message delivered (only it was delivered now newly)
                // int flag = 0;
                // for(int j = 0; j < 5; j++){
                //     receiver_buffer_element *element = &SM[i].receive_buffer[j];
                //     if(element->seqnum == SM[i].receiver_window.last_delivered_seqnum + 1){
                //         flag = 1;
                //         SM[i].receiver_window.last_delivered_seqnum++;
                //         element->in_use = 0;
                //         SM[i].receiver_window.window_size++;
                //         // SM[i].receiver_window.nospace = 0;
                //         //deliver the message to the user
                //         printf("Message received: %s\n", element->message);
                //     }
                // }
                // //send ack for the last delivered message by checking flag
                // if(flag){
                //     char ack[2];
                //     ack[0] = '0';
                //     //add lastdelivered seqnum to ack[1] ,ack[2]
                //     ack[1] =( SM[i].receiver_window.last_delivered_seqnum)/10 + '0';
                //     ack[2] = (SM[i].receiver_window.last_delivered_seqnum)%10 + '0';
                //     //add size of receiver window to ack[3]
                //     ack[3] = SM[i].receiver_window.window_size + '0';
                //     sendto(SM[i].udp_socket_id, ack, 2, 0, (struct sockaddr *)&other_end_addr, addr_len);
                // }


            }
        }
    }

    return NULL;
}
}
}


#define CLEANUP_INTERVAL 10 // Cleanup interval in seconds

// Function to check and cleanup MTP sockets
void *garbage_collector(void *arg) {
    while (1) {
        // Iterate through MTP socket array and check process status
        MTP_Socket_Info* SM = access_shared_memory();
        for (int i = 0; i < NUM_MTP_SOCKETS; i++) {
            if (!SM[i].is_free) { // If socket is in use
                pid_t pid = SM[i].creator_pid;
                if (pid != 0 && kill(pid, 0) == -1 && errno == ESRCH) {
                    // Process is terminated, close socket
                    close(SM[i].udp_socket_id);
                    SM[i].is_free = 1; // Mark socket as free
                    printf("Closed socket for terminated process %d\n", pid);
                }
            }
        }
        detach_shared_memory(SM); // Detach shared memory
        sleep(CLEANUP_INTERVAL); // Sleep for cleanup interval
    }
    return NULL;
}


void cleanup() {
    // Close socket file descriptors
    MTP_Socket_Info* SM=  access_shared_memory();

    for (int i = 0; i < NUM_MTP_SOCKETS; i++) {
        if(!SM[i].is_free){
        close(SM[i].udp_socket_id);
        }
    }

    // Remove shared memory segment
    // if (shmctl(shmid, IPC_RMID, NULL) == -1) {
    //     perror("shmctl");
    //     exit(EXIT_FAILURE);
    // }

    printf("Cleanup complete.\n");
}

void sigint_handler(int signum) {
    printf("\nSIGINT received. Performing cleanup...\n");
    cleanup();
    exit(EXIT_SUCCESS);
}

int main() {
    // Step 1: Create shared memory structure SOCK_INFO
    signal(SIGINT,sigint_handler);
    int shmid;
    printf("Creating shared memory for SOCK_INFO\n");
    int Sem1, Sem2;
   // use semget
    Sem1 = semget(SEM1_KEY, 1, 0777|IPC_CREAT);
    Sem2 = semget(SEM2_KEY, 1, 0777|IPC_CREAT);
    if (Sem1 == -1 || Sem2 == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }
        
        if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }
    
    
    
       // Create threads S and R
    // pthread_create(&thread_S, NULL, thread_S_function, NULL);
    // pthread_create(&thread_R, NULL, thread_R_function, NULL);
    if (pthread_create(&gc_thread, NULL, garbage_collector, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    printf("Threads S and R created\n");
   
         MTP_Socket_Info* SM_init;

    // Get the shared memory segment
    if ((shmid = shmget(ftok(".",'1'), sizeof(MTP_Socket_Info) * NUM_MTP_SOCKETS, 0777|IPC_CREAT)) == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach to the segment to get a pointer to it
    if ((SM_init = shmat(shmid, NULL, 0)) == (MTP_Socket_Info *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    int shmid1;
    SOCK_INFO* sock_info_init;

    // Get the shared memory segment
    if ((shmid1 = shmget(ftok(".",'2'), sizeof(SOCK_INFO), 0777|IPC_CREAT)) == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach to the segment to get a pointer to it
    if ((sock_info_init = shmat(shmid1, NULL, 0)) == (SOCK_INFO *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
   


    // Initialize SOCK_INFO fields to 0
    memset(sock_info_init, 0, sizeof(SOCK_INFO));
    memset(SM_init, 0, sizeof(MTP_Socket_Info) * NUM_MTP_SOCKETS);
    
    
    for(int i = 0; i < NUM_MTP_SOCKETS; i++){
        SM_init[i].udp_socket_id = -1;
        SM_init[i].is_free = 1;
        SM_init[i].sender_window.base = 0;
        SM_init[i].sender_window.nextseqnum = 0;
        SM_init[i].sender_window.window_size = 5;
        SM_init[i].receiver_window.window_size = 0;
        SM_init[i].receiver_window.nospace = 0;
        SM_init[i].receiver_window.last_delivered_seqnum = 0;
        SM_init[i].last_free_sender_buffer_index = 0;
        // for(int j = 0; j < 5; j++){
        //     SM[i].receiver_window.expectedseqnums[j] = j+1;
        // }
        for(int j = 0; j < 5; j++){
            SM_init[i].send_buffer[j].in_use = 0;
            SM_init[i].send_buffer[9-j].in_use = 0;

            SM_init[i].receive_buffer[j].in_use = 0;
        }

    }
    memset(sock_info_init, 0, sizeof(SOCK_INFO));


    printf("\nInitialised the table and sockinfo\n\n");
    // Step 3: Main thread logic
    while (1) {
        // Step 3(a): Wait on Sem1
        printf("Waiting on Sem1\n");
        semaphore_wait(Sem1);

    //      MTP_Socket_Info* SM_init;
    // int shmid;

    // Get the shared memory segment
    if ((shmid = shmget(ftok(".",'1'), sizeof(MTP_Socket_Info) * NUM_MTP_SOCKETS, 0777|IPC_CREAT)) == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach to the segment to get a pointer to it
    if ((SM_init = shmat(shmid, NULL, 0)) == (MTP_Socket_Info *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    int shmid1;
    SOCK_INFO* sock_info_init;

    // Get the shared memory segment
    if ((shmid1 = shmget(ftok(".",'2'), sizeof(SOCK_INFO), 0777|IPC_CREAT)) == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Attach to the segment to get a pointer to it
    if ((sock_info_init = shmat(shmid1, NULL, 0)) == (SOCK_INFO *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
   
        // SM=  access_shared_memory();

        // MTP_Socket_Info* SM=  access_shared_memory();
        // sock_info=  access_sockinfo_memory();
        
        // Step 3(b): Look at SOCK_INFO
        if (sock_info_init->sock_id == 0 && sock_info_init->src_port==0  && sock_info_init->dst_port==0&& sock_info_init->error_number == 0&& sock_info_init->creator_pid==0) {
            // It's an m_socket call
            // Create a UDP socket

            printf("init_proc: request for m_socket process\n");
            int udp_sock_id = socket(AF_INET, SOCK_DGRAM, 0);
            int optval = 1;
            if (setsockopt(udp_sock_id, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
                perror("setsockopt");
                exit(EXIT_FAILURE);
            }
            if (udp_sock_id == -1) {
                // If error, put -1 in sock_id field and errno in errno field
                sock_info_init->sock_id = -1;
                sock_info_init->error_number = errno;
            } else {
                // Put the socket id returned in the sock_id field of SOCK_INFO
                sock_info_init->sock_id = udp_sock_id;
            }
            printf("new socket created with sockid=%d\n",sock_info_init->sock_id);

        } else {
            // It's an m_bind call
            printf("init_proc: request for m_bind process\n");
            // Make a bind() call on the sock_id value, with the IP and port given
            int sockfd = sock_info_init->sock_id;
            const char *src_ip = sock_info_init->src_ip;
            int src_port = sock_info_init->src_port; 

            const char *des_ip = sock_info_init->dst_ip;
            int des_port = sock_info_init->dst_port;
            int pid = sock_info_init->creator_pid;

            struct sockaddr_in src_addr;
            src_addr.sin_family = AF_INET;
            src_addr.sin_port = htons(src_port);
            src_addr.sin_addr.s_addr = inet_addr(src_ip);

            // printf("sockfd %d\n",sockfd);
            // Perform the bind call
            if (bind(sockfd, (struct sockaddr *)&src_addr, sizeof(src_addr)) <0) {
                printf("error in binding\n");
                perror("bind");

                sock_info_init->error_number = errno;
                sock_info_init->sock_id = -1;
            }
            else{
                for (int i = 0; i < NUM_MTP_SOCKETS; i++) {
                    if (SM_init[i].udp_socket_id == sockfd) {
                        strncpy(SM_init[i].other_end_IP, des_ip, MAX_IP_LENGTH);
                        SM_init[i].other_end_port = des_port;
                        SM_init[i].is_free = 0;
                        SM_init[i].creator_pid=pid;
                        printf("BIND values written in SM[]\n");
                        break;
                        // Return the index if found
                    }
            }

            // detach_sockinfo_memory(sock_info);

            // detach_shared_memory(SM);
            // printf("detached all in init\n");

            }
            
        }

        // Step 3(c): Signal on Sem2
        semaphore_signal(Sem2);
    }

    // Clean up: Detach shared memory
    if (shmdt(sock_info_init) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
    if (pthread_join(gc_thread, NULL) != 0) {
        perror("pthread_join");
        exit(EXIT_FAILURE);
    }
    // if (pthread_join(thread_S, NULL) != 0) {
    //     perror("pthread_join");
    //     exit(EXIT_FAILURE);
    // }
    // if (pthread_join(thread_R, NULL) != 0) {
    //     perror("pthread_join");
    //     exit(EXIT_FAILURE);
    // }
    //  pthread_join(thread_S, NULL)0;
    // pthread_join(thread_R, NULL);


    return 0;
}




