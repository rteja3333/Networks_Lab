// A Simple Client Implementation
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAXLINE 1024

int main() {
    int sockfd, err;
    struct sockaddr_in servaddr;
    int n;
    socklen_t len;
    char *hello = "CLIENT:HELLO";
    char buffer[MAXLINE];

    // Creating socket file descriptor
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8181);
    err = inet_aton("127.0.0.1", &servaddr.sin_addr);
    if (err == 0) {
        printf("Error in ip-conversion\n");
        exit(EXIT_FAILURE);
    }

    char filename[50];
    printf("\nserver connected !!!\n\n");

    while (1) {
        printf("--------------------------\n");
        printf("Enter Filename : ");
        scanf("%s", filename);
        printf("\n");

        // Send filename to the server
        sendto(sockfd, (const char *)filename, strlen(filename), 0,
               (const struct sockaddr *)&servaddr, sizeof(servaddr));

        // Receive response from the server
        n = recvfrom(sockfd, (char *)buffer, MAXLINE, 0,
                     (struct sockaddr *)&servaddr, &len);
        buffer[n] = '\0';

        if (strcmp(buffer, "HELLO") == 0) {
            printf("client received : %s\n--------------------------\n", buffer);

            // Open a local file for writing
            FILE *file = fopen("./localfile.txt", "w");
            int word_count = 0;

            do {
                char message[50];
                scanf("%s", message);
                printf("\n");

                // Send user input to the server
                sendto(sockfd, (const char *)message, strlen(message), 0,
                       (const struct sockaddr *)&servaddr, sizeof(servaddr));
                printf("client sent     : %s\n\n", message);

                // Receive response from the server
                n = recvfrom(sockfd, (char *)buffer, MAXLINE, 0,
                             (struct sockaddr *)&servaddr, &len);
                buffer[n] = '\0';

                if (strcmp(buffer, "END") != 0) {

                    // Write received data to the local file
                    if (fputs(buffer, file) == EOF) {
                        perror("Error writing to the file");
                        fclose(file);
                        return 1; // Return an error code
                    }

                    fflush(file);

                    int x = fputs("\n", file);
                }
                printf("client received : %s\n--------------------------\n", buffer);
            } while (strcmp(buffer, "END") != 0);

            // Close the local file
            fclose(file);
        } else {
            printf("client received : %s \n", buffer);
        }
    }

    return 0;
}
