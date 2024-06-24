#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/select.h>
#include <netpacket/packet.h> // For sockaddr_ll
#include <netinet/if_ether.h> // For ETH_P_ALL
#include <net/if.h> // For ifreq
#include <sys/ioctl.h> // For ioctl
#include <pthread.h>

#define BUF_SIZE 1024
#define MAX_DOMAIN_SIZE 31
#define SIMDNS_PROTOCOL 254
#define RETRY_LIMIT 3
#define TIMEOUT_SEC 5
#define MAX_PENDING_QUERIES 10


float drop_probability = 0;



struct PendingQuery {
    int query_id;
    char query_string[BUF_SIZE];
    //latest time when the query was sent
    time_t last_query_time;

    int num_retries; // Number of times the query has been retried
};


// Structure representing SimDNS Query packet
struct SimDNSQueryPacket {
    uint16_t id;            // ID: 16 bits
    uint8_t message_type;   // Message Type: 1 bit
    uint8_t num_queries : 3; // Number of queries: 3 bits
    struct Query {
        uint32_t query_size : 4; // Size of query string: 4 bits
        char domain_name[32];    // Domain name for the query (max size: 32 bytes)
    } queries[8];               // Array of queries (maximum 8)
};

//structure representing the simDNS response packet
struct SimDNSResponsePacket {
    uint16_t id;            // ID: 16 bits
    uint8_t message_type;   // Message Type: 1 bit
    uint8_t num_queries : 3; // Number of queries: 3 bits
    struct Query_1 {
        //flag bit to indicate valid ip address
        uint8_t valid_ip : 1;   // Valid IP flag: 1 bit
        //ip address of the domain name
        char ip_address[32];    // Domain name for the query (max size: 32 bytes)
    } queries[8];               // Array of queries (maximum 8)
};



//FUNCTION TO DROP THE PACKET
int drop_packet(float drop_probability){
    float random_number = (float)rand() / RAND_MAX;
    if (random_number < drop_probability) {
        return 1;
    }
    return 0;
}




struct PendingQuery pending_queries[MAX_PENDING_QUERIES];
int num_pending_queries = 0;
int query_id_counter = 0;


// Function to add a pending query to the table
int add_pending_query(int query_id, const char *query_string) {
    if (num_pending_queries < MAX_PENDING_QUERIES) {
        pending_queries[num_pending_queries].query_id = query_id;
        strcpy(pending_queries[num_pending_queries].query_string, query_string);
        pending_queries[num_pending_queries].num_retries = 0;
        pending_queries[num_pending_queries].last_query_time = time(NULL); // Set the last query time to current time
        num_pending_queries++;
        return 1;
    } else {
        printf("Pending query table is full!\n");
        return 0;

    }
}

// Function to remove a pending query from the table
void remove_pending_query(int query_id) {
    int i;
    for (i = 0; i < num_pending_queries; i++) {
        if (pending_queries[i].query_id == query_id) {
            break;
        }
    }

    if (i < num_pending_queries) {
        for (int j = i; j < num_pending_queries - 1; j++) {
            pending_queries[j] = pending_queries[j + 1];
        }
        num_pending_queries--;
    }
}

// Function to get the index of a pending query in the table
int find_pending_query(int query_id) {
    for (int i = 0; i < num_pending_queries; i++) {
        if (pending_queries[i].query_id == query_id) {
            return i;
        }
    }
    return -1;
}





// Function to validate domain name format
#include <ctype.h>

int validate_domain(const char *domain) {
    int length = strlen(domain);
    
    // Check length
    if (length < 3 || length > 31) {
        return 0; // Invalid length
    }
    
    // Check for invalid characters
    for (int i = 0; i < length; i++) {
        if (!isalnum(domain[i]) && domain[i] != '-' && domain[i] != '.') {
            return 0; // Invalid character
        }
    }
    
    // Check for consecutive hyphens or hyphen at beginning or end
    for (int i = 0; i < length - 1; i++) {
        if (domain[i] == '-' && domain[i+1] == '-') {
            return 0; // Consecutive hyphens
        }
    }
    
    // Check if hyphen is at beginning or end
    if (domain[0] == '-' || domain[length - 1] == '-') {
        return 0; // Hyphen at beginning or end
    }
    
    // Check for spaces and special characters (except dot)
    for (int i = 0; i < length; i++) {
        if (isspace(domain[i]) || (!isalnum(domain[i]) && domain[i] != '-' && domain[i] != '.')) {
            return 0; // Invalid character
        }
    }

    return 1; // Valid domain name
}

// Function to send simDNS query
#include <stdint.h>

// Define constants for message type and maximum number of queries
#define QUERY_MESSAGE_TYPE 0
#define MAX_QUERIES 8

int construct_query_packet(const char *query_str, struct SimDNSQueryPacket *packet) {
    // Initialize packet fields
    packet->id = query_id_counter++; // Assign a unique ID to the query
    packet->message_type = 0; // Query message type
    packet->num_queries = 0; // Initialize number of queries

    // Parse the query string
    char *token;
    char *str = strdup(query_str); // Make a copy of the query string
    token = strtok(str, " "); // Tokenize by space

    // Skip the first token ("getIP")
    token = strtok(NULL, " ");
    //Skip the second token (number of queries)
    token = strtok(NULL, " ");

    // Extract and add domain names to the packet
    while (token != NULL) {
        // Copy the domain name into the packet
        strncpy(packet->queries[packet->num_queries].domain_name, token, sizeof(packet->queries[packet->num_queries].domain_name) - 1);
        packet->queries[packet->num_queries].domain_name[sizeof(packet->queries[packet->num_queries].domain_name) - 1] = '\0'; // Ensure null-termination

        // Calculate and set the query size
        packet->queries[packet->num_queries].query_size = strlen(packet->queries[packet->num_queries].domain_name);

        // Increment the number of queries
        packet->num_queries++;

        // Get the next token
        token = strtok(NULL, " ");
    }
    printf("no of queries sent:%d\n",packet->num_queries);

    //add the query to the pending query table if it is not already present in the table
    if(find_pending_query(packet->id) == -1){
    if (add_pending_query(packet->id, query_str)) {
        // printf("Query added to pending queries.\n");
        return 1;
    }
    else{
        return 0;
    }
    }
    else{
        // printf("Query already present in pending queries.\n");
        return 1;
    }


    // Free the memory allocated for the copy of the query string
    free(str);
}


int send_query(int client_socket, const char *query_string) {
    // Construct the SimDNSquery packet
    struct SimDNSQueryPacket query_packet;
    if(!construct_query_packet(query_string, &query_packet)){
        return 0;
        }


    // Initialize server address structure
    struct sockaddr_ll server_addr;
    struct ifreq ifr;
    char interface_name[] = "wlo1"; // Change this to your desired interface name

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    if (ioctl(client_socket, SIOCGIFINDEX, &ifr) < 0) {
        perror("Interface name to index conversion failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Fill server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sll_family = AF_PACKET;
    server_addr.sll_protocol = htons(ETH_P_ALL);
    server_addr.sll_ifindex = ifr.ifr_ifindex; // Interface index

    // Construct the IP header
    char datagram[4096]; // Adjust size as needed for the datagram
    struct iphdr *iph = (struct iphdr *)(datagram);
    memset(iph, 0, sizeof(struct iphdr));
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct SimDNSQueryPacket));
    iph->id = htons(54321);
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = SIMDNS_PROTOCOL;
    // Get the current device IP address using some system call or use a predefined IP
    iph->saddr = inet_addr("192.168.1.100");  // Your client's IP address
    iph->daddr = inet_addr("127.0.0.1");      // Server's IP address
    iph->check = 0; // Set to 0 before calculating checksum
    // iph->check = checksum(iph, sizeof(struct iphdr));

    // Copy SimDNS query packet into datagram after the IP header
    memcpy(datagram  + sizeof(struct iphdr), &query_packet, sizeof(struct SimDNSQueryPacket));

    // Send the packet
    if (sendto(client_socket, datagram, ntohs(iph->tot_len), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Packet send failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    //use select call to receive the response from the server and handle the response
    //if the response is not received within a certain time, resend the query until max retries are reached
    //if max retries are reached, remove the query from the pending queries table
    int activity;
    fd_set readfds;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    FD_ZERO(&readfds);
    FD_SET(client_socket, &readfds);
    //do until max retries are reached
    //find the query in the pending queries table
    int i=find_pending_query(query_packet.id);
    while(pending_queries[i].num_retries < RETRY_LIMIT){
        //wait for the response from the server ,

        activity = select(client_socket + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0) {
            perror("Select failed");
            exit(EXIT_FAILURE);
        }
        if (activity == 0) {
            // Query timed out
            printf("Query %d timed out. Retrying...\n", query_packet.id);
            //increment the number of retries
            pending_queries[i].num_retries++;
            //resend the query
            if (sendto(client_socket, datagram, ntohs(iph->tot_len), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("Packet send failed");
                close(client_socket);
                exit(EXIT_FAILURE);
            }
        }
        else{
            //clear the buffer
            char buffer[BUF_SIZE];
            memset(buffer, 0, BUF_SIZE);
            //receive the response from the server
            if (recvfrom(client_socket, buffer, sizeof(buffer), 0, NULL,NULL) < 0) {
                perror("Receive failed");
                exit(EXIT_FAILURE);
            }
            // printf("Received packet of length: %ld\n", strlen(buffer));
            // printf("Received packet: %s\n", buffer);

            // extract the header and print the ethernet and ip header
            // struct ethhdr *eth_header = (struct ethhdr *)buffer;
            struct iphdr *ip_header = (struct iphdr *)(buffer );
            
            //extract simdns query response packet from the buffer
            struct SimDNSResponsePacket *response_packet = (struct SimDNSResponsePacket *)(buffer + sizeof(struct iphdr));
            //check if the response packet is valid
            if (ip_header->protocol != SIMDNS_PROTOCOL) {
                // printf("Invalid response packet received.\n");
                continue;
            }
            //check if the response packet is a  query response
            if (response_packet->message_type != 1) {
                // printf("Invalid response message type----it is query .\n");
                continue;
            }
            //drop the packet with a certain probability
            if (drop_packet(drop_probability)) {
                printf("Dropped packet with ID: %d\n", response_packet->id);
                continue;
            }



            printf("---------------------------\n\nReceived response packet with ID: %d\n", response_packet->id);
            // printf("Message Type: %d\n", response_packet->message_type);
            printf("Number of queries: %d\n", response_packet->num_queries);
            // Print the domain names and IP addresses

            //find the query in the pending queries table
            int j=find_pending_query(response_packet->id);
            // for each domain name asked in the query we get from pending queries table , we print the corressponding ip address from the response packet
            // in query string from pending queries table , tokenise the domain names with space , to get the domain names and print the corresponding ip address
            
            char *token;
            char *str = strdup(pending_queries[j].query_string); // Make a copy of the query string
            token = strtok(str, " "); // Tokenize by space
            // Skip the first token ("getIP")
            token = strtok(NULL, " ");
            //Skip the second token (number of queries)
            token = strtok(NULL, " ");
            // Extract and add domain names to the packet
            int k=0;

            while (token != NULL) {
                // Print the domain name and IP address
                printf("Domain %s: %s\n",token, response_packet->queries[k].ip_address);
                // Get the next token
                token = strtok(NULL, " ");
                k++;
            }
            printf("---------------------------\n\n");
            // Remove the query from the pending queries table
            remove_pending_query(response_packet->id);
            break;
        }
    }



    return 1;
}


//function to parse the query taken from the user
int parse_query(char *query_string) {
    // Parse query string to extract number of queries and domain names
    int num_queries;
    char domain[MAX_DOMAIN_SIZE + 1];  // Buffer to store domain name

    // Tokenize query string to extract number of queries
    char query_string_copy[strlen(query_string) + 1]; // Create a copy of the query string
    strcpy(query_string_copy, query_string);

    char *token = strtok(query_string_copy, " ");
    if (token == NULL || strcmp(token, "getIP") != 0) {
        printf("Invalid query format. Expected format: getIP N <domain-1> <domain-2> ... <domain-N>\n");
        return 0;
    }

    // Tokenize again to extract number of queries
    token = strtok(NULL, " ");
    if (token == NULL) {
        printf("Number of queries not specified.\n");
        return 0;
    }
    num_queries = atoi(token);
    if (num_queries <= 0 || num_queries > MAX_QUERIES) {
        printf("Invalid number of queries.\n");
        return 0;
    }

    // Calculate the number of packets needed to send all domain names
    int num_packets = (num_queries + MAX_QUERIES - 1) / MAX_QUERIES;

    for (int packet_num = 0; packet_num < num_packets; packet_num++) {
        // Calculate the number of domain names to include in this packet
        int domains_in_packet = (num_queries < MAX_QUERIES) ? num_queries : MAX_QUERIES;
        printf("Domains in packet: %d\n", domains_in_packet);

        // Send each domain name as a separate query in this packet
        for (int i = 0; i < domains_in_packet; i++) {
            token = strtok(NULL, " ");  // Get next token (domain name)
            if (token == NULL) {
                printf("Error: Insufficient domain names specified.\n");
                return 0;
            }
            strncpy(domain, token, MAX_DOMAIN_SIZE);  // Copy domain name to buffer
            domain[MAX_DOMAIN_SIZE] = '\0';  // Ensure null termination

            // Validate domain name format
            if (!validate_domain(domain)) {
                printf("Error: Invalid domain name format.\n");
                return 0;
            }
    
                // Print the domain name
                printf("Domain %d: %s\n", i + 1, domain);
        }
    }
    return 1;

}






// Function to calculate checksum
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int main() {
    int client_socket;
    struct sockaddr_ll server_addr, client_addr;
    socklen_t client_addr_len;
    char buffer[BUF_SIZE];
    // struct sockaddr_ll server_addr;
    char interface_name[] = "wlo1"; // Change this to your desired interface name
    struct ifreq ifr;

    // Create raw socket to capture all packets till Ethernet
    client_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    printf("Socket created with sockid:%d\n",client_socket);
    if (client_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Get interface index by name
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    if (ioctl(client_socket, SIOCGIFINDEX, &ifr) < 0) {
        perror("Interface name to index conversion failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Bind socket to local IP address using struct sockaddr_ll server_addr
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sll_family = AF_PACKET;
    server_addr.sll_protocol = htons(ETH_P_ALL);
    server_addr.sll_ifindex = ifr.ifr_ifindex; // Interface index

    // Bind the socket
    if (bind(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Socket bound to local IP successfully.\n");


    // take query input from user
    char query_string[BUF_SIZE];
    while(1){
        //clear query string
        memset(query_string, 0, BUF_SIZE);
        printf("---------------------------\n\n");
    printf("Enter query: ");
    fgets(query_string, BUF_SIZE, stdin);
    query_string[strcspn(query_string, "\n")] = 0; // Remove newline character
    //check if the query is exit
    if(strcmp(query_string,"exit") == 0){
        break;
    }

    if(parse_query(query_string)){
            send_query(client_socket, query_string);
        //  printf("Query sent to server.\n");
        



    }

    }
    // Send the query to the server



    close(client_socket);

    return 0;
}