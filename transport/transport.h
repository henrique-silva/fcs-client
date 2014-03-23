#ifndef _TRANSPORT_H_
#define _TRANSPORT_H_

#include <stddef.h>
#include <inttypes.h>

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

struct transport_ops {
    int (*bpm_connection)(int *fd, char *hostname, char* port);
    int (*bpm_send)(int fd, uint8_t *buf, uint32_t *len);
    int (*bpm_recv)(int fd, uint8_t *buf, uint32_t *len);
};

struct transport_s {
    int fd;
    const struct transport_ops *ops;
};

#endif
