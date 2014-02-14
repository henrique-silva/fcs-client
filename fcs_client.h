#ifndef _FCS_CLIENT_H_
#define _FCS_CLIENT_H_

#if defined( __GNUC__)
#define PACKED __attribute__ ((packed))
#else
#error "Unsupported compiler?"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <netinet/tcp.h>

#include <bsmp/client.h>

struct _recv_pkt_t {
  uint8_t data[BSMP_MAX_MESSAGE];
};

typedef struct _recv_pkt_t recv_pkt_t;

struct _send_pkt_t {
  uint8_t data[BSMP_MAX_MESSAGE];
};

typedef struct _send_pkt_t send_pkt_t;

#endif
