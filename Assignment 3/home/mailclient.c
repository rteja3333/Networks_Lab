#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_MSG_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_address> <server_port>\n", argv[0]);
        exit(1);
    }

    char *server_address = argv[1];
    int server_port = atoi(argv[2]);

    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[MAX_MSG_SIZE];

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creating socket");
        exit(1);
    }

    // Set server address and port
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_address, &serv_addr.sin_addr) <= 0) {
        perror("Invalid server address");
        exit(1);
    }

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        exit(1);
    }

    // Receive server greeting
    recv(sockfd, buffer, MAX_MSG_SIZE, 0);
    printf("%s\n", buffer); // Print server greeting

    // Send HELO command
    strcpy(buffer, "HELO example.com\r\n");
    send(sockfd, buffer, strlen(buffer), 0);
    recv(sockfd, buffer, MAX_MSG_SIZE, 0);
    printf("%s\n", buffer); // Print server reply to HELO command

    // Send MAIL FROM command
    strcpy(buffer, "MAIL FROM: <ag@iitkgp.edu>\r\n");
    send(sockfd, buffer, strlen(buffer), 0);
    recv(sockfd, buffer, MAX_MSG_SIZE, 0);
    printf("%s\n", buffer); // Print server reply to MAIL FROM command

    // Send RCPT TO command
    strcpy(buffer, "RCPT TO: <gb@iitkgp.edu>\r\n");
    send(sockfd, buffer, strlen(buffer), 0);
    recv(sockfd, buffer, MAX_MSG_SIZE, 0);
    printf("%s\n", buffer); // Print server reply to RCPT TO command

    // Send DATA command
    strcpy(buffer, "DATA\r\n");
    send(sockfd, buffer, strlen(buffer), 0);
    recv(sockfd, buffer, MAX_MSG_SIZE, 0);
    printf("%s\n", buffer); // Print server reply to DATA command

    // Send email content
    strcpy(buffer, "From: ag@iitkgp.edu\r\n");
    send(sockfd, buffer, strlen(buffer), 0);
    strcpy(buffer, "To: gb@iitkgp.edu\r\n");
    send(sockfd, buffer, strlen(buffer), 0);
    strcpy(buffer, "Subject: Test mail\r\n");
    send(sockfd, buffer, strlen(buffer), 0);
    strcpy(buffer, "This is a test mail.\r\n");
    send(sockfd, buffer, strlen(buffer), 0);
    strcpy(buffer, "How are you doing?\r\n");
    send(sockfd, buffer, strlen(buffer), 0);
    strcpy(buffer, ".\r\n");
    send(sockfd, buffer, strlen(buffer), 0);

    // Receive server reply to email content
    recv(sockfd, buffer, MAX_MSG_SIZE, 0);
    printf("%s\n", buffer);

    // Send QUIT command
    strcpy(buffer, "QUIT\r\n");
    send(sockfd, buffer, strlen(buffer), 0);
    recv(sockfd, buffer, MAX_MSG_SIZE, 0);
    printf("%s\n", buffer); // Print server reply to QUIT command

    // Close socket
    close(sockfd);

    return 0;
}
