// A Simple UDP Server that sends a HELLO message
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>

#define MAXLINE 1024

// Function to search for a file in the current directory
int searchFile(const char *filename) {
    DIR *dir;
    struct dirent *entry;
    dir = opendir("./");
    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, filename) == 0) {
            printf("File found\n------------------\n");
            return 1; // File found
        }
    }

    printf("File %s not found\n------------------\n", filename);
    return 0; // File not found
}

int main() {
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    int n;
    socklen_t len;
    char buffer[MAXLINE];

    // Create socket file descriptor
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(8181);
    printf("\nServer Running....\n\n");

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *)&servaddr,
             sizeof(servaddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        len = sizeof(cliaddr);
        n = recvfrom(sockfd, (char *)buffer, MAXLINE, 0,
                     (struct sockaddr *)&cliaddr, &len);
        buffer[n] = '\0';
        char *filename = buffer;
        printf("file name received : %s\n\n", buffer);

        // Search for the file in the current directory
        int found = searchFile(filename);

        if (found == 0) {
            // File not found, send an error message to the client
            char message[100];
            sprintf(message, "File %s not found", filename);
            int messagelen = strlen(message);
            if (sendto(sockfd, message, messagelen, 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) != messagelen) {
                printf("send() sent a different number of bytes than expected");
                exit(0);
            }
        } else {
            // File found, open the file for reading
            FILE *file = fopen(filename, "r");

            // Check if the file was opened successfully
            if (file == NULL) {
                perror("Error opening the file");
                return 1; // Return an error code
            }

            // Read the first word from the file
            char message[100];
            int x = fscanf(file, "%s", message);
            int messagelen = strlen(message);

            // Send the first word to the client
            if (sendto(sockfd, message, messagelen, 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) != messagelen) {
                perror("sendto() error");
                fclose(file);
                close(sockfd);
                return 1; // Return an error code
            }

            printf("server sent     : %s\n------------------\n", message);

            // Read and send the remaining words from the file
            while (fscanf(file, "%s", message) == 1) {
                int messagelen = strlen(message);

                // Receive acknowledgment from the client
                n = recvfrom(sockfd, (char *)buffer, MAXLINE, 0, (struct sockaddr *)&cliaddr, &len);
                buffer[n] = '\0';
                printf("server received : %s\n\n", buffer);

                // Send the word over the socket
                if (sendto(sockfd, message, messagelen, 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) != messagelen) {
                    perror("sendto() error");
                    fclose(file);
                    close(sockfd);
                    return 1; // Return an error code
                }

                printf("server sent     : %s\n------------------\n", message);
            }

            fclose(file);
        }
    }

    return 0;
}
