// Simple FCS Client interfacing with the BSMP library
// for controlling the BPM FPGA
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
#include <signal.h>

#include "fcs_client.h"
#include "debug.h"

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

#define PORT "8080" // the FPGA port client will be connecting to
#define FE_PORT "6791" // the RFFE port client will be connecting to
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define MONIT_POLL_RATE 10000 //usec

const char* program_name;
char *hostname = NULL;
int need_hostname = 0;
char *fe_hostname = NULL;
int need_fe_hostname = 0;

sig_atomic_t _interrupted = 0;

// C^c signal handler
static void sigint_handler (int sig, siginfo_t *siginfo, void *context)
{
    (void) sig;
    (void) siginfo;
    (void) context;
    _interrupted = 1;
}

#define PLOT_BUFFER_LEN 1024 // in 32-bit words
#define NUM_CHANNELS 4
typedef struct _plot_values_monit_double_t {
    double ch0[PLOT_BUFFER_LEN];
    double ch1[PLOT_BUFFER_LEN];
    double ch2[PLOT_BUFFER_LEN];
    double ch3[PLOT_BUFFER_LEN];
} plot_values_monit_double_t;

typedef struct _plot_values_monit_uint32_t {
    uint32_t ch0;
    uint32_t ch1;
    uint32_t ch2;
    uint32_t ch3;
} plot_values_monit_uint32_t;

/* 4096 data of A, B, C or D */
plot_values_monit_uint32_t pval_monit_uint32[PLOT_BUFFER_LEN];
plot_values_monit_double_t pval_monit_double;

/* Our FPGA socket */
int sockfd;
/* Our FE socket */
int fe_sockfd;

/* Our send/receive packet for the FPGA */
recv_pkt_t recv_pkt;
send_pkt_t send_pkt;
/* Our send/receive packet for the FE */
recv_pkt_t fe_recv_pkt;
send_pkt_t fe_send_pkt;

/***************************************************************/
/********************** Utility functions **********************/
/***************************************************************/

int __sendall(int fd, uint8_t *buf, uint32_t *len)
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

int __recvall(int fd, uint8_t *buf, uint32_t *len)
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

/***************************************************************/
/**********************      Functions       *******************/
/***************************************************************/

void print_packet (char* pre, uint8_t *data, uint32_t size)
{
#ifdef DEBUG
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
#else
    (void) pre;
    (void) data;
    (void) size;
#endif
}

int __bpm_send(int fd, uint8_t *data, uint32_t *count)
{
    uint8_t  packet[BSMP_MAX_MESSAGE];
    uint32_t packet_size = *count;
    uint32_t len = *count;

    memcpy (packet, data, *count);

    print_packet("SEND()", packet, packet_size);

    int ret = __sendall(fd, packet, &len);
    DEBUGP ("bpm_send(%d): %d bytes sent!\n", fd, len);

    if(len != packet_size) {
        if(ret < 0)
            perror("send");
        return -1;
    }

    return 0;
}

int __bpm_recv(int fd, uint8_t *data, uint32_t *count)
{
    uint8_t packet[PACKET_SIZE] = {0};
    uint32_t packet_size;
    uint32_t len = PACKET_HEADER;

    int ret = __recvall(fd, packet, &len);
    if(len != PACKET_HEADER) {
        if(ret < 0)
            perror("recv");
        return -1;
    }

    DEBUGP ("bpm_recv(%d): received %d bytes (header)!\n", fd, PACKET_HEADER);

    //uint32_t remaining = (packet[2] << 8) + packet[3];
    uint32_t remaining = (packet[1] << 8) + packet[2];
    len = remaining;

    DEBUGP ("bpm_recv(%d): %d bytes to recv!\n", fd, remaining);

    ret = __recvall(fd, packet + PACKET_HEADER, &len);
    if(len != remaining) {
        if(ret < 0)
            perror("recv");
        return -1;
    }

    DEBUGP("bpm_recv(%d) received payload!\n", fd);

    packet_size = PACKET_HEADER + remaining;

    print_packet("RECV", packet, packet_size);

    *count = packet_size;
    memcpy(data, packet, *count);

    return 0;
}

/***************************************************************/
/**********************      Wrappers       *******************/
/***************************************************************/

int bpm_fpga_send(uint8_t *data, uint32_t *count)
{
    return __bpm_send(sockfd, data, count); // sockfd is the FPGA socket
}

int bpm_fpga_recv(uint8_t *data, uint32_t *count)
{
    return __bpm_recv(sockfd, data, count); // sockfd is the FPGA socket
}

int bpm_fe_send(uint8_t *data, uint32_t *count)
{
    return __bpm_send(fe_sockfd, data, count); // fe_sockfd is the FPGA socket
}

int bpm_fe_recv(uint8_t *data, uint32_t *count)
{
    return __bpm_recv(fe_sockfd, data, count); // fe_sockfd is the FPGA socket
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
            "  -o  --setfpgahostname  <host>   Sets FPGA hostname to <host>\n"
            "  -w  --setrffehostname  <host>   Sets RFFE hostname to <host>\n"
            "  -x  --setkx        <value>[nm]  Sets parameter Kx to <value>\n"
            "                                    [in UFIX25_0 format]\n"
            "  -y  --setky        <value>[nm]  Sets parameter Ky to <value>\n"
            "                                   [in UFIX25_0 format]\n"
            "  -s  --setksum      <value>      Sets parameter Ksum to <value>\n"
            "                                   [in FIX25_24 format]\n"
            "  -j  --setswon                   Sets FPGA deswitching on\n"
            "  -k  --setswoff                  Sets FPGA deswitching off\n"
            "  -g  --setfeswon                 Sets RFFE switching on\n"
            "  -m  --setfeswoff                Sets RFFE switching off\n"
            "  -d  --setdivclk    <value>      Sets FPGA switching divider clock to <value>\n"
            "                                    [in number of ADC clock cycles]\n"
            "  -p  --setphaseclk  <value>      Sets FPGA switching phase clock to <value>\n"
            "                                    [in number of ADC clock cycles]\n"
            "  -u  --setwdwon                  Sets FPGA windowing on\n"
            "  -e  --setwdwoff                 Sets FPGA windowing off\n"
            "  -n  --setwdwdly    <value>      Sets FPGA windowing delay\n"
            "                                  [<value> must be between 0 and 500 [ADC clk cycles]]\n"
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
            "  -a  --setfeatt1    <value>      Sets the RFFE Attenuator 1 to <value>\n"
            "                                  [<value> must be between 0 and 31.5 [dB]\n"
            "                                   with 0.5 step. Invalid attenuation values\n"
            "                                   will be rounded down to the nearest valid\n"
            "                                   value]\n"
            "  -z  --setfeatt2    <value>      Sets the RFFE Attenuator 2 to <value>\n"
            "                                  [<value> must be between 0 and 31.5 [dB]\n"
            "                                   with 0.5 step. Invalid attenuation values\n"
            "                                   will be rounded down to the nearest valid\n"
            "                                   value]\n"
            "  -X  --getkx                     Gets parameter Kx [nm] in UFIX25_0 format\n"
            "  -Y  --getky                     Gets parameter Ky [nm] in UFIX25_0 format\n"
            "  -S  --getksum                   Gets parameter Ksum in FIX25_24 format\n"
            "  -J  --getsw                     Gets FPGA deswitching state \n"
            "                                    [0x1 is no switching and 0x3 is switching]\n"
            "  -G  --getfesw                   Gets RFFE switching state \n"
            "                                    [0x1 is no switching and 0x3 is switching]\n"
            "  -D  --getdivclk                 Gets FPGA switching divider clock value\n"
            "                                    [in number of ADC clock cycles]\n"
            "  -P  --getphaseclk               Gets FPGA switching phase clock\n"
            "                                    [in number of ADC clock cycles]\n"
            "  -E  --getwdw                    Gets FPGA windowing state \n"
            "  -N  --getwdwdly                 Gets FPGA windowing delay\n"
            "                                  [<value> will be between 0 and 500 [ADC clk cycles]]\n"
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
    {"setfpgahostname", required_argument,   NULL, 'o'},
    {"setrffehostname", required_argument,   NULL, 'w'},
    {"setkx",           required_argument,   NULL, 'x'},
    {"setky",           required_argument,   NULL, 'y'},
    {"setksum",         required_argument,   NULL, 's'},
    {"setswon",         no_argument,         NULL, 'j'},
    {"setswoff",        no_argument,         NULL, 'k'},
    {"setfeswon",       no_argument,         NULL, 'g'},
    {"setfeswoff",      no_argument,         NULL, 'm'},
    {"setdivclk",       required_argument,   NULL, 'd'},
    {"setphaseclk",     required_argument,   NULL, 'p'},
    {"setwdwon",        no_argument,         NULL, 'u'},
    {"setwdwoff",       no_argument,         NULL, 'e'},
    {"setwdwdly",       required_argument,   NULL, 'n'},
    {"setadcclk",       required_argument,   NULL, 'q'},
    {"setddsfreq",      required_argument,   NULL, 'i'},
    {"setsamples",      required_argument,   NULL, 'l'},
    {"setchan",         required_argument,   NULL, 'c'},
    {"startacq",        no_argument,         NULL, 't'},
    {"setfeatt1",       required_argument,   NULL, 'a'},
    {"setfeatt2",       required_argument,   NULL, 'z'},
    {"getkx",           no_argument,         NULL, 'X'},
    {"getky",           no_argument,         NULL, 'Y'},
    {"getksum",         no_argument,         NULL, 'S'},
    {"getsw",           no_argument,         NULL, 'J'},
    {"getfesw",         no_argument,         NULL, 'G'},
    {"getdivclk",       no_argument,         NULL, 'D'},
    {"getphaseclk",     no_argument,         NULL, 'P'},
    {"getwdw",          no_argument,         NULL, 'U'},
    {"getwdwdly",       no_argument,         NULL, 'N'},
    {"getadcclk",       no_argument,         NULL, 'Q'},
    {"getddsfreq",      no_argument,         NULL, 'I'},
    {"getsamples",      no_argument,         NULL, 'L'},
    {"getchan",         no_argument,         NULL, 'C'},
    {"getfeatt1",       no_argument,         NULL, 'A'},
    {"getfeatt2",       no_argument,         NULL, 'Z'},
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

/***************************************************/
/*************** General Functions *****************/
/***************************************************/

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
#define SET_WDW_ON_ID           15
#define SET_WDW_ON_NAME         "set_wdw_on"
#define SET_WDW_OFF_ID          16
#define SET_WDW_OFF_NAME        "set_wdw_off"
#define GET_WDW_ID              17
#define GET_WDW_NAME            "get_wdw"
#define SET_WDW_DLY_ID          18
#define SET_WDW_DLY_NAME        "set_wdw_dly"
#define GET_WDW_DLY_ID          19
#define GET_WDW_DLY_NAME        "get_wdw_dly"
#define SET_ADCCLK_ID           20
#define SET_ADCCLK_NAME         "set_adc_clk"
#define GET_ADCCLK_ID           21
#define GET_ADCCLK_NAME         "get_adc_clk"
#define SET_DDSFREQ_ID          22
#define SET_DDSFREQ_NAME        "set_dds_freq"
#define GET_DDSFREQ_ID          23
#define GET_DDSFREQ_NAME        "get_dds_freq"
#define SET_ACQ_PARAM_ID        24
#define SET_ACQ_PARAM_NAME      "set_acq_param"
#define GET_ACQ_SAMPLES_ID      25
#define GET_ACQ_SAMPLES_NAME    "get_acq_samples"
#define GET_ACQ_CHAN_ID         26
#define GET_ACQ_CHAN_NAME       "get_acq_chan"
#define SET_ACQ_START_ID        27
#define SET_ACQ_START_NAME      "set_acq_start"
#define END_ID                  28

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
    {SET_WDW_ON_NAME            , 0, {0}, {0}},
    {SET_WDW_OFF_NAME           , 0, {0}, {0}},
    {GET_WDW_NAME               , 0, {0}, {0}},
    {SET_WDW_DLY_NAME           , 0, {0}, {0}},
    {GET_WDW_DLY_NAME           , 0, {0}, {0}},
    {SET_ADCCLK_NAME            , 0, {0}, {0}},
    {GET_ADCCLK_NAME            , 0, {0}, {0}},
    {SET_DDSFREQ_NAME           , 0, {0}, {0}},
    {GET_DDSFREQ_NAME           , 0, {0}, {0}},
    {SET_ACQ_PARAM_NAME         , 0, {0}, {0}},
    {GET_ACQ_SAMPLES_NAME       , 0, {0}, {0}},
    {GET_ACQ_CHAN_NAME          , 0, {0}, {0}},
    {SET_ACQ_START_NAME         , 0, {0}, {0}}
};

/***************************************************/
/*************** Streaming CURVES ******************/
/***************************************************/

#define CURVE_MONIT_AMP_ID      0
#define CURVE_MONIT_AMP_NAME    "monit_amp"
#define CURVE_MONIT_POS_ID      1
#define CURVE_MONIT_POS_NAME    "monit_pos"
#define END_MONIT_ID            2

static struct call_func_t call_curve_monit[END_MONIT_ID] =
{
    {CURVE_MONIT_AMP_NAME       , 0, {0}, {0}},
    {CURVE_MONIT_POS_NAME       , 0, {0}, {0}}
};

typedef struct _str_idx_t {
    char *str_idx[4]; // four strings of four chars
} str_idx_t;

static str_idx_t monit_str_idx[END_MONIT_ID] = {
    {{"MONIT_AMP_A", "MONIT_AMP_B", "MONIT_AMP_C", "MONIT_AMP_D"}},
    {{"MONIT_POS_X", "MONIT_POS_Y", "MONIT_POS_Q", "MONIT_POS_SUM"}}
};

#define ANY_CURVE_TYPE_ID        0
#define ANY_CURVE_TYPE_NAME      "any_type_curve"
#define END_CURVE_TYPE_ID        1

static struct call_func_t call_curve_type[END_CURVE_TYPE_ID] = {
    {ANY_CURVE_TYPE_NAME        , 0, {0}, {0}}
};

/***************************************************/
/*************** On-demand CURVES ******************/
/***************************************************/
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

/***************************************************/
/*************** RFFE Functions * ******************/
/***************************************************/
enum var_type {
    UINT8_T = 0,
    UINT16_T,
    UINT32_T,
    UINT64_T,
    FLOAT_T,
    DOUBLE_T
};

struct call_var_t {
    const char *name;
    int call;
    int rw;                               // 1 is read and 0 is write
    enum var_type type;
    uint8_t write_val[sizeof(uint32_t)*2]; // 2 32-bits variables
    uint8_t read_val[sizeof(uint32_t)*2]; // 2 32-bits variable
};

#define SET_FE_SW_ON_ID         0
#define SET_FE_SW_ON_NAME       "getset_fe_sw"
#define SET_FE_SW_OFF_ID        (SET_FE_SW_ON_ID) // They are the same ID in FE server
#define SET_FE_SW_OFF_NAME      SET_FE_SW_ON_NAME
#define GET_FE_SW_ID            (SET_FE_SW_ON_ID) // The are the same ID in FE server
#define GET_FE_SW_NAME          SET_FE_SW_ON_NAME
//#define GETSET_FE_SW_LVL_ID     1
//#define GETSET_FE_SW_LVL_NAME   "getset_sw_lvl"
#define GETSET_FE_ATT1_ID       1
#define GETSET_FE_ATT1_NAME     "getset_fe_att1"
#define GETSET_FE_ATT2_ID       2
#define GETSET_FE_ATT2_NAME     "getset_fe_att2"
#define END_FE_ID               3

static struct call_var_t call_fe_var[END_FE_ID] = {
    {SET_FE_SW_ON_NAME          , 0, 0, UINT8_T, {0}, {0}}, // The set "sw off" and "get sw"
    // are on the same ID in FE server
    //{GETSET_FE_SW_LVL_NAME      , 0, 0, UINT8_T, {0}, {0}}, // The set "sw lvl" and get "sw lvl"
    //                                             // are on the same ID in FE server
    {GETSET_FE_ATT1_NAME        , 0, 0, DOUBLE_T, {0}, {0}}, // The set "att1" and get "att1"
    // are on the same ID in FE server
    {GETSET_FE_ATT2_NAME        , 0, 0, DOUBLE_T, {0}, {0}} // The set "att2" and get "att2"
    // are on the same ID in FE server
};

// Some FE variable values
#define FE_SW_OFF               0x1
#define FE_SW_ON                0x3
// Some FE corection factors
#define FE_SW_DIV_FACTOR        2

/***************************************************/
/************ Client Utility Functions *************/
/***************************************************/

#define NUM_CHANNELS 4
#define SIZE_16_BYTES sizeof(uint16_t)
#define SIZE_32_BYTES sizeof(uint32_t)

/* Print data composed of 16-bit signed data */
int print_curve_16 (uint8_t *curve_data, uint32_t len)
{
    unsigned int i;
    for (i = 0; i < len/(SIZE_16_BYTES*NUM_CHANNELS); ++i) {
        printf ("%d %d %d %d\n\r",
                *((int16_t *)curve_data + i*NUM_CHANNELS),
                *((int16_t *)curve_data + i*NUM_CHANNELS+1),
                *((int16_t *)curve_data + i*NUM_CHANNELS+2),
                *((int16_t *)curve_data + i*NUM_CHANNELS+3));
    }

    return 0;
}

/* Print data composed of 32-bit signed data */
int print_curve_32 (uint8_t *curve_data, uint32_t len)
{
    unsigned int i;
    //for (i = 0; i < len/4; ++i) {
    //    printf ("%d\n", *((int32_t *)((uint8_t *)curve_data + i*4)));
    //}
    for (i = 0; i < len/(SIZE_32_BYTES*NUM_CHANNELS); ++i) {
        printf ("%d %d %d %d\n\r",
                *((int32_t *)curve_data + i*NUM_CHANNELS),
                *((int32_t *)curve_data + i*NUM_CHANNELS+1),
                *((int32_t *)curve_data + i*NUM_CHANNELS+2),
                *((int32_t *)curve_data + i*NUM_CHANNELS+3));
    }

    return 0;
}

int read_bsmp_val(struct call_var_t *fe_var)
{
    // Find out with type of varible this is and printf
    // the correct string specifier
    switch (fe_var->type) {
        case UINT8_T:
            printf ("%s: %" PRIu8 "\n", fe_var->name, *((uint8_t *)fe_var->read_val));
            break;

        case UINT16_T:
            printf ("%s: %" PRIu16 "\n", fe_var->name, *((uint16_t *)fe_var->read_val));
            break;

        case UINT32_T:
            printf ("%s: %" PRIu32 "\n", fe_var->name, *((uint32_t *)fe_var->read_val));
            break;

        case UINT64_T:
            printf ("%s: %" PRIu64 "\n", fe_var->name, *((uint64_t *)fe_var->read_val));
            break;

        case FLOAT_T:
            printf ("%s: %f\n", fe_var->name, *((float *)fe_var->read_val));
            break;

        case DOUBLE_T:
            printf ("%s: %f\n", fe_var->name, *((double *)fe_var->read_val));
            break;

        default:
            printf ("%s: %" PRIu8 "\n", fe_var->name, *((uint8_t *)fe_var->read_val));
    }

    return 0;
}

/***************************************************/
/************ Socket-specific Functions *************/
/***************************************************/
int bpm_connection(char *hostname, char* port)
{
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    int yes = 1;
    int fd;

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
        if ((fd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        /* This is important for correct behaviour */
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes,
                    sizeof(int)) == -1) {
            perror("setsockopt");
            return -2;
            //exit(1);
        }

        if (connect(fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(fd);
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

    return fd;
}

int main(int argc, char *argv[])
{
    //struct addrinfo hints, *servinfo, *p;
    //int rv;
    //char s[INET6_ADDRSTRLEN];
    //int yes = 1;

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
    while ((ch = getopt_long(argc, argv, "hvbro:w:x:y:s:jkd:p:uen:q:i:l:c:gmta:z:XYSJDPUQILCB:ENFG",
                    long_options, NULL)) != -1)
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
                need_hostname = 1;
                break;
                // Reset to default
            case 'r':
                call_func[RESET_FUNC_ID].call = 1;
                need_hostname = 1;
                break;
                // Set FPGA Hostname
            case 'o':
                hostname = strdup(optarg);
                break;
                // Set RFFE Hostname
            case 'w':
                fe_hostname = strdup(optarg);
                break;
                // Set KX
            case 'x':
                call_func[SET_KX_ID].call = 1;
                *((uint32_t *)call_func[SET_KX_ID].param_in) = (uint32_t) atoi(optarg);
                need_hostname = 1;
                break;
                // Set KY
            case 'y':
                call_func[SET_KY_ID].call = 1;
                *((uint32_t *)call_func[SET_KY_ID].param_in) = (uint32_t) atoi(optarg);
                need_hostname = 1;
                break;
                // Set Ksum
            case 's':
                call_func[SET_KSUM_ID].call = 1;
                *((uint32_t *)call_func[SET_KSUM_ID].param_in) = (uint32_t) atoi(optarg);
                need_hostname = 1;
                break;
                // Set FPGA Deswitching On
            case 'j':
                call_func[SET_SW_ON_ID].call = 1;
                need_hostname = 1;
                break;
                // Set FPGA Deswitching Off
            case 'k':
                call_func[SET_SW_OFF_ID].call = 1;
                need_hostname = 1;
                break;
                // Set FE Switching On
            case 'g':
                call_fe_var[SET_FE_SW_ON_ID].call = 1;
                call_fe_var[SET_FE_SW_ON_ID].rw = 0; // write to variable
                *call_fe_var[SET_FE_SW_ON_ID].write_val = (uint8_t) FE_SW_ON;
                need_fe_hostname = 1;
                break;
                // Set FE Switching Off
            case 'm':
                call_fe_var[SET_FE_SW_ON_ID].call = 1;
                call_fe_var[SET_FE_SW_ON_ID].rw = 0;
                //    *call_fe_var[SET_FE_SW_ON_ID].write_val = (uint8_t) FE_SW_OFF;
                *call_fe_var[SET_FE_SW_ON_ID].write_val = (uint8_t) 0x1;
                need_fe_hostname = 1;
                break;
                // Set DIVCLK
                // FIXME: This command is correctly implemented in the FPGA
                // firmware, but on the RFFE controller we need 2x the clock
                // in order to drive the switching clock, because it clocks a
                // JK-FF in toggle mode.
                //
                // So, we just divide the clk div here by 2
            case 'd':
                call_func[SET_SW_DIVCLK_ID].call = 1;
                //*((uint32_t *)call_func[SET_SW_DIVCLK_ID].param_in) = (uint32_t) (atoi(optarg)/FE_SW_DIV_FACTOR);
                *((uint32_t *)call_func[SET_SW_DIVCLK_ID].param_in) = (uint32_t) (atoi(optarg));
                need_hostname = 1;
                break;
                // Set PHASECLK
            case 'p':
                call_func[SET_SW_PHASECLK_ID].call = 1;
                *((uint32_t *)call_func[SET_SW_PHASECLK_ID].param_in) = (uint32_t) atoi(optarg);
                need_hostname = 1;
                break;
                // Set Windowing On
            case 'u':
                call_func[SET_WDW_ON_ID].call = 1;
                need_hostname = 1;
                break;
                // Set Windowing Off
            case 'e':
                call_func[SET_WDW_OFF_ID].call = 1;
                need_hostname = 1;
                break;
                // Set Windowing delay
            case 'n':
                call_func[SET_WDW_DLY_ID].call = 1;
                *((uint32_t *)call_func[SET_WDW_DLY_ID].param_in) = (uint32_t) atoi(optarg);
                need_hostname = 1;
                break;
                // Set ADCCLK
            case 'q':
                call_func[SET_ADCCLK_ID].call = 1;
                *((uint32_t *)call_func[SET_ADCCLK_ID].param_in) = (uint32_t) atoi(optarg);
                need_hostname = 1;
                break;
                // Set DDSFREQ
            case 'i':
                call_func[SET_DDSFREQ_ID].call = 1;
                *((uint32_t *)call_func[SET_DDSFREQ_ID].param_in) = (uint32_t) atoi(optarg);
                need_hostname = 1;
                break;
                // Set Acq Samples
            case 'l':
                //call_func[SET_ACQ_SAMPLES_ID].call = 1;
                //*((uint32_t *)call_func[SET_ACQ_SAMPLES_ID].param_in) = (uint32_t) atoi(optarg);
                acq_samples_set = 1;
                acq_samples_val = (uint32_t) atoi(optarg);
                need_hostname = 1;
                break;
                // Set Acq Chan
            case 'c':
                //call_func[SET_ACQ_CHAN_ID].call = 1;
                //*((uint32_t *)call_func[SET_ACQ_CHAN_ID].param_in) = (uint32_t) atoi(optarg);
                acq_chan_set = 1;
                acq_chan_val = (uint32_t) atoi(optarg);
                need_hostname = 1;
                break;
                // Set Acq Start
            case 't':
                call_func[SET_ACQ_START_ID].call = 1;
                need_hostname = 1;
                break;
                // Set FE Att1
            case 'a':
                call_fe_var[GETSET_FE_ATT1_ID].call = 1;
                call_fe_var[GETSET_FE_ATT1_ID].rw = 0; // Write value to variable
                *((double *)call_fe_var[GETSET_FE_ATT1_ID].write_val) = (double) atof(optarg);
                need_fe_hostname = 1;
                break;
                // Set FE Att2
            case 'z':
                call_fe_var[GETSET_FE_ATT2_ID].call = 1;
                call_fe_var[GETSET_FE_ATT2_ID].rw = 0; // Write value to variable
                *((double *)call_fe_var[GETSET_FE_ATT2_ID].write_val) = (double) atof(optarg);
                need_fe_hostname = 1;
                break;
                // Get Kx
            case 'X':
                call_func[GET_KX_ID].call = 1;
                need_hostname = 1;
                break;
                // Get Ky
            case 'Y':
                call_func[GET_KY_ID].call = 1;
                need_hostname = 1;
                break;
                // Get Ksum
            case 'S':
                call_func[GET_KSUM_ID].call = 1;
                need_hostname = 1;
                break;
                // Get FPGA Deswitching State
            case 'J':
                call_func[GET_SW_ID].call = 1;
                need_hostname = 1;
                break;
                // Get FE Switching State
            case 'G':
                call_fe_var[SET_FE_SW_ON_ID].call = 1;
                call_fe_var[SET_FE_SW_ON_ID].rw = 1; // Read value from variable
                need_fe_hostname = 1;
                break;
                // Get DIVCLK
            case 'D':
                call_func[GET_SW_DIVCLK_ID].call = 1;
                need_hostname = 1;
                break;
                // Get PHASECLK
            case 'P':
                call_func[GET_SW_PHASECLK_ID].call = 1;
                need_hostname = 1;
                break;
                // Get Windowing delay
            case 'N':
                call_func[GET_WDW_DLY_ID].call = 1;
                need_hostname = 1;
                break;
                // Get Windopwing State
            case 'U':
                call_func[GET_WDW_ID].call = 1;
                need_hostname = 1;
                break;
                // Get ADCCLK
            case 'Q':
                call_func[GET_ADCCLK_ID].call = 1;
                need_hostname = 1;
                break;
                // Get DDSFREQ
            case 'I':
                call_func[GET_DDSFREQ_ID].call = 1;
                need_hostname = 1;
                break;
                // Get Acq Samples
            case 'L':
                call_func[GET_ACQ_SAMPLES_ID].call = 1;
                need_hostname = 1;
                break;
                // Get Acq Chan
            case 'C':
                call_func[GET_ACQ_CHAN_ID].call = 1;
                need_hostname = 1;
                break;
                // Get FE Att1
            case 'A':
                call_fe_var[GETSET_FE_ATT1_ID].call = 1;
                call_fe_var[GETSET_FE_ATT1_ID].rw = 1; // Read value from variable
                need_fe_hostname = 1;
                break;
                // Get FE Att2
            case 'Z':
                call_fe_var[GETSET_FE_ATT2_ID].call = 1;
                call_fe_var[GETSET_FE_ATT2_ID].rw = 1; // Read value from variable
                need_fe_hostname = 1;
                break;
                // Get Curve
            case 'B':
                call_curve_type[ANY_CURVE_TYPE_ID].call = 1;
                acq_curve_chan = (uint32_t) atoi(optarg);
                /**((uint32_t *)call_curve[GET_CURVE_ID].param_in) = (uint32_t) atoi(optarg);*/
                need_hostname = 1;
                break;
                // Get Monit. Amp
            case 'E':
                call_curve_monit[CURVE_MONIT_AMP_ID].call = 1;
                need_hostname = 1;
                break;
            case 'F':
                call_curve_monit[CURVE_MONIT_POS_ID].call = 1;
                need_hostname = 1;
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

    // Options checking!
    if (need_hostname && hostname == NULL) {
        fprintf(stderr, "%s: FPGA hostname not set!\n", program_name);
        print_usage(stderr, 1);
    }

    if (need_fe_hostname && fe_hostname == NULL) {
        fprintf(stderr, "%s: RFFE hostname not set!\n", program_name);
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

    // Setup sigint signal handler
    struct sigaction act;

    memset (&act, 0, sizeof(act));
    act.sa_sigaction = sigint_handler;
    act.sa_flags = SA_SIGINFO;

    if (sigaction (SIGINT, &act, NULL) != 0) {
        perror ("sigaction");
        exit (0);
    }

    // Socket specific part

    // Initilize connection to FPGA and FE
    //ugly "solution" to allow connecting to just FPGA or FE
    /***************************************************/
    /******** Init Connection and BSMP library *********/
    /***************************************************/

    bsmp_client_t *fe_client = NULL;
    enum bsmp_err err;

    if(need_fe_hostname) {
        fe_sockfd = bpm_connection(fe_hostname, FE_PORT);

        if (fe_sockfd < 0) {
            fprintf(stderr, "Error connecting to FE server\n");
            goto exit_fe_conn;
        }

        // Create a new client FE instance
        fe_client = bsmp_client_new(bpm_fe_send, bpm_fe_recv);

        if(!fe_client) {
            fprintf(stderr, "Error allocating FE BSMP instance\n");
            goto exit_fe_close;
        }

        DEBUGP ("BSMP FE created!\n");

        if((err = bsmp_client_init(fe_client))) {
            fprintf(stderr, "bsmp_client_init (FE): %s\n", bsmp_error_str(err));
            goto exit_fe_destroy;
        }

        DEBUGP ("BSMP FE initilized!\n");
    }

    bsmp_client_t *client = NULL;

    if(need_hostname){
        sockfd = bpm_connection(hostname, PORT);

        if (sockfd < 0) {
            fprintf(stderr, "Error connecting to FPGA server\n");
            goto exit_fpga_conn;
        }

        // Create a new client instance
        client = bsmp_client_new(bpm_fpga_send, bpm_fpga_recv);

        if(!client) {
            fprintf(stderr, "Error allocating FPGA BSMP instance\n");
            goto exit_fpga_close;
        }

        DEBUGP ("FPGA BSMP instance created!\n");

        // Initialize the client instance (communication must be already working)
        if((err = bsmp_client_init(client))) {
            fprintf(stderr, "bsmp_client_init (FPGA): %s\n", bsmp_error_str(err));
            goto exit_fpga_destroy;
        }

        DEBUGP ("FPGA BSMP initilized!\n");
    }

    /***************************************************/
    /***************** Get BSMP handlers ***************/
    /***************************************************/
    struct bsmp_func_info_list *fe_funcs;
    struct bsmp_var_info_list *fe_vars;
    unsigned int i;

    if(need_fe_hostname){

        TRY("funcs_fpga_list", bsmp_get_funcs_list(fe_client, &fe_funcs));

        // Get FE list of functions
        DEBUGP("\n"C"Server FE has %d Functions(s):\n", fe_funcs->count);
        for(i = 0; i < fe_funcs->count; ++i) {
            DEBUGP(C" ID[%d] INPUT[%2d bytes] OUTPUT[%2d bytes]\n",
                    fe_funcs->list[i].id,
                    fe_funcs->list[i].input_size,
                    fe_funcs->list[i].output_size);
        }

        TRY("vars_fe_list", bsmp_get_vars_list(fe_client, &fe_vars));

        // Get FE list of variables
        DEBUGP(C"Server FE has %d Variable(s):\n", fe_vars->count);
        for(i = 0; i < fe_vars->count; ++i) {
            DEBUGP(C" ID[%d] SIZE[%2d] %s\n",
                    fe_vars->list[i].id,
                    fe_vars->list[i].size,
                    fe_vars->list[i].writable ? "WRITABLE " : "READ-ONLY");
        }
    }

    struct bsmp_func_info_list *funcs;
    struct bsmp_curve_info_list *curves;

    if(need_hostname){

        TRY("funcs_fpga_list", bsmp_get_funcs_list(client, &funcs));

        // Get FPGA list of functions
        DEBUGP("\n"C"Server FPGA has %d Functions(s):\n", funcs->count);

        for(i = 0; i < funcs->count; ++i) {
            DEBUGP(C" ID[%d] INPUT[%2d bytes] OUTPUT[%2d bytes]\n",
                    funcs->list[i].id,
                    funcs->list[i].input_size,
                    funcs->list[i].output_size);
        }

        // Get FPGA list of curves
        TRY("curves_list", bsmp_get_curves_list(client, &curves));

        DEBUGP("\n"C"Server FPGA has %d Curve(s):\n", curves->count);
        for(i = 0; i < curves->count; ++i) {
            DEBUGP(C" ID[%d] BLOCKS[%3d (%5d bytes each)] %s\n",
                    curves->list[i].id,
                    curves->list[i].nblocks,
                    curves->list[i].block_size,
                    curves->list[i].writable ? "WRITABLE" : "READ-ONLY");
        }
    }

    /***************************************************/
    /**** Call BSMP variables/functions/curves *********/
    /***************************************************/
    if (need_fe_hostname) {
        // Call all the FE variables the user specified with its parameters
        DEBUGP("\n");
        struct bsmp_var_info *fe_var_name;// = &fe_vars->list[0];

        for (i = 0; i < ARRAY_SIZE(call_fe_var); ++i) {
            if (call_fe_var[i].call) {
                fe_var_name = &fe_vars->list[i];

                if (call_fe_var[i].rw) { // Read variable
                    DEBUGP ("calling %s variable for reading!\n", call_fe_var[i].name);
                    TRY(call_fe_var[i].name, bsmp_read_var(fe_client, fe_var_name, call_fe_var[i].read_val));
                }
                else { // write variable
                    //DEBUGP ("calling %s variable for writing with value 0x%x!\n", call_fe_var[i].name,
                    //        *((uint32_t *)call_fe_var[i].write_val));
                    DEBUGP ("calling %s variable for writing with value %f!\n", call_fe_var[i].name,
                            *((double *)call_fe_var[i].write_val));
                    TRY(call_fe_var[i].name, bsmp_write_var(fe_client, fe_var_name, call_fe_var[i].write_val));
                }
            }
        }

        // Show all results
        for (i = 0; i < ARRAY_SIZE(call_fe_var); ++i) {
            if (call_fe_var[i].call && call_fe_var[i].rw == 1) { // Print result
                // for read variables only
                read_bsmp_val(&call_fe_var[i]);
                //printf ("%s: %f\n", call_fe_var[i].name, *((double *)call_fe_var[i].read_val));
            }
        }
    }

    if (need_hostname) {
        struct bsmp_func_info *func;
        uint8_t func_error;

        // Call all the FPGA functions the user specified with its parameters
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
                DEBUGP(C"Requesting curve #%d\n", i);

                curve = &curves->list[i];
                curve_data = malloc(curve->block_size*curve->nblocks);
                /* Potential failure can happen here if large buffer is requested!! */
                TRY("malloc curve data", !curve_data);
                TRY((call_curve[i].name), bsmp_read_curve(client, curve,
                            curve_data, &curve_data_len));

                DEBUGP(C" Got %d bytes of curve\n", curve_data_len);
                if (i == CURVE_ADC_ID)
                    print_curve_16 (curve_data, curve_data_len);
                else
                    print_curve_32 (curve_data, curve_data_len);

            }
        }

        // Poll to infinity the Monit. Functions if called
        for (i = 0; i < ARRAY_SIZE(call_curve_monit); ++i) {
            if (call_curve_monit[i].call) {
                DEBUGP(C"Requesting curve #%d\n", END_CURVE_ID+i);
                curve = &curves->list[END_CURVE_ID+i];// These are just after the regular functions
                //curve_data = malloc(curve->block_size*curve->nblocks);
                while (!_interrupted) {
                    unsigned int j;
                    char curve_name[20];
                    for (j = 0; j < PLOT_BUFFER_LEN && !_interrupted; ++j) { // in 4 * 32-bit words
                        TRY((call_curve_monit[i].name), bsmp_read_curve(client, curve,
                                    (uint8_t *)(pval_monit_uint32 + j), &curve_data_len));

                        // Output Curve to stdout
                        printf ("%d %d %d %d\n\r", pval_monit_uint32[j].ch0,
                                pval_monit_uint32[j].ch1,
                                pval_monit_uint32[j].ch2,
                                pval_monit_uint32[j].ch3);
                        //printf ("%d\n\r", pval_monit_uint32[j].ch0);
                        fflush(stdout);

                        // Can this update rate cause problems for gnuplot?
                        usleep (MONIT_POLL_RATE); /* 10 Hz update */

                    }
                }
            }
        }
        free (curve_data);
    }

exit_fpga_destroy:
    bsmp_client_destroy(client);
    DEBUGP("BSMP FPGA deallocated\n");
exit_fpga_close:
    close (sockfd);
    DEBUGP("Socket FPGA closed\n");
exit_fpga_conn:
exit_fe_destroy:
    bsmp_client_destroy (fe_client);
    DEBUGP("BSMP FE deallocated\n");
exit_fe_close:
    close (fe_sockfd);
    DEBUGP("Socket FE closed\n");
exit_fe_conn:
    free (hostname);
    free (fe_hostname);
    return 0;
}
