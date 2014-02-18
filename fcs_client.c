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
#define PACKET_SIZE             BSMP_MAX_MESSAGE
#define PACKET_HEADER           BSMP_HEADER_SIZE

#define TRY(name, func)\
    do {\
        enum bsmp_err err = func;\
        if(err) {\
            fprintf(stderr, C "%s: %s\n", name, bsmp_error_str(err));\
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
    int32_t n;

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
    int32_t n;

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
    uint8_t  packet[BSMP_MAX_MESSAGE];
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
           "  -h  --help                      Display this usage information.\n"
           "  -v  --verbose                   Print verbose messages.\n"
           "  -b  --blink                     Blink board leds\n"
           "  -r  --reset                     Reconfigure all options to its defaults\n"
           "  -o  --sethostname  <host>       Sets hostname to <host>\n"
           "  -x  --setkx        <value>[nm]  Sets parameter Kx to <value>\n"
           "                                    [in UFIX25_0 format]\n"
           "  -y  --setky        <value>[nm]  Sets parameter Ky to <value>\n"
           "                                   [in UFIX25_0 format]\n"
           "  -s  --setksum      <value>      Sets parameter Ksum to <value>\n"
           "                                   [in FIX25_24 format]\n"
           "  -j  --setswon                   Sets FPGA switching on\n"
           "  -k  --setswoff                  Sets FPGA switching off\n"
           "  -d  --setdivclk    <value>      Sets FPGA switching divider clock to <value>\n"
           "                                    [in number of ADC clock cycles]\n"
           "  -p  --setphaseclk  <value>      Sets FPGA switching phase clock to <value>\n"
           "                                    [in number of ADC clock cycles]\n"
           "  -q  --setadcclk    <value>      Sets FPGA reference ADC clock to <value> [in Hertz]\n"
           "  -i  --setddsfreq   <value>      Sets FPGA DDS Frequency to <value> [in Hertz]\n"
           "  -l  --setsamples   <number of samples>\n"
           "                                  Sets FPGA Acquisition parameters\n"
           "                                  [<number of samples> must be between 4 and\n"
           "                                  ??? (TBD) \n"
           "  -c  --setchan      <channel> \n"
           "                                  Sets FPGA Acquisition parameters\n"
           "                                  [<channel> must be one of the following:\n"
           "                                  0 -> ADC; 1-> TBT Amp; 2 -> TBT Pos\n"
           "                                  3 -> FOFB Amp; 4-> FOFB Pos]\n"
           "  -t  --startacq                  Starts FPGA acquistion with the previous parameters\n"
           "  -X  --getkx                     Gets parameter Kx [nm] in UFIX25_0 format\n"
           "  -Y  --getky                     Gets parameter Ky [nm] in UFIX25_0 format\n"
           "  -S  --getksum                   Gets parameter Ksum in FIX25_24 format\n"
           "  -J  --getsw                     Gets FPGA switching state \n"
           "                                    [0x1 is no switching and 0x3 is switching]\n"
           "  -D  --getdivclk                 Gets FPGA switching divider clock value\n"
           "                                    [in number of ADC clock cycles]\n"
           "  -P  --getphaseclk               Gets FPGA switching phase clock\n"
           "                                    [in number of ADC clock cycles]\n"
           "  -Q  --getadcclk                 Gets FPGA reference ADC clock [in Hertz]\n"
           "  -I  --getddsfreq                Gets FPGA DDS Frequency [in Hertz]\n"
           "  -L  --getsamples                Gets FPGA number of samples of the next acquisition\n"
           "  -C  --getchan                   Gets FPGA data channel of the next acquisition\n"
           "  -B  --getcurve   <channel>\n    Gets FPGA curve data of channel <channel_number>\n"
           "                                  [<channel> must be one of the following:\n"
           "                                  0 -> ADC; 1-> TBT Amp; 2 -> TBT Pos\n"
           "                                  3 -> FOFB Amp; 4-> FOFB Pos]\n"
           "  -E  --getmonitamp               Gets FPGA Monitoring Ampltitude Sample\n"
           "                                  This consists of the following:\n"
           "                                  Monit. Amp 0, Amp 1, Amp 2, Amp 3\n"
           "  -F  --getmonitpos               Gets FPGA Monitoring Position Sample\n"
           "                                  This consists of the following:\n"
           "                                  Monit. X, Y, Q, Sum\n"
           );
  exit (exit_code);
}

static struct option long_options[] =
{
    {"help",            no_argument,         NULL, 'h'},
    {"verbose",         no_argument,         NULL, 'v'},
    {"blink",           no_argument,         NULL, 'b'},
    {"reset",           no_argument,         NULL, 'r'},
    {"sethostname",     required_argument,   NULL, 'o'},
    {"setkx",           required_argument,   NULL, 'x'},
    {"setky",           required_argument,   NULL, 'y'},
    {"setksum",         required_argument,   NULL, 's'},
    {"setswon",         no_argument,         NULL, 'j'},
    {"setswoff",        no_argument,         NULL, 'k'},
    {"setdivclk",       required_argument,   NULL, 'd'},
    {"setphaseclk",     required_argument,   NULL, 'p'},
    {"setadcclk",       required_argument,   NULL, 'q'},
    {"setddsfreq",      required_argument,   NULL, 'i'},
    {"setsamples",      required_argument,   NULL, 'l'},
    {"setchan",         required_argument,   NULL, 'c'},
    {"startacq",        no_argument,         NULL, 't'},
    {"getkx",           no_argument,         NULL, 'X'},
    {"getky",           no_argument,         NULL, 'Y'},
    {"getksum",         no_argument,         NULL, 'S'},
    {"getsw ",          no_argument,         NULL, 'J'},
    {"getdivclk",       no_argument,         NULL, 'D'},
    {"getphaseclk",     no_argument,         NULL, 'P'},
    {"getadcclk",       no_argument,         NULL, 'Q'},
    {"getddsfreq",      no_argument,         NULL, 'I'},
    {"getsamples",      no_argument,         NULL, 'L'},
    {"getchan",         no_argument,         NULL, 'C'},
    {"getcurve",        required_argument,   NULL, 'B'},
    {"getmonitamp",     no_argument,         NULL, 'E'},
    {"getmonitpos",     no_argument,         NULL, 'F'},
    {NULL, 0, NULL, 0}
};

struct call_func_t {
    const char *name;
    int call;
    uint8_t param_in[sizeof(uint32_t)*2]; // 2 32-bits variables
    uint8_t param_out[sizeof(uint32_t)]; // 1 32-bits variable
};

#define BLINK_FUNC_ID           0
#define BLINK_FUNC_NAME         "blink"
#define RESET_FUNC_ID           1
#define RESET_FUNC_NAME         "reset"
#define SET_KX_ID               2
#define SET_KX_NAME             "set_kx"
#define GET_KX_ID               3
#define GET_KX_NAME             "get_kx"
#define SET_KY_ID               4
#define SET_KY_NAME             "set_ky"
#define GET_KY_ID               5
#define GET_KY_NAME             "set_ky"
#define SET_KSUM_ID             6
#define SET_KSUM_NAME           "set_ksum"
#define GET_KSUM_ID             7
#define GET_KSUM_NAME           "get_ksum"
#define SET_SW_ON_ID            8
#define SET_SW_ON_NAME          "set_sw_on"
#define SET_SW_OFF_ID           9
#define SET_SW_OFF_NAME         "set_sw_off"
#define GET_SW_ID               10
#define GET_SW_NAME             "get_sw"
#define SET_SW_DIVCLK_ID        11
#define SET_SW_DIVCLK_NAME      "set_sw_divclk"
#define GET_SW_DIVCLK_ID        12
#define GET_SW_DIVCLK_NAME      "get_sw_divclk"
#define SET_SW_PHASECLK_ID      13
#define SET_SW_PHASECLK_NAME    "set_sw_phaseclk"
#define GET_SW_PHASECLK_ID      14
#define GET_SW_PHASECLK_NAME    "get_sw_phaseclk"
#define SET_ADCCLK_ID           15
#define SET_ADCCLK_NAME         "set_adc_clk"
#define GET_ADCCLK_ID           16
#define GET_ADCCLK_NAME         "get_adc_clk"
#define SET_DDSFREQ_ID          17
#define SET_DDSFREQ_NAME        "set_dds_freq"
#define GET_DDSFREQ_ID          18
#define GET_DDSFREQ_NAME        "get_dds_freq"
#define SET_ACQ_PARAM_ID        19
#define SET_ACQ_PARAM_NAME      "set_acq_param"
#define GET_ACQ_SAMPLES_ID      20
#define GET_ACQ_SAMPLES_NAME    "get_acq_samples"
#define GET_ACQ_CHAN_ID         21
#define GET_ACQ_CHAN_NAME       "get_acq_chan"
#define SET_ACQ_START_ID        22
#define SET_ACQ_START_NAME      "set_acq_start"
#define END_ID                  23

static struct call_func_t call_func[END_ID] =
{
    {BLINK_FUNC_NAME            , 0, {0}, {0}},
    {RESET_FUNC_NAME            , 0, {0}, {0}},
    {SET_KX_NAME                , 0, {0}, {0}},
    {GET_KX_NAME                , 0, {0}, {0}},
    {SET_KY_NAME                , 0, {0}, {0}},
    {GET_KY_NAME                , 0, {0}, {0}},
    {SET_KSUM_NAME              , 0, {0}, {0}},
    {GET_KSUM_NAME              , 0, {0}, {0}},
    {SET_SW_ON_NAME             , 0, {0}, {0}},
    {SET_SW_OFF_NAME            , 0, {0}, {0}},
    {GET_SW_NAME                , 0, {0}, {0}},
    {SET_SW_DIVCLK_NAME         , 0, {0}, {0}},
    {GET_SW_DIVCLK_NAME         , 0, {0}, {0}},
    {SET_SW_PHASECLK_NAME       , 0, {0}, {0}},
    {GET_SW_PHASECLK_NAME       , 0, {0}, {0}},
    {SET_ADCCLK_NAME            , 0, {0}, {0}},
    {GET_ADCCLK_NAME            , 0, {0}, {0}},
    {SET_DDSFREQ_NAME           , 0, {0}, {0}},
    {GET_DDSFREQ_NAME           , 0, {0}, {0}},
    {SET_ACQ_PARAM_NAME         , 0, {0}, {0}},
    {GET_ACQ_SAMPLES_NAME       , 0, {0}, {0}},
    {GET_ACQ_CHAN_NAME          , 0, {0}, {0}},
    {SET_ACQ_START_NAME         , 0, {0}, {0}}
};

#define GET_MONIT_AMP_ID        0
#define GET_MONIT_AMP_NAME      "get_monit_amp"
#define GET_MONIT_POS_ID        1
#define GET_MONIT_POS_NAME      "get_monit_pos"
#define END_MONIT_ID            2

static struct call_func_t call_curve_monit[END_MONIT_ID] =
{
    {GET_MONIT_AMP_NAME         , 0, {0}, {0}},
    {GET_MONIT_POS_NAME         , 0, {0}, {0}}
};

#define ANY_CURVE_TYPE_ID        0
#define ANY_CURVE_TYPE_NAME      "any_type_curve"
#define END_CURVE_TYPE_ID        1

static struct call_func_t call_curve_type[END_CURVE_TYPE_ID] = {
    {ANY_CURVE_TYPE_NAME        , 0, {0}, {0}}
};

// We have 5 curves declared in server:
// 0 -> ADC, 1-> TBTAMP, 2 -> TBTPOS,
// 3 -> FOFBAMP, 4 -> FOFBPOS
#define CURVE_ADC_ID            0
#define CURVE_ADC_NAME          "adc_curve"
#define CURVE_TBTAMP_ID         1
#define CURVE_TBTAMP_NAME       "tbtamp_curve"
#define CURVE_TBTPOS_ID         2
#define CURVE_TBTPOS_NAME       "tbtpos_curve"
#define CURVE_FOFBAMP_ID        3
#define CURVE_FOFBAMP_NAME      "fofbamp_curve"
#define CURVE_FOFMPOS_ID        4
#define CURVE_FOFBPOS_NAME      "fofbpos_curve"
#define END_CURVE_ID            5

static struct call_func_t call_curve[END_CURVE_ID] = {
    {CURVE_ADC_NAME             , 0, {0}, {0}},
    {CURVE_TBTAMP_NAME          , 0, {0}, {0}},
    {CURVE_TBTPOS_NAME          , 0, {0}, {0}},
    {CURVE_FOFBAMP_NAME         , 0, {0}, {0}},
    {CURVE_FOFBPOS_NAME         , 0, {0}, {0}}
};

/* Print data composed of 16-bit signed data */
int print_curve_16 (uint8_t *curve_data, uint32_t len)
{
    unsigned int i;
    for (i = 0; i < len/2; ++i) {
        printf ("%d\n", *((int16_t *)((uint8_t *)curve_data + i*2)));
    }

    return 0;
}

/* Print data composed of 32-bit signed data */
int print_curve_32 (uint8_t *curve_data, uint32_t len)
{
    unsigned int i;
    for (i = 0; i < len/4; ++i) {
        printf ("%d\n", *((int32_t *)((uint8_t *)curve_data + i*4)));
    }

    return 0;
}

int main(int argc, char *argv[])
{
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    int yes = 1;

    int verbose = 0;
    int ch;

    // Acquitision parameters check
    int acq_samples_set = 0;
    uint32_t acq_samples_val = 0;
    int acq_chan_set = 0;
    uint32_t acq_chan_val = 0;
    uint32_t acq_curve_chan = 0;

    program_name = argv[0];

    // loop over all of the options
    while ((ch = getopt_long(argc, argv, "hvbro:x:y:s:jkd:p:q:i:l:c:tXYSJDPQILCB:EF", long_options, NULL)) != -1)
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
              // Set Hostname
              case 'o':
                  hostname = strdup(optarg);
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
              // Set Switching On
              case 'j':
                  call_func[SET_SW_ON_ID].call = 1;
                  break;
               // Set Switching Off
              case 'k':
                  call_func[SET_SW_OFF_ID].call = 1;
                  break;
               // Set DIVCLK
              case 'd':
                  call_func[SET_SW_DIVCLK_ID].call = 1;
                  *((uint32_t *)call_func[SET_SW_DIVCLK_ID].param_in) = (uint32_t) atoi(optarg);
                  break;
               // Set PHASECLK
              case 'p':
                  call_func[SET_SW_PHASECLK_ID].call = 1;
                  *((uint32_t *)call_func[SET_SW_PHASECLK_ID].param_in) = (uint32_t) atoi(optarg);
                  break;
               // Set ADCCLK
              case 'q':
                  call_func[SET_ADCCLK_ID].call = 1;
                  *((uint32_t *)call_func[SET_ADCCLK_ID].param_in) = (uint32_t) atoi(optarg);
                  break;
               // Set DDSFREQ
              case 'i':
                  call_func[SET_DDSFREQ_ID].call = 1;
                  *((uint32_t *)call_func[SET_DDSFREQ_ID].param_in) = (uint32_t) atoi(optarg);
                  break;
               // Set Acq Samples
              case 'l':
                  //call_func[SET_ACQ_SAMPLES_ID].call = 1;
                  //*((uint32_t *)call_func[SET_ACQ_SAMPLES_ID].param_in) = (uint32_t) atoi(optarg);
                  acq_samples_set = 1;
                  acq_samples_val = (uint32_t) atoi(optarg);
                  break;
               // Set Acq Chan
              case 'c':
                  //call_func[SET_ACQ_CHAN_ID].call = 1;
                  //*((uint32_t *)call_func[SET_ACQ_CHAN_ID].param_in) = (uint32_t) atoi(optarg);
                  acq_chan_set = 1;
                  acq_chan_val = (uint32_t) atoi(optarg);
                  break;
               // Set Acq Start
              case 't':
                  call_func[SET_ACQ_START_ID].call = 1;
                  break;
               // Get Kx
              case 'X':
                  call_func[GET_KX_ID].call = 1;
                  break;
               // Get Ky
              case 'Y':
                  call_func[GET_KY_ID].call = 1;
                  break;
              // Get Ksum
              case 'S':
                  call_func[GET_KSUM_ID].call = 1;
                  break;
              // Get Switching Off
              case 'J':
                  call_func[GET_SW_ID].call = 1;
                  break;
              // Get DIVCLK
              case 'D':
                  call_func[GET_SW_DIVCLK_ID].call = 1;
                  break;
              // Get PHASECLK
              case 'P':
                  call_func[GET_SW_PHASECLK_ID].call = 1;
                  break;
              // Get ADCCLK
              case 'Q':
                  call_func[GET_ADCCLK_ID].call = 1;
                  break;
              // Get DDSFREQ
              case 'I':
                  call_func[GET_DDSFREQ_ID].call = 1;
                  break;
              // Get Acq Samples
              case 'L':
                  call_func[GET_ACQ_SAMPLES_ID].call = 1;
                  break;
              // Get Acq Chan
              case 'C':
                  call_func[GET_ACQ_CHAN_ID].call = 1;
                  break;
              // Get Curve
              case 'B':
                  call_curve_type[ANY_CURVE_TYPE_ID].call = 1;
                  acq_curve_chan = (uint32_t) atoi(optarg);
                  /**((uint32_t *)call_curve[GET_CURVE_ID].param_in) = (uint32_t) atoi(optarg);*/
                  break;
              // Get Monit. Amp
              case 'E':
                  call_curve_monit[GET_MONIT_AMP_ID].call = 1;
                  break;
              case 'F':
                  call_curve_monit[GET_MONIT_POS_ID].call = 1;
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

    // Both Acq Chan and Acq Samples must be set or none of them
    if ((acq_samples_set && !acq_chan_set) || (!acq_samples_set && acq_chan_set)) {
        fprintf(stderr, "%s: If --setsamples or --setchan is set the other must be too!\n", program_name);
        return -1;
    }

    // If we are here, we are good with the acquisition parameters
    if (acq_samples_set && acq_chan_set) {
        call_func[SET_ACQ_PARAM_ID].call = 1;
        *((uint32_t *)call_func[SET_ACQ_PARAM_ID].param_in) = acq_samples_val;
        *((uint32_t *)call_func[SET_ACQ_PARAM_ID].param_in + 1) = acq_chan_val;
    }

    // Check for acq_curve_chan bounds
    if (call_curve_type[ANY_CURVE_TYPE_ID].call) {
        if (acq_curve_chan > END_CURVE_ID-1) {//0 -> adc, tbtamp, tbtpos, fofbamp, 4-> fofbpos
            fprintf(stderr, "%s: Specified curve ID invalid. It must be between %d and %d!\n", program_name, CURVE_ADC_ID, END_CURVE_ID-1);
            return -1;
        }

        call_curve[acq_curve_chan].call = 1;
    }

    // Socket specific part
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
    bsmp_client_t *client = bsmp_client_new(bpm_send, bpm_recv);

    if(!client)
    {
        fprintf(stderr, "Error allocating BSMP instance\n");
        goto exit_close;
    }

    // Initialize the client instance (communication must be already working)
    enum bsmp_err err;
    if((err = bsmp_client_init(client)))
    {
        fprintf(stderr, "bsmp_client_init: %s\n", bsmp_error_str(err));
        goto exit_destroy;
    }

    struct bsmp_func_info_list *funcs;
    TRY("funcs_list", bsmp_get_funcs_list(client, &funcs));

    // Get list of functions
    printf("\n"C"Server has %d Functions(s):\n", funcs->count);
    unsigned int i;
    for(i = 0; i < funcs->count; ++i) {
        printf(C" ID[%d] INPUT[%2d bytes] OUTPUT[%2d bytes]\n",
                funcs->list[i].id,
                funcs->list[i].input_size,
                funcs->list[i].output_size);
    }

    // Get list of curves
    struct bsmp_curve_info_list *curves;
    TRY("curves_list", bsmp_get_curves_list(client, &curves));

    printf("\n"C"Server has %d Curve(s):\n", curves->count);
    for(i = 0; i < curves->count; ++i)
        printf(C" ID[%d] BLOCKS[%3d (%5d bytes each)] %s\n",
                curves->list[i].id,
                curves->list[i].nblocks,
                curves->list[i].block_size,
                curves->list[i].writable ? "WRITABLE" : "READ-ONLY");

    // Call all the functions the user specified with its parameters
    struct bsmp_func_info *func;
    uint8_t func_error;

    for (i = 0; i < ARRAY_SIZE(call_func); ++i) {
        if (call_func[i].call) {
            func = &funcs->list[i];
            TRY((call_func[i].name), bsmp_func_execute(client, func,
                                            &func_error, call_func[i].param_in, call_func[i].param_out));
        }
    }

    // Show all results
    for (i = 0; i < ARRAY_SIZE(call_func); ++i) {
        if (call_func[i].call /*&& *((uint32_t *)call_func[i].param_out) != 0*/) {
            printf ("%s: %d\n", call_func[i].name, *((uint32_t *)call_func[i].param_out));
        }
    }

    // Call specified curves
    struct bsmp_curve_info *curve;
    uint8_t *curve_data = NULL;
    uint32_t curve_data_len;
    for (i = 0; i < ARRAY_SIZE(call_curve); ++i) {
        if (call_curve[i].call) {
            // Requesting curve
            printf(C"Requesting curve #%d\n", i);

            curve = &curves->list[i];
            curve_data = malloc(curve->block_size*curve->nblocks);
            /* POtential failure can happen here if large buffer is requested!! */
            TRY("malloc curve data", !curve_data);
            TRY((call_curve[i].name), bsmp_read_curve(client, curve,
                                            curve_data, &curve_data_len));

            printf(C" Got %d bytes of curve\n", curve_data_len);
            if (i == CURVE_ADC_ID)
                print_curve_16 (curve_data, curve_data_len);
            else
                print_curve_32 (curve_data, curve_data_len);

        }
    }

    free (curve_data);

    unsigned int j = 0;
    // poll to infinity the Monit. Functions if called
    for (i = 0; i < ARRAY_SIZE(call_curve_monit); ++i) {
        if (call_curve_monit[i].call) {
            printf(C"Requesting curve #%d\n", END_CURVE_ID+i);
            curve = &curves->list[END_CURVE_ID+i];// These are just after the regular functions
            curve_data = malloc(curve->block_size*curve->nblocks);
            for(j = 0; j < 4096; ++j) { // ????? FIXME
                TRY((call_curve_monit[i].name), bsmp_read_curve(client, curve,
                                            curve_data, &curve_data_len));
                print_curve_32 (curve_data, curve_data_len);
            }
        }
    }

    close(sockfd);

exit_destroy:
    free (curve_data);
    bsmp_client_destroy(client);
    puts("BSMP deallocated");
exit_close:
    close(sockfd);
    puts("Socket closed");
    free (hostname);
    return 0;
}
