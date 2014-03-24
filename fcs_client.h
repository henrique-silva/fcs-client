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
#include <inttypes.h>
#include <sys/types.h>

#include <bsmp/client.h>

struct _recv_pkt_t {
  uint8_t data[BSMP_MAX_MESSAGE];
} PACKED;

typedef struct _recv_pkt_t recv_pkt_t;

struct _send_pkt_t {
  uint8_t data[BSMP_MAX_MESSAGE];
} PACKED;

typedef struct _send_pkt_t send_pkt_t;

#endif
