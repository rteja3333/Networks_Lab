#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "msocket.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8000
#define CLIENT_IP "127.0.0.1"
#define CLIENT_PORT 9000
#define FILE_NAME "file.txt"

#define MSG_SIZE 1024
int main() {
    int sockfd;
    FILE *fp;
    char buffer[MSG_SIZE];
    ssize_t bytes_read, bytes_sent;

    /* Create MTP socket */
    sockfd = m_socket(AF_INET, SOCK_MTP, 0);
    if (sockfd == -1) {
        perror("m_socket");
        exit(EXIT_FAILURE);
    }
   int sockfd1 = m_socket(AF_INET, SOCK_MTP, 0);
    /* Bind MTP socket */
    if (m_bind(sockfd, SERVER_IP, SERVER_PORT, CLIENT_IP, CLIENT_PORT) == -1) {
        perror("m_bind");
        exit(EXIT_FAILURE);
    }

    /* Open file for reading */
    fp = fopen(FILE_NAME, "r");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    /* Send file data */
    while ((bytes_read = fread(buffer, 1, MSG_SIZE, fp)) > 0) {
        bytes_sent = m_sendto(sockfd, buffer, bytes_read, 0, CLIENT_IP, CLIENT_PORT);
        if (bytes_sent == -1) {
            perror("m_sendto");
            break;
        }
    }

    fclose(fp);
    m_close(sockfd);

    return 0;
}