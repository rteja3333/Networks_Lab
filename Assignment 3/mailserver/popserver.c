#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <stdbool.h>

#define MAX_MSG_SIZE 5000
#define MAX_USERNAME_LEN 100
#define MAX_PASSWORD_LEN 100
#define MAX_LINE_LEN 100
#define MAX_EMAILS 100
char username[MAX_USERNAME_LEN]="";
int no_of_mails;

int marked_emails[MAX_EMAILS] = {0}; // Array to track marked emails

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void handle_stat(int newsockfd, FILE* mymails) {
    // Implement handling of STAT command
    close(mymails);
    char mailbox_filename[200];
    snprintf(mailbox_filename, sizeof(mailbox_filename), "./%s/mymailbox.txt", username);
    mymails = fopen(mailbox_filename, "r");
    
    fseek(mymails, 0, SEEK_SET);
    int count = 0;
    int total_size = 0;
    char line[MAX_MSG_SIZE];
    while (fgets(line, MAX_MSG_SIZE, mymails) != NULL) {
        if (strcmp(line, ".\r\n") == 0) {
            total_size += strlen(line); // Accumulate message size
            count++; // Increment message count
        } else {
            total_size += strlen(line); // Accumulate message size
        }
    }
    char response[MAX_MSG_SIZE]="";
    // strcat(response,"+OK 1 100000");
    snprintf(response, sizeof(response), "+OK %d %d\r\n", count, total_size);
    no_of_mails=count;
    send(newsockfd, response, strlen(response), 0);
    // printf("stat semt: %s\n",response);
    // printf("\t\t stat over\n");
}

void handle_list(int newsockfd, FILE* mymails, int msg) {
    // Implement handling of LIST command

    close(mymails);
    char mailbox_filename[200];
    snprintf(mailbox_filename, sizeof(mailbox_filename), "./%s/mymailbox.txt", username);
    mymails = fopen(mailbox_filename, "r");
    
    char response[MAX_MSG_SIZE];
    fseek(mymails, 0, SEEK_SET);
    int count = 0;
    int total_size = 0;

    if (msg == -1) {
        // Count the total number of messages and their total size
        while (fgets(response, MAX_MSG_SIZE, mymails) != NULL) {
            if (marked_emails[count] == 0) {
                int mail_size = strlen(response); // Initialize mail size
                while (fgets(response, MAX_MSG_SIZE, mymails) != NULL) {
                    mail_size += strlen(response); // Add to mail size
                    if (strcmp(response, ".\r\n") == 0) { // Check for end of mail
                        break;
                    }
                }
                total_size += mail_size;
                count++;
                
            }
        }

        // Send the total number of messages and their total size
        snprintf(response, sizeof(response), "+OK (%d)messages (%d)size\r\n", count, total_size);
        send(newsockfd, response, strlen(response), 0);

        // Send each message number with its corresponding size
        fseek(mymails, 0, SEEK_SET);
        count = 0;
        while (fgets(response, MAX_MSG_SIZE, mymails) != NULL) {
            if (marked_emails[count] == 0) {
                int mail_size = strlen(response); // Initialize mail size
                while (fgets(response, MAX_MSG_SIZE, mymails) != NULL) {
                    mail_size += strlen(response); // Add to mail size
                    if (strcmp(response, ".\r\n") == 0) { // Check for end of mail
                        break;
                    }
                }
                snprintf(response, sizeof(response), "+OK %d %d\r\n", count+1, mail_size);
                send(newsockfd, response, strlen(response), 0);
            }
            count++;
        }
    } else {
        // Check if the message number is within range and not marked as deleted
        if (msg > 0 && msg <= count && marked_emails[msg-1] == 0) {
            // Find and send the size of the specified message
            count = 0;
            while (fgets(response, MAX_MSG_SIZE, mymails) != NULL) {
                if (marked_emails[count] == 0) {
                    if (count == msg - 1) { // Email numbering starts from 1
                        int mail_size = strlen(response); // Initialize mail size
                        while (fgets(response, MAX_MSG_SIZE, mymails) != NULL) {
                            mail_size += strlen(response); // Add to mail size
                            if (strcmp(response, ".\r\n") == 0) { // Check for end of mail
                                break;
                            }
                        }
                        snprintf(response, sizeof(response), "+OK %d %d\r\n", msg, mail_size);
                        send(newsockfd, response, strlen(response), 0);
                        break;
                    }
                }
                count++;
            }
        } else {
            snprintf(response, sizeof(response), "-ERR no such message\r\n");
            send(newsockfd, response, strlen(response), 0);

        }
    }
    
    send(newsockfd, ".\r\n", 3, 0); // Send the termination message
    fclose(mymails); // Close the mailbox file
    // printf("list done\n");
}



void handle_retr(int newsockfd, FILE* mymails, int email_number) {
    // Implement handling of RETR command
    close(mymails);
    char mailbox_filename[200];
    snprintf(mailbox_filename, sizeof(mailbox_filename), "./%s/mymailbox.txt", username);
    mymails = fopen(mailbox_filename, "r");
    
    char response[MAX_MSG_SIZE];
    fseek(mymails, 0, SEEK_SET);
    int count = 0;
    int mail_size = 0;
    bool found_email = false;
    bool email_deleted = false;
    char mail_content[MAX_MSG_SIZE]="";
    if ((marked_emails[email_number-1] == 1)) {
        send(newsockfd, "-ERR Message marked as deleted\r\n", strlen("-ERR Message marked as deleted\r\n"), 0);
        // printf("-ERR (trying to retrieve deleted msg)\n");
        return;
    }
    while (fgets(response, MAX_MSG_SIZE, mymails) != NULL) {
        if (strcmp(response, ".\r\n") == 0) {
             // Check for end of email marker
            count++;
            if (count == email_number) {
                // End of the requested email, send it to the client
                strcat(mail_content, response);
                mail_size += strlen(response);
                snprintf(response, sizeof(response), "+OK %d octets\r\n", mail_size);
                send(newsockfd, response, strlen(response), 0);
                send(newsockfd, mail_content, strlen(mail_content), 0);
                break;
            }
        }
         else {
            
            if (count == email_number-1) {
                found_email = true;
                // Copy the mail content to a separate buffer
                strcat(mail_content, response);
                mail_size += strlen(response);

            }
            }
            
    }
    
}

void handle_dele(int newsockfd, int email_number) {
    // Implement handling of DELE command
    if (email_number < 1 || email_number > no_of_mails) {
        char response[] = "-ERR no such message\r\n";
        send(newsockfd, response, strlen(response), 0);
    } else if (marked_emails[email_number - 1] == 1) {
        char response[] = "-ERR message already deleted\r\n";
        send(newsockfd, response, strlen(response), 0);
    } else {
        marked_emails[email_number - 1] = 1; // Mark email as deleted
        char response[] = "+OK message deleted\r\n";
        send(newsockfd, response, strlen(response), 0);
    }
}


void handle_rset(int newsockfd) {
    // Implement handling of RSET command
    memset(marked_emails, 0, sizeof(marked_emails)); // Clear marked emails array
    char response[] = "+OK reset done\r\n";
    send(newsockfd, response, strlen(response), 0);
}


void delete_marked_mails() {
    FILE *mailsfile;
    char line[MAX_MSG_SIZE];
    char temp_filename[L_tmpnam];
    FILE *temp_file;
    int count =0;
    // Open the original mailbox file for reading
    char mailbox_filename[200];
    snprintf(mailbox_filename, sizeof(mailbox_filename), "./%s/mymailbox.txt", username);
    mailsfile = fopen(mailbox_filename, "r");
    if (mailsfile == NULL) {
        perror("fopen");
        return;
    }

    // Create a temporary file
    if (tmpnam(temp_filename) == NULL) {
        perror("tmpnam");
        fclose(mailsfile);
        return;
    }

    // Open the temporary file for writing
    temp_file = fopen(temp_filename, "w+");
    if (temp_file == NULL) {
        perror("fopen");
        fclose(mailsfile);
        return;
    }

    // Read mails from the original file and write to temporary file
    while (fgets(line, MAX_MSG_SIZE, mailsfile) != NULL) {
        // printf("line reading from original:%s\n",line);
        if (strcmp(line, ".\r\n") == 0) {
            if (!marked_emails[count]) {
                fputs(line, temp_file);
                // printf("line writing into temp:%s\n",line);

            }
            count++;
        } else {
            if (!marked_emails[count]) {
                fputs(line, temp_file);
                // printf("line writing into temp:%s\n",line);

            }
        }
    }

    // Close original mailbox file
    fclose(mailsfile);

    // Close temporary file
    fclose(temp_file);

    // Reopen original mailbox file in write mode (truncate existing content)
    mailsfile = fopen(mailbox_filename, "w");
    if (mailsfile == NULL) {
        perror("fopen");
        return;
    }

    // Open temporary file for reading
    temp_file = fopen(temp_filename, "r");
    if (temp_file == NULL) {
        perror("fopen");
        return;
    }

    // Copy contents from temporary file to original mailbox file
    while (fgets(line, MAX_MSG_SIZE, temp_file) != NULL) {
                        // printf("line writing into originsk:%s\n",line);

        fputs(line, mailsfile);
        // sleep(1);
    }

    // Close temporary file
    fclose(temp_file);
    fclose(mailsfile);
    printf("Deleted the marked mails from the mailbox.\n");
}


// Function to handle AUTHORIZATION state
FILE* handleAuthorization(FILE *userFile, int clientSocket) {
    char password[MAX_PASSWORD_LEN];
    char buffer[MAX_MSG_SIZE];
    char line[MAX_LINE_LEN];
    char *token;
    // Read username and password from user.txt
    // if (fscanf(userFile, "%s %s", username, password) != 2) {
    //     fprintf(stderr, "Error reading username and password from file\n");
    //     return NULL;
    // }

    // Loop to receive commands from the client
    while (1) {
        // Receive command from the client
        if (recv(clientSocket, buffer, sizeof(buffer), 0) < 0) {
            fprintf(stderr, "Error receiving command from client\n");
            break;
        }

        // Check if the received command is USER
        if (strncmp(buffer, "USER", 4) == 0) {
            // Extract username from the received command
            char *providedUsername = strtok(buffer + 5, "\r\n");

            strcpy(username,providedUsername);
            while (fgets(line, sizeof(line), userFile)) {
                // Tokenize the line to extract username and password
                token = strtok(line, " ");
                if (token != NULL) {
                    // Compare the username
                    if (strcmp(token, providedUsername) == 0) {
                         memset(buffer,0,sizeof(buffer));
                        sprintf(buffer, "+OK Username accepted\r\n" );
                        send(clientSocket, buffer, strlen(buffer), 0);
                        printf("username correct\n");
                
                        // Username found, store the corresponding password
                        token = strtok(NULL, " "); // Get next token (password)
                        if (token != NULL) {
                            strncpy(password, token, sizeof(password));
                            password[sizeof(password) - 1] = '\0'; // Ensure null-terminated
                            // printf("Password for user %s is: %s\n", username, password);
                            fclose(userFile);
                            break;
                        }
                    }
                }
                else{
                    send(clientSocket, "-ERR Invalid username\r\n", strlen("-ERR Invalid username\r\n"), 0);
                    printf("Invalid Username\n");
                    close(clientSocket);
                    return NULL;
            
                }
            }
        }
        // Check if the received command is PASS
        else if (strncmp(buffer, "PASS", 4) == 0) {
            // Extract password from the received command
            char *providedPassword = strtok(buffer + 5, "\r\n");

            // Compare provided password with the password read from file
            if (strcmp(password, providedPassword) == 0) {
                // Send success response
                 memset(buffer,0,sizeof(buffer));
                sprintf(buffer, "+OK Paasword accepted\r\n" );
                send(clientSocket, buffer, strlen(buffer), 0);
                printf("password correct\n");
            
                // Close user.txt file
                // fclose(userFile);

                // Construct mailbox filename
                char mailboxFilename[256];
                snprintf(mailboxFilename, sizeof(mailboxFilename), "./%s/mymailbox.txt", username);

                // Open mailbox file for append
                FILE *mailboxFile = fopen(mailboxFilename, "r");
                if (mailboxFile == NULL) {
                    perror("Error opening mailbox file");
                    return NULL;
                }

                // Return mailbox file pointer
                return mailboxFile;
            } else {
                // Send error response and close connection
                send(clientSocket, "-ERR Invalid password\r\n", strlen("-ERR Invalid password\r\n"), 0);
                close(clientSocket);
                printf("Invalid password\n");
                printf("Connection closed.\n");
                exit(0);
                printf("\n\nAuthentication failed !!!");

                return NULL;
            }
        }
        // Handle other commands as needed

        memset(buffer, 0, sizeof(buffer)); // Clear buffer for next command
    }
    printf("\n\nAuthentication successful !!!");
    fclose(userFile); // Close the user.txt file
    return NULL;
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

        printf("POP3 Waiting for incoming connections...\n\n");

        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) {
            error("ERROR on accept");
        }
        printf("\n--------------------------\nPOP3 Connection accepted. Waiting for commands...\n");

        memset(buffer,0,sizeof(buffer));
        sprintf(buffer, "+OK POP3 Server ready\r\n" );
        send(newsockfd, buffer, strlen(buffer), 0);
            
        // send(newsockfd, "+OK POP3 Server ready\r\n", strlen("+OK POP3 Server ready\r\n"), 0);
        // printf("sent pop3 ready\n");
        // Send 220 Service Ready greeting upon connection
        
        if(fork()==0){
            close(sockfd);
            FILE *userFile = fopen("user.txt", "r");
            if (userFile == NULL) {
                perror("Error opening user.txt");
                return 1;
            }

           FILE* mymails= handleAuthorization(userFile, newsockfd);

            while(1){
                bzero(buffer, MAX_MSG_SIZE);
                n = recv(newsockfd, buffer, MAX_MSG_SIZE - 1, 0);
                if (n < 0) {
                    error("ERROR reading from socket");
                }
                if(n>0){
                // if (n == 0) {
                //     break; // Connection closed by client
                // }
                // printf("Received command: %s\n",buffer);
                printf("-------\nReceived command: %s\n",buffer);
                if (strncmp(buffer, "STAT", 4) == 0) {
                    handle_stat(newsockfd,mymails);
                    printf("handled stat successful\n");
                }
                else if (strncmp(buffer, "RETR", 4) == 0) {
                    int email_number;
                    // Parse the integer value from the buffer
                    if (sscanf(buffer, "RETR %d", &email_number) == 1) {
                        // printf("Extracted email number: %d\n", email_number);
                    } 
                        handle_retr(newsockfd,mymails,email_number);
                        printf("handled RETR successful\n");
                
                }
                else if (strncmp(buffer, "LIST", 4) == 0) {
                    int msg = -1; // Initialize msg to -1
                    char *ptr = buffer + 4; // Move pointer to the position after "LIST"
                    while (*ptr != '\0') {
                        if (isdigit(*ptr)) { // Check if the character is a digit
                            msg = strtol(ptr, &ptr, 10); // Convert the digit sequence to an integer
                            break;
                        }
                        ptr++;
                    }
                    handle_list(newsockfd, mymails, msg);
                    printf("handled list successful\n");
                }

                else if (strncmp(buffer, "DELE", 4) == 0) {
                    int email_number;
                    // Parse the integer value from the buffer
                    if (sscanf(buffer, "DELE %d", &email_number) == 1) {
                        // printf("Extracted email number: %d\n", email_number);
                    } 
                        handle_dele(newsockfd,email_number);
                        printf("handled DELE successful\n");
                
                }
                else if (strncmp(buffer, "RSET", 4) == 0) {
                    handle_rset(newsockfd);
                    printf("handled RESET successful\n");
                }
                
                else if (strncmp(buffer, "QUIT", 4) == 0) {
                    delete_marked_mails();
                    printf("handled QUIT successful\n");
                }
                else{
                    printf("NO SUCH COMMAND : %s\n",buffer);
                }




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
