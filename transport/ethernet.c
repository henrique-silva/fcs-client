#include "transport.h"
#include "ethernet.h"
#include "revision.h"
#include "debug.h"

/***************************************************************/
/********************** Utility functions **********************/
/***************************************************************/

int ethnernet_sendall(int fd, uint8_t *buf, uint32_t *len)
{
    uint32_t total = 0;        // how many bytes we've sent
    uint32_t bytesleft = *len; // how many we have left to send
    int32_t n;

    while(total < *len) {
        n = send(fd, (char *)buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

int ethernet_recvall(int fd, uint8_t *buf, uint32_t *len)
{
    uint32_t total = 0;        // how many bytes we've recv
    uint32_t bytesleft = *len; // how many we have left to recv
    int32_t n;

    while(total < *len) {
        n = recv(fd, (char *)buf+total, bytesleft, 0);
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

/***************************************************/
/************ Socket-specific Functions *************/
/***************************************************/
int ethernet_connection(int *fd, char *hostname, char* port)
{
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    int yes = 1;

    // Socket specific part
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((*fd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        /* This is important for correct behaviour */
        if (setsockopt(*fd, IPPROTO_TCP, TCP_NODELAY, &yes,
                    sizeof(int)) == -1) {
            perror("setsockopt");
            return -2;
            //exit(1);
        }

        if (connect(*fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(*fd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return -3;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    DEBUGP("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    return *fd;
}

const struct transport_ops ethernet_ops = {
    .bpm_connection = ethernet_connection,
    .bpm_recv = ethernet_recvall,
    .bpm_send = ethnernet_sendall
};
