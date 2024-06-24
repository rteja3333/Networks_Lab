#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <netdb.h>
#define SIMDNS_PROTOCOL 254
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#define MAX_QUERIES 8
#define MAX_DOMAIN_SIZE 32
#define BUF_SIZE 1024
#include <netpacket/packet.h> // For sockaddr_ll
#include <netinet/if_ether.h> // For ETH_P_ALL
#include <net/if.h> // For ifreq
#include <sys/ioctl.h> // For ioctl
#include <netinet/ip.h> // For IP_MAXPACKET
#include <netinet/udp.h> // For UDP header
#include <netinet/ip_icmp.h> // Provides declarations for icmp header
#include <netinet/tcp.h> // Provides declarations for tcp header
#include <netinet/ip.h> // Provides declarations for ip header
#include <netinet/in.h> // Provides declarations for in_addr
#include <netinet/if_ether.h> // Provides declarations for ether_header
// #include <linux/if_packet.h> // For sockaddr_ll
// #include <linux/if_ether.h> // For ETH_P_ALL
// #include <linux/if_arp.h> // For ARPHRD_ETHER
#include <sys/socket.h> // Provides declarations for socket
#include <netdb.h> // Provides declarations for gethostbyname

float drop_probability = 0;

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




// Function to generate a simDNS response packet for a given query packet
struct SimDNSResponsePacket* generate_response(struct SimDNSQueryPacket *query_packet) {
    // Allocate memory for the response packet
    struct SimDNSResponsePacket *response_packet = (struct SimDNSResponsePacket *)malloc(sizeof(struct SimDNSResponsePacket));
    if (response_packet == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    // Copy the ID and message type from the query packet
    response_packet->id = query_packet->id;
    response_packet->message_type = 1; // Response message type

    // Copy the number of queries from the query packet
    response_packet->num_queries = query_packet->num_queries;
    printf("------------------------\nSimDNS Packet\nNumber of queries: %d\n", query_packet->num_queries);
    //for each domain name in the query packet generate the ip address by using gethostbyname and store it in the response packet
    for (int i = 0; i < query_packet->num_queries; i++) {
        // Copy the query size and domain name from the query packet
        response_packet->queries[i].valid_ip = 0;
        response_packet->queries[i].ip_address[0] = '\0';
        // Get the IP address for the domain name
        struct hostent *host = gethostbyname(query_packet->queries[i].domain_name);
        //print the ip address of the domain name
        printf("IP address of %s: %s\n", query_packet->queries[i].domain_name, inet_ntoa(*(struct in_addr *)host->h_addr_list[0]));
        if (host != NULL) {
            // Copy the IP address to the response packet
            response_packet->queries[i].valid_ip = 1;
            strcpy(response_packet->queries[i].ip_address, inet_ntoa(*(struct in_addr *)host->h_addr_list[0]));
        }
    }
    return response_packet;
}


//FUNCTION TO DROP THE PACKET
int drop_packet(float drop_probability){
    float random_number = (float)rand() / RAND_MAX;
    if (random_number < drop_probability) {
        return 1;
    }
    return 0;
}






int main() {
    int server_socket;
    struct sockaddr_ll server_addr, client_addr;
    socklen_t client_addr_len;
    char buffer[BUF_SIZE];
    // struct sockaddr_ll server_addr;
    char interface_name[] = "wlo1"; // Change this to your desired interface name
    struct ifreq ifr;

    // Create raw socket to capture all packets till Ethernet
    server_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Get interface index by name
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
    if (ioctl(server_socket, SIOCGIFINDEX, &ifr) < 0) {
        perror("Interface name to index conversion failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Bind socket to local IP address using struct sockaddr_ll server_addr
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sll_family = AF_PACKET;
    server_addr.sll_protocol = htons(ETH_P_ALL);
    server_addr.sll_ifindex = ifr.ifr_ifindex; // Interface index

    // Bind the socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Socket bound to local IP successfully.\n");




    // memset(&server_addr, 0, sizeof(server_addr));
    // server_addr.sin_family = AF_INET;
    // server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    //     perror("Bind failed");
    //     exit(EXIT_FAILURE);
    // }

    // Main loop to receive and process packets
    while (1) {
        // Receive packet
        memset(buffer, 0, sizeof(buffer));
        if (recvfrom(server_socket, buffer, sizeof(buffer), 0, NULL,NULL) < 0) {
            perror("Receive failed");
            exit(EXIT_FAILURE);
        }

        // Drop the packet with a certain probability
        


        // printf("Received packet of length: %ld\n", strlen(buffer));
        // printf("Received packet: %s\n", buffer);

        // extract the header and print the ethernet and ip header
        // struct ethhdr *eth_header = (struct ethhdr *)buffer;
        struct iphdr *ip_header = (struct iphdr *)(buffer );
        
        //extract simdns query packet from buffer
        struct SimDNSQueryPacket *simdns_query_packet = (struct SimDNSQueryPacket *)(buffer + sizeof(struct iphdr));    

        // break;




        // Parse Ethernet header
        // struct ethhdr *eth_header = (struct ethhdr *)buffer;
        // if (ntohs(eth_header->h_proto) == ETH_P_IP) {
            // Parse IP header
            // struct iphdr *ip_header = (struct iphdr *)(buffer + sizeof(struct ethhdr));
            if (ip_header->protocol == SIMDNS_PROTOCOL&&simdns_query_packet->message_type == 0) {
                   //drop the packet with a certain probability
                if (drop_packet(drop_probability)) {
                    printf("dropped query packet with id : %d\n", simdns_query_packet->id);
                    continue;
                }

                    //print the query packet
                    // printf("SimDNS Query Packet\n");
                    // printf("   |-ID: %u\n", simdns_query_packet->id);
                    // printf("   |-Message Type: %s\n", simdns_query_packet->message_type ? "Response" : "Query");
                    // printf("   |-Number of Queries: %u\n", simdns_query_packet->num_queries);
                    // for (int i = 0; i < simdns_query_packet->num_queries; i++) {
                    //     printf("   |-Query %d: %s\n", i+1, simdns_query_packet->queries[i].domain_name);
                    // }
                    // printf("\n");



                    // //     printf("Ethernet Header\n");
                    // // printf("   |-Source Address      : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X \n", eth_header->h_source[0], eth_header->h_source[1], eth_header->h_source[2], eth_header->h_source[3], eth_header->h_source[4], eth_header->h_source[5]);
                    // // printf("   |-Destination Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X \n", eth_header->h_dest[0], eth_header->h_dest[1], eth_header->h_dest[2], eth_header->h_dest[3], eth_header->h_dest[4], eth_header->h_dest[5]);
                    // // printf("   |-Protocol            : %u \n", (unsigned short)eth_header->h_proto);
                    // printf("\n");
                    // printf("IP Header\n");
                    // printf("   |-Version        : %d\n", (unsigned int)ip_header->version);
                    // printf("   |-Internet Header Length  : %d DWORDS or %d Bytes\n", (unsigned int)ip_header->ihl, ((unsigned int)(ip_header->ihl)) * 4);
                    // printf("   |-Type Of Service : %d\n", (unsigned int)ip_header->tos);
                    // printf("   |-Total Length    : %d Bytes\n", ntohs(ip_header->tot_len));
                    // printf("   |-Identification  : %d\n", ntohs(ip_header->id));
                    // printf("   |-Time To Live    : %d\n", (unsigned int)ip_header->ttl);
                    // printf("   |-Protocol        : %d\n", (unsigned int)ip_header->protocol);
                    // printf("   |-Header Checksum : %d\n", ntohs(ip_header->check));
                    // printf("   |-Source IP       : %s\n", inet_ntoa(*(struct in_addr *)&ip_header->saddr));
                    // printf("   |-Destination IP  : %s\n", inet_ntoa(*(struct in_addr *)&ip_header->daddr));
                    // printf("\n");

                //if the packet is a query packet
                if (simdns_query_packet->message_type == 0) {
                    //client address 
                    //take ip from the ip header
                    client_addr.sll_addr[0] = ip_header->saddr & 0xFF;
                    client_addr.sll_addr[1] = (ip_header->saddr >> 8) & 0xFF;
                    client_addr.sll_addr[2] = (ip_header->saddr >> 16) & 0xFF;
                    client_addr.sll_addr[3] = (ip_header->saddr >> 24) & 0xFF;
                    client_addr.sll_addr[4] = 0;
                    client_addr.sll_addr[5] = 0;
                    client_addr.sll_halen = 6;

                    client_addr.sll_family = AF_PACKET;
                    client_addr.sll_protocol = htons(ETH_P_ALL);
                    client_addr.sll_ifindex = ifr.ifr_ifindex; // Interface index
                    //client address length
                    client_addr_len = sizeof(client_addr);

                    // clear the buffer
                    // memset(buffer, 0, sizeof(buffer));

                    //generate a response packet
                    struct SimDNSResponsePacket *response_packet = generate_response(simdns_query_packet);
                    //add the ip header to the response packet
                    struct iphdr *response_ip_header = (struct iphdr *)(buffer );
                    // make the ip header of the response packet
                    response_ip_header->version = 4;
                    response_ip_header->ihl = 5;
                    response_ip_header->tos = 0;
                    response_ip_header->tot_len = sizeof(struct iphdr) + sizeof(struct SimDNSResponsePacket);
                    response_ip_header->id = htons(54321);
                    response_ip_header->frag_off = 0;
                    response_ip_header->ttl = 255;
                    response_ip_header->protocol = SIMDNS_PROTOCOL;
                    response_ip_header->check = 0;
                    response_ip_header->saddr = ip_header->daddr;
                    response_ip_header->daddr = ip_header->saddr;
                    response_ip_header->check = 0;
                    
                    //add the ip header to the response packet
                    memcpy(buffer + sizeof(struct iphdr), response_packet, sizeof(struct SimDNSResponsePacket));

                    //send the response packet to the client
                    sendto(server_socket, buffer, sizeof(struct iphdr) + sizeof(struct SimDNSResponsePacket), 0, (struct sockaddr *)&client_addr, client_addr_len);


                    //print the response packet with the    ip header
                    // printf("SimDNS Response Packet\n");
                    // printf("   |-ID: %u\n", response_packet->id);
                    // printf("   |-Message Type: %s\n", response_packet->message_type ? "Response" : "Query");
                    // printf("   |-Number of Queries: %u\n", response_packet->num_queries);
                    // for (int i = 0; i < response_packet->num_queries; i++) {
                    //     printf("   |-Query %d: %s\n", i+1, response_packet->queries[i].ip_address);
                    // }
                    // printf("\n");
                    // printf("IP Header\n");
                    // printf("   |-Version        : %d\n", (unsigned int)response_ip_header->version);
                    // printf("   |-Internet Header Length  : %d DWORDS or %d Bytes\n", (unsigned int)response_ip_header->ihl, ((unsigned int)(response_ip_header->ihl)) * 4);
                    // printf("   |-Type Of Service : %d\n", (unsigned int)response_ip_header->tos);
                    // printf("   |-Total Length    : %d Bytes\n", ntohs(response_ip_header->tot_len));
                    // printf("   |-Identification  : %d\n", ntohs(response_ip_header->id));
                    // printf("   |-Time To Live    : %d\n", (unsigned int)response_ip_header->ttl);
                    // printf("   |-Protocol        : %d\n", (unsigned int)response_ip_header->protocol);
                    // printf("   |-Header Checksum : %d\n", ntohs(response_ip_header->check));
                    // printf("   |-Source IP       : %s\n", inet_ntoa(*(struct in_addr *)&response_ip_header->saddr));
                    // printf("   |-Destination IP  : %s\n", inet_ntoa(*(struct in_addr *)&response_ip_header->daddr));
                    // printf("\n");

                


                // Generate simDNS response
                // char *response_packet = generate_response(query_data);

                // Send the response to the client
                //sendto(server_socket, response_packet, strlen(response_packet), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
            }
            // break;
        }
    }

    return 0;
}











