/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <inttypes.h>
#include <arpa/inet.h>

#include "fcs_client.h"

#define C "CLIENT: "
#define PACKET_SIZE             SLLP_MAX_MESSAGE
#define PACKET_HEADER           SLLP_HEADER_SIZE

#define TRY(name, func)\
    do {\
        enum sllp_err err = func;\
        if(err) {\
            fprintf(stderr, C name": %s\n", sllp_error_str(err));\
            exit(-1);\
        }\
    }while(0)

#define PORT "8080" // the port client will be connecting to

/* Our socket */
int sockfd;

/* Our send/receive packet */
recv_pkt_t recv_pkt;
send_pkt_t send_pkt;

/* Our receive packet */
//recv_pkt_t recv_pkt;
//send_pkt_t send_pkt;
//
//struct sllp_raw_packet recv_packet = {.data = recv_pkt.data };
//struct sllp_raw_packet send_packet = {.data = send_pkt.data };

/***************************************************************/
/********************** Utility functions **********************/
/***************************************************************/

int sendall(uint8_t *buf, uint32_t *len)
{
    uint32_t total = 0;        // how many bytes we've sent
    uint32_t bytesleft = *len; // how many we have left to send
    uint32_t n;

    while(total < *len) {
        n = send(sockfd, (char *)buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

int recvall(uint8_t *buf, uint32_t *len)
{
    uint32_t total = 0;        // how many bytes we've recv
    uint32_t bytesleft = *len; // how many we have left to recv
    uint32_t n;

    while(total < *len) {
        n = recv(sockfd, (char *)buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/***************************************************************/
/**********************      Functions       *******************/
/***************************************************************/

void print_packet (char* pre, uint8_t *data, uint32_t size)
{
    printf("%s: [", pre);

    if(size < 32)
    {
        unsigned int i;
        for(i = 0; i < size; ++i)
            printf("%02X ", data[i]);
        printf("]\n");
    }
    else
        printf("%d bytes ]\n", size);
}

int bpm_send(uint8_t *data, uint32_t *count)
{
    uint8_t  packet[SLLP_MAX_MESSAGE];
    uint32_t packet_size = *count;
    uint32_t len = *count;

    memcpy (packet, data, *count);

    print_packet("SEND", packet, packet_size);

    int ret = sendall(packet, &len);
    if(len != packet_size) {
        if(ret < 0)
            perror("send");
        return -1;
    }

    return 0;
}

int bpm_recv(uint8_t *data, uint32_t *count)
{
    uint8_t packet[PACKET_SIZE] = {0};
    uint32_t packet_size;
    uint32_t len = PACKET_HEADER;

    int ret = recvall(packet, &len);
    if(len != PACKET_HEADER) {
          if(ret < 0)
              perror("recv");
          return -1;
    }

    printf("bpm_recv: received %d bytes (header)!\n", PACKET_HEADER);

    //uint32_t remaining = (packet[2] << 8) + packet[3];
    uint32_t remaining = (packet[1] << 8) + packet[2];
    len = remaining;

     printf("bpm_recv: %d bytes to recv!\n", remaining);

    ret = recvall(packet + PACKET_HEADER, &len);
    if(len != remaining) {
        if(ret < 0)
          perror("recv");
        return -1;
    }

    printf("bpm_recv: received payload!\n");

    packet_size = PACKET_HEADER + remaining;

    print_packet("RECV", packet, packet_size);

    *count = packet_size;
    memcpy(data, packet, *count);

    return 0;
}

int main(int argc, char *argv[])
{
    //int numbytes;
    //char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    int yes = 1;

    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        /* This is important for correct behaviour */
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

     // Create a new client instance
    sllp_client_t *client = sllp_client_new(bpm_send, bpm_recv);

    if(!client)
    {
        fprintf(stderr, "Error allocating SLLP instance\n");
        goto exit_close;
    }

    // Initialize the client instance (communication must be already working)
    enum sllp_err err;
    if((err = sllp_client_init(client)))
    {
        fprintf(stderr, "sllp_client_init: %s\n", sllp_error_str(err));
        goto exit_destroy;
    }

    //if (tcp_client_handle_server(sockfd, &send_pkt) == -1) {
    //    fprintf(stderr, "clinet: failed to handle client\n");
    //    return -3;
    //}

    struct sllp_func_info_list *funcs;
    TRY("funcs_list", sllp_get_funcs_list(client, &funcs));

    // Check the number of functions
    printf("\n"C"Server has %d Functions(s):\n", funcs->count);
    int i;
    for(i = 0; i < funcs->count; ++i) {
        printf(C" ID[%d] INPUT[%2d bytes] OUTPUT[%2d bytes]\n",
                funcs->list[i].id,
                funcs->list[i].input_size,
                funcs->list[i].output_size);
    }

    // Call the first one
    struct sllp_func_info *func_convert_ads = &funcs->list[0];
    printf(C"Server, start the conversions of the A/D converters. NOW!!!\n");
    uint8_t func_error;
    TRY("convert ads", sllp_func_execute(client, func_convert_ads,
                                         &func_error, NULL, NULL));
    func_convert_ads = &funcs->list[1];
    printf(C"Server, start blink leds. NOW!!!\n");
    TRY("blink leds", sllp_func_execute(client, func_convert_ads,
                                         &func_error, NULL, NULL));

    close(sockfd);

exit_destroy:
    sllp_client_destroy(client);
    puts("SLLP deallocated");
exit_close:
    close(sockfd);
    puts("Socket closed");
    return 0;
}
