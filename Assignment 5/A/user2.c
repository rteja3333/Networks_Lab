#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "msocket.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9001
#define CLIENT_IP "127.0.0.1"
#define CLIENT_PORT 8001
#define FILE_NAME "received_file.txt"

#define MSG_SIZE 1024
int main() {
    int sockfd;
    FILE *fp;
    char buffer[MSG_SIZE];
    ssize_t bytes_received;
    char src_ip[16];
    int src_port;

    /* Create MTP socket */
    sockfd = m_socket(AF_INET, SOCK_MTP, 0);


    if (sockfd == -1) {
        perror("m_socket");
        exit(EXIT_FAILURE);
    }

    /* Bind MTP socket */
    if (m_bind(sockfd, SERVER_IP, SERVER_PORT, CLIENT_IP, CLIENT_PORT) == -1) {
        perror("m_bind");
        exit(EXIT_FAILURE);
    }

    /* Open file for writing */
    fp = fopen("recieve.txt", "w");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    sleep(5);
    /* Receive file data */
    while (1) {
        bytes_received = m_recvfrom(sockfd, buffer, MSG_SIZE, 0, src_ip, &src_port);
        if (bytes_received == -1) {
            // if (m_errno == ENOMSG) {
            //     /* No more messages, exit loop */
            //     break;
            // } else {
            //     perror("m_recvfrom");
            //     exit(EXIT_FAILURE);
            // }
            break;
        }
        fwrite(buffer, 1, bytes_received, fp); 
    }

    fclose(fp);
    m_close(sockfd);

    return 0;
}