#include "transport.h"
#include "serial_rs232.h"
#include "revision.h"
#include "debug.h"

/***************************************************************/
/********************** Serial functions **********************/
/***************************************************************/

int serial_set_interface_attribs (int fd, int speed, int parity)
{
    struct termios tty;
    memset (&tty, 0, sizeof tty);
    if (tcgetattr (fd, &tty) != 0)
    {
        perror ("client: rs232_tcsgetattr");
        return -1;
    }

    cfsetospeed (&tty, speed);
    cfsetispeed (&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK;         // ignore break signal
    tty.c_lflag = 0;                // no signaling chars, no echo,
    // no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
    // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr (fd, TCSANOW, &tty) != 0)
    {
        perror ("client: rs232_tcssetattr");
        return -1;
    }
    return 0;
}

int serial_set_blocking (int fd, int should_block)
{
    struct termios tty;
    memset (&tty, 0, sizeof tty);

    if (tcgetattr (fd, &tty) != 0) {
        perror ("client: rs232_tcsgetattr");
        return -1;
    }

    tty.c_cc[VMIN]  = should_block ? 1 : 0;
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    if (tcsetattr (fd, TCSANOW, &tty) != 0) {
        perror ("client: rs232_tcsetattr");
        return -1;
    }

    return 0;
}

/***************************************************************/
/********************** Utility functions **********************/
/***************************************************************/

int serial_rs232_sendall(int fd, uint8_t *buf, uint32_t *len)
{
    uint32_t total = 0;        // how many bytes we've sent
    uint32_t bytesleft = *len; // how many we have left to send
    int32_t n;

    while(total < *len) {
        n = write(fd, (char *)buf+total, bytesleft);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

int serial_rs232_recvall(int fd, uint8_t *buf, uint32_t *len)
{
    uint32_t total = 0;        // how many bytes we've recv
    uint32_t bytesleft = *len; // how many we have left to recv
    int32_t n;

    while(total < *len) {
        n = read(fd, (char *)buf+total, bytesleft);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

/***************************************************/
/************ Socket-specific Functions *************/
/***************************************************/
int serial_rs232_connection(int *fd, char *hostname, char* port)
{
    (void) port;

    *fd = open (hostname, O_RDWR | O_NOCTTY | O_SYNC);

    if (*fd < 0) {
        perror ("client: rs232_open: ");
        return -1;
    }

    //serial_set_interface_attribs (*fd, B115200, 0);  // set speed to 115,200 bps, 8n1 (no parity)
    serial_set_interface_attribs (*fd, B9600, 0);  // set speed to 115,200 bps, 8n1 (no parity)
    serial_set_blocking (*fd, 1);                // set blocking

    return *fd;
}

const struct transport_ops serial_rs232_ops = {
    .bpm_connection = serial_rs232_connection,
    .bpm_recv = serial_rs232_recvall,
    .bpm_send = serial_rs232_sendall
};
