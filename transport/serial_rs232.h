#ifndef _TRANSPORT_SERIAL_RS232_
#define _TRANSPORT_SERIAL_RS232_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

int serial_set_interface_attribs (int fd, int speed, int parity);
int serial_set_blocking (int fd, int should_block);

int serial_rs232_sendall(int fd, uint8_t *buf, uint32_t *len);
int serial_rs232_recvall(int fd, uint8_t *buf, uint32_t *len);
int serial_rs232_connection(int *fd, char *hostname, char* port);

extern const struct transport_ops serial_rs232_ops;

#endif
