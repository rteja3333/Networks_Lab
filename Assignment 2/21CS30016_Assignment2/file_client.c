
/*    THE CLIENT PROCESS */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>
#include <fcntl.h>

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



int main()
{
	int			sockfd ;
	struct sockaddr_in	serv_addr;

	int i;
	char buf[100];

	
    while (1) {
        /* Opening a socket is exactly similar to the server process */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Unable to create socket\n");
		exit(0);
	}

	serv_addr.sin_family	= AF_INET;
	inet_aton("127.0.0.1", &serv_addr.sin_addr);
	serv_addr.sin_port	= htons(20000);

	/* With the information specified in serv_addr, the connect()
	   system call establishes a connection with the server process.
	*/



    char filename[50];
    if ((connect(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr))) < 0) {
                perror("Unable to connect to server\n");
                exit(0);
            }
                    printf("--------------------------\n");


    printf("\nserver connected !!!\n\n");


        printf("Enter Filename : ");
        scanf("%s", filename);
        printf("\n");

        int found=searchFile(filename);

        if (found) {

            
            int k,a;  
            printf("Enter k: ");
            scanf("%d",&k);
            send(sockfd,&k,sizeof(k), 0);
            // printf("client send1: %d\n",k);
            // Open a local file for writing
int fd = open(filename, O_RDONLY);
if (fd == -1) {
    perror("Error opening file");
    exit(EXIT_FAILURE);
}

while (1) {
    ssize_t bytesRead = read(fd, buf, sizeof(buf));
    if (bytesRead == 0) {
        // printf("end of file\n");
        // End of file reached
        break;
    }
    send(sockfd, buf, bytesRead, 0);
    // printf("client send: %.*s\n", (int)bytesRead, buf);

    recv(sockfd, &a, sizeof(a), 0);
    // printf("ack received: %d\n", a);
    a = 0;
    memset(buf, 0, sizeof(buf));
}

// Signal the end of file
buf[0] = 1;
send(sockfd, buf, 1, 0);

// Close the local file
close(fd);

printf("Sent the file for encryption\n\n");

            int n=1;
            // n=;
            printf("Recieving encrypted file ...\n\n");

             char encfilename[50];
             strcpy(encfilename, ""); // Initialize with an empty string

             strcat(encfilename,filename);
             strcat(encfilename,".enc");


        int encfd = open(encfilename, O_CREAT | O_WRONLY | O_TRUNC, 0644);

     
while (n) {
    n = recv(sockfd, buf, sizeof(buf), 0);
    // No need to check for null byte in buf[0]
if(buf[0]==1)break;
    if (n > 0) {
        // printf("client rec: %.*s\n", n, buf);
        
        // Write the entire buffer to encfd
        int x = write(encfd, buf, n);

        // Acknowledge message
        int a = 1;
        send(sockfd, &a, sizeof(a), 0);
    }
}
            // printf("Encryption done\n\n");
            printf("Original file: %s   \nEncrypted file: %s\n",filename,encfilename);
             close(fd);

        }

        else{
            printf("File not found\n\n");
        }
        sleep(1);
        shutdown(sockfd, SHUT_RDWR);

        close(sockfd);
    }



	return 0;

}

