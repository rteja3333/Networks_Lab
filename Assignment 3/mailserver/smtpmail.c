#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#define MAX_MSG_SIZE 5000

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int userExists(const char *receiver) {
    // Concatenate receiver string with current directory path
    char path[1024];
        char foldername[MAX_MSG_SIZE];

    sscanf(receiver, "%[^@]", foldername);

    snprintf(path, sizeof(path), "./%s", foldername);

    // Check if folder exists
    FILE *folder = fopen(path, "r");
    if (folder) {
        fclose(folder);
        return 1; // Folder exists
    } else {
        return 0; // Folder does not exist
    }
}

void saveMessage(const char *sender, const char *receiver, const char *message) {
    time_t rawtime;
    struct tm *timeinfo;
    char filename[MAX_MSG_SIZE];
    FILE *file;
    // printf("mess:%s\n",message);
    // time(&rawtime);
    // timeinfo = localtime(&rawtime);
    strcpy(filename, "mymailbox.txt");

    // strftime(filename, sizeof(filename), "%Y%m%d%H%M%S", timeinfo);
    // strcat(filename, "_");
    // strcat(filename, sender);
    // strcat(filename, ".txt");

    // Create file in recipient's folder and save the message
    char foldername[MAX_MSG_SIZE];
    sscanf(receiver, "%[^@]", foldername);
    // printf("folder name:%s\n",foldername);
    // snprintf(foldername, sizeof(foldername), "./%s", receiver);
    mkdir(foldername, 0777); // Create folder if it doesn't exist
    chdir(foldername); // Change directory to recipient's folder
    file = fopen(filename, "a");
    if (file == NULL) {
        error("ERROR creating file");
    }   
    fprintf(file, "%s\n", message);
    fclose(file);
    chdir(".."); // Change directory back to the parent directory
    printf("message saved successfully in %s folder\n",foldername);
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port_number>\n", argv[0]);
        exit(1);
    }

    int sockfd, newsockfd, portno, n;
    char buffer[MAX_MSG_SIZE];
    char sender[MAX_MSG_SIZE]; // Store sender's email address
    char reciever[MAX_MSG_SIZE]; // Store sender's email address
    char message[MAX_MSG_SIZE];


    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    portno = atoi(argv[1]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR on binding");
    }

    listen(sockfd, 5);


    while (1) {

        clilen = sizeof(cli_addr);

        printf("Waiting for incoming connections...\n\n");

        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) {
            error("ERROR on accept");
        }

        printf("Connection accepted. Waiting for commands...\n");
        // Send 220 Service Ready greeting upon connection
        
        if(fork()==0){
            close(sockfd);
           strcpy(buffer, "220 <iitkgp.edu> Service Ready\r\n");
          send(newsockfd, buffer, strlen(buffer), 0);
        while(1){
        bzero(buffer, MAX_MSG_SIZE);
        n = recv(newsockfd, buffer, MAX_MSG_SIZE - 1, 0);
        if (n < 0) {
            error("ERROR reading from socket");
        }
        if (n == 0) {
            break; // Connection closed by client
        }
        printf("Received command: %s\n", buffer);

        // // Process the received command and generate appropriate replies
        // printf("Received command: %s\n", buffer);

        // Example: Reply with 250 OK after receiving EHLO command
        if (strncmp(buffer, "HELO", 4) == 0) {
            // printf("helo\n\n");
            memset(buffer,0,sizeof(buffer));
            // Send 250 OK response with server's domain name and EHLO greet
            strcpy(buffer, "250 OK Hello iitkgp.edu\r\n");
            send(newsockfd, buffer, strlen(buffer), 0);
        }
        else if (strncmp(buffer, "MAIL FROM:", 10) == 0) {
            // Extract sender's email address
            sscanf(buffer, "MAIL FROM: <%[^>]", sender);
            // Send 250 OK response with sender's email address
                        memset(buffer,0,sizeof(buffer));

            sprintf(buffer, "250 <%s>.. Sender ok\r\n", sender);
            send(newsockfd, buffer, strlen(buffer), 0);
        }  
        else if (strncmp(buffer, "RCPT TO:", 8) == 0) {
            // printf("rcpt\n");;
            // Extract sender's email address
            sscanf(buffer, "RCPT TO: <%[^>]", reciever);
            int usercheck=userExists(reciever);

            if(usercheck){
                printf("given reciever exist\n\n");

            // Send 250 OK response with sender's email address
                memset(buffer,0,sizeof(buffer));
                sprintf(buffer, "250 root.. Recipient ok\r\n" );
                send(newsockfd, buffer, strlen(buffer), 0);
            }
            else{
                memset(buffer,0,sizeof(buffer));
                sprintf(buffer, "550 No such user\r\n" );
                send(newsockfd, buffer, strlen(buffer), 0);

            }
        }
        else if (strncmp(buffer, "DATA", 4) == 0) {
            memset(buffer, 0, sizeof(buffer));
            sprintf(buffer, "354 Enter mail,end with \".\" on a line by itself");
            send(newsockfd, buffer, strlen(buffer), 0);
            int buffer_len;
            do {
                int bytes_received;
                bzero(buffer, MAX_MSG_SIZE);
                bytes_received = recv(newsockfd, buffer, MAX_MSG_SIZE - 1, 0);
                if (bytes_received < 0) {
                    error("ERROR reading from socket");
                }
                strcat(message, buffer);
                strcat(buffer, "\0");

                // Check if subject line is received
                if (strstr(message, "\r\nSubject:") != NULL) {
                    // Get current time
                    time_t now = time(NULL);
                    struct tm *tm_info = localtime(&now);
                    char received_time[50];
                    strftime(received_time, sizeof(received_time), "Received: %Y-%m-%d %H:%M\r\n", tm_info);

                    // Find the position to insert the received time
                    char *subject_pos = strstr(message, "\r\nSubject:");
                    if (subject_pos != NULL) {
                        // Calculate position to insert received time
                        char *insert_position = strchr(subject_pos, '\n'); // Move to the next line after Subject
                        if (insert_position != NULL) {
                            insert_position++; // Move past the newline character
                            memmove(insert_position + strlen(received_time), insert_position, strlen(insert_position) + 1);
                            memcpy(insert_position, received_time, strlen(received_time));
                        }
                    }
                }

                // Check if the last five characters in the buffer are "\r\n.\r\n"
                buffer_len = strlen(buffer);
                if (buffer_len < 5) {
                    continue;
                }
            } while ((strncmp(buffer + buffer_len - 5, "\r\n.\r\n", 5) != 0)); // Keep receiving until "." is received

            // Send acknowledgment
            saveMessage(sender, reciever, message);

            memset(buffer, 0, sizeof(buffer));
            sprintf(buffer, "250 OK Message accepted for delivery");
            send(newsockfd, buffer, strlen(buffer), 0);
        }


        

      
        // Example: Reply 221 Service closing transmission channel upon receiving QUIT command
        else if (strncmp(buffer, "QUIT", 4) == 0) {
            // Send 221 Service closing transmission channel
            strcpy(buffer, "221 <iitkgp.edu> Service closing transmission channel\r\n");
            send(newsockfd, buffer, strlen(buffer), 0);
            break; // Terminate connection
        }
        else{
            printf("nothing..\n");
        }
        }
       close(newsockfd);
        printf("Connection closed.\n");
        exit(0);


    }
       close(newsockfd);

    }


    
    return 0;
}
