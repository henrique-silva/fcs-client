#ifndef _TRANSPORT_ETHERNET_
#define _TRANSPORT_ETHERNET_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

int ethnernet_sendall(int fd, uint8_t *buf, uint32_t *len);
int ethernet_recvall(int fd, uint8_t *buf, uint32_t *len);
void *get_in_addr(struct sockaddr *sa);
int ethernet_connection(int *fd, char *hostname, char* port);

extern const struct transport_ops ethernet_ops;

#endif
