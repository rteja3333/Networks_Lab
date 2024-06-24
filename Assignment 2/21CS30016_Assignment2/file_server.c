
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ctype.h>

			/* THE SERVER PROCESS */

#include <stdio.h>
#include <stdlib.h>

char* encrypt(char s[], int k, int n) {
    char* enc = (char*)malloc((n+1) * sizeof(char)); // Allocate memory for the encrypted string

    if (enc == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n; i++) {
       if (isalpha(s[i])) {
            char base = isupper(s[i]) ? 'A' : 'a';
            enc[i] = ((s[i] - base + k) % 26) + base;  // Encrypt alphabetic characters
        } else {
            enc[i] = s[i];  // Leave non-alphabetic characters unchanged
        }
    }

    enc[n] = '\0';  // Null-terminate the encrypted string

    return enc;
}


int main()
{
	int			sockfd, newsockfd ; /* Socket descriptors */
	int			clilen;
	struct sockaddr_in	cli_addr, serv_addr;

	int i;
	char buf[100];		/* We will use this buffer for communication */

	
	while (1) {
if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Cannot create socket\n");
		exit(0);
	}

	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= INADDR_ANY;
	serv_addr.sin_port		= htons(20000);

	/* With the information provided in serv_addr, we associate the server
	   with its port using the bind() system call. 
	*/
int reuse = 1;
setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	if (bind(sockfd, (struct sockaddr *) &serv_addr,
					sizeof(serv_addr)) < 0) {
		perror("Unable to bind local address\n");
		exit(0);
	}
    listen(sockfd, 5); 
    printf("-------------------------\n\nserver running...\n\n");

		clilen = sizeof(cli_addr);
		
       

        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr,
					&clilen) ;

         char ip[INET_ADDRSTRLEN]; // INET_ADDRSTRLEN is typically 16

        // Convert the binary IP address to a string in dot-decimal notation
        if (inet_ntop(AF_INET, &(cli_addr.sin_addr), ip, INET_ADDRSTRLEN) == NULL) {
            perror("inet_ntop");
            return 1;
        }
        char portno[6]; // 5 characters for the maximum port number (65535) plus null terminator

        // Use sprintf to convert the integer port number to a string
        sprintf(portno, "%d", ntohs(cli_addr.sin_port));
        char filename[100];
        char encfilename[100];

        strcpy(filename, ip);
        strcat(filename, portno);
        strcat(filename, ".txt");

        strcpy(encfilename, filename);
        strcat(encfilename, ".enc");
       
        // printf("encfd:%d\n\n",encfd);
		if (newsockfd < 0) {
			perror("Accept error\n");
			exit(0);
		}
        printf("Accepted a client\n");

        int k=0;
        if (recv(newsockfd, &k, sizeof(k), 0) == -1) {
                perror("Receive failed");
                // Handle error
            }   

             int fd = open(filename, O_CREAT | O_WRONLY, 0644);
        int encfd = open(encfilename, O_CREAT | O_WRONLY, 0644);
     
        // printf("recieved k= %d\n\n",k);
        
        int n=1;
        
while (1) {
    n = recv(newsockfd, buf, sizeof(buf), 0);
    if (buf[0] == 1) break;

    // printf("server rec: %.*s\n", n, buf);

    // Write the received data to the file
    int x = write(fd, buf, n);

    // Encrypt the data
    char* enc = encrypt(buf, k, n);

    // Write the encrypted data to the file
    x = write(encfd, enc, n);

    // printf("written in file: %.*s\n", n, enc);

    // Clean up
    free(enc);

    // Acknowledge message
    int a = 1;
    send(newsockfd, &a, sizeof(a), 0);
}

        printf("file created : %s\n\n",filename);

        close(fd);
        close(encfd);
        //encryption done
         printf("encrypted file name : %s\n\n",encfilename);


        printf("*******  Encryption done  ***** \n\nSending encrypted file\n\n");
        int a;
            encfd=open(encfilename,O_RDONLY);
// lseek(encfd, 0, SEEK_SET); // Move the file pointer to the beginning
if (encfd == -1) {
    perror("Error opening file");
    exit(EXIT_FAILURE);
}

while (1) {
    // Read encrypted data from the file
    ssize_t bytesRead = read(encfd, buf, sizeof(buf));

    if (bytesRead <= 0) {
        // Either end of file or an error occurred
        break;
    }

    // Send encrypted data to the client
    send(newsockfd, buf, bytesRead, 0);
// printf("serv send: %.*s\n", (int)bytesRead, buf);

    // Receive acknowledgment from the client
    recv(newsockfd, &a, sizeof(a), 0);
    // printf("ack received: %d\n", a);

    // Reset buffer
    memset(buf, 0, sizeof(buf));
}

// Signal end of transmission
buf[0] = 1;
send(newsockfd, buf, sizeof(buf), 0);
// Close the encrypted file
close(encfd);
// printf("sent enc file");
sleep(1);
shutdown(newsockfd, SHUT_RDWR);
close(newsockfd);
	close(sockfd);
    }
	return 0;
}
			

