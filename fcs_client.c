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
#include <getopt.h>

#include "fcs_client.h"

#define C "CLIENT: "
#define PACKET_SIZE             SLLP_MAX_MESSAGE
#define PACKET_HEADER           SLLP_HEADER_SIZE

#define TRY(name, func)\
    do {\
        enum sllp_err err = func;\
        if(err) {\
            fprintf(stderr, C "%s: %s\n", name, sllp_error_str(err));\
            exit(-1);\
        }\
    }while(0)

#define PORT "8080" // the port client will be connecting to
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

const char* program_name;
char *hostname = NULL;

/* Our socket */
int sockfd;

/* Our send/receive packet */
recv_pkt_t recv_pkt;
send_pkt_t send_pkt;

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

// Command-line handling

void print_usage (FILE* stream, int exit_code)
{
  fprintf (stream, "Usage:  %s options \n", program_name);
  fprintf (stream,
           "  -h  --help                  Display this usage information.\n"
           "  -v  --verbose               Print verbose messages.\n"
           "  -b  --blink                 Blink board leds\n"
           "  -r  --reset                 Reconfigure all options to its defaults\n"
           "  -x  --kx        <value>     Sets parameter Kx to <value>\n"
           "  -y  --ky        <value>     Sets parameter Ky to <value>\n"
           "  -s  --ksum      <value>     Sets parameter Ksum to <value>\n"
           "  -o  --hostname  <host>      Sets hostname to <host>\n"
           );
  exit (exit_code);
}

static struct option long_options[] =
{
    {"help", no_argument, NULL, 'h'},
    {"verbose", no_argument, NULL, 'v'},
    {"blink", no_argument, NULL, 'b'},
    {"reset", no_argument, NULL, 'r'},
    {"kx", required_argument, NULL, 'x'},
    {"ky", required_argument, NULL, 'y'},
    {"ksum", required_argument, NULL, 's'},
    {"hostname", required_argument, NULL, 'o'},
    {NULL, 0, NULL, 0}
};

struct call_func_t {
    const char *name;
    int call;
    uint8_t param_in[sizeof(uint32_t)*4]; // 4 32-bits variables
    uint8_t param_out[sizeof(uint32_t)]; // 1 32-bit variable
};

#define BLINK_FUNC_ID           0
#define BLINK_FUNC_NAME         "blink"
#define RESET_FUNC_ID           1
#define RESET_FUNC_NAME         "reset"
#define SET_KX_ID               2
#define SET_KX_NAME             "set_kx"
#define SET_KY_ID               3
#define SET_KY_NAME             "set_ky"
#define SET_KSUM_ID             4
#define SET_KSUM_NAME           "set_ksum"
#define END_ID                  5

static struct call_func_t call_func[END_ID] =
{
    {BLINK_FUNC_NAME            , 0, {0}, {0}},
    {RESET_FUNC_NAME            , 0, {0}, {0}},
    {SET_KX_NAME                , 0, {0}, {0}},
    {SET_KY_NAME                , 0, {0}, {0}},
    {SET_KSUM_NAME              , 0, {0}, {0}}
};

int main(int argc, char *argv[])
{
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    int yes = 1;

    int verbose = 0;
    int ch;
    
    program_name = argv[0];

    // loop over all of the options
    while ((ch = getopt_long(argc, argv, "hvbrx:y:s:o:", long_options, NULL)) != -1)
    {
         // check to see if a single character or long option came through
         switch (ch)
         {
              case 'h':
                  print_usage(stderr, 0);
              case 'v':
                  verbose = 1;
                  break;
              // Blink leds
              case 'b':
                  call_func[BLINK_FUNC_ID].call = 1;
                  break;
              // Reset to default
              case 'r':
                  call_func[RESET_FUNC_ID].call = 1;
                  break;
              // Set KX
              case 'x':
                  call_func[SET_KX_ID].call = 1;
                  *((uint32_t *)call_func[SET_KX_ID].param_in) = (uint32_t) atoi(optarg);
                  break;
              // Set KY
              case 'y':
                  call_func[SET_KY_ID].call = 1;
                  *((uint32_t *)call_func[SET_KY_ID].param_in) = (uint32_t) atoi(optarg);
                  break;
              // Set Ksum
              case 's':
                  call_func[SET_KSUM_ID].call = 1;
                  *((uint32_t *)call_func[SET_KSUM_ID].param_in) = (uint32_t) atoi(optarg);
                  break;
              // Set Hostname
              case 'o':
                  hostname = strdup(optarg);
                  break;
              case ':':
              case '?':   /* The user specified an invalid option.  */
                   print_usage (stderr, 1);
              case -1:    /* Done with options.  */
                  break;
              default:
                fprintf(stderr, "%s: bad option\n", program_name);
                print_usage(stderr, 1);
         }    
    }

    if (hostname == NULL) {
        fprintf(stderr, "%s: hostname not set!\n", program_name);
        print_usage(stderr, 1);
    } 

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, PORT, &hints, &servinfo)) != 0) {
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

    // Call all the functions the user specified with its parameters
    //struct sllp_func_info *func_blink_leds = &funcs->list[0];
    //printf(C"Server, start blinking leds...\n");
    //TRY("blink leds", sllp_func_execute(client, func_blink_leds,
    //                                     &func_error, NULL, NULL));
    
    struct sllp_func_info *func;
    uint8_t func_error;
    
    for (i = 0; i < ARRAY_SIZE(call_func); ++i) {
        if (call_func[i].call) {
            func = &funcs->list[i];
            TRY((call_func[i].name), sllp_func_execute(client, func,
                                            &func_error, call_func[i].param_in, call_func[i].param_out));
        }
    }

    close(sockfd);

exit_destroy:
    sllp_client_destroy(client);
    puts("SLLP deallocated");
exit_close:
    close(sockfd);
    puts("Socket closed");
    free (hostname);
    return 0;
}
