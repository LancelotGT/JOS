#include <inc/lib.h>
#include <inc/error.h>
#include <inc/rpc.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>

// Implement remote procedure call across network.
// The PRC is built on top of network driver and its TCP/IP
// protocol stack. It marshalls user input arguments as
// network packets and using sockets to transfer packets and
// get results back. Currently only supports TCP/IP.

#define BUFFSIZE 32
static int sock = -1;
static struct sockaddr_in rpcserver;
static int args_num;
static int arg_types[RPCMAXARGS];

// Copy a argument into the rpc packet
static int arg_copy(char* data, int a, int type)
{
    int len = 0;
    switch(type) {
    case NTYPE:
        return 0;
    case INT:
        *(int*)data = a;
        len = sizeof(int);
        break;
    case CHAR:
        *data = a;
        len = sizeof(char);
        break;
    case STRING:
        len = strlen((char*)a);
        strcpy(data, (char*)a);
        break;
    default:
        cprintf("Unsupported data types");
        return -1;
    }
    return len;
}

// This function init a RPC session based on
// RPC server ipaddress and port
int rpc_client_init(char* ipaddr, uint16_t port)
{
    int i;

    for (i = 0; i < RPCMAXARGS; i++)
        arg_types[i] = NTYPE;
    args_num = 0;

    cprintf("Start RPC client session. Connecting to:\n");
    cprintf("\tip address %s = %x\n", ipaddr, inet_addr(ipaddr));

    // Construct the server sockaddr_in structure
    memset(&rpcserver, 0, sizeof(rpcserver));             // Clear struct
    rpcserver.sin_family = AF_INET;                        // Internet/IP
    rpcserver.sin_addr.s_addr = inet_addr(ipaddr);         // IP address
    rpcserver.sin_port = htons(port);                      // server port

    return 0;
}

// This function is used by user to indicate the data types used
// in their rpc call. By default, no input arguments will be used.
int rpc_client_args(int t0, int t1, int t2, int t3, int t4, int t5)
{
    if (t1 >= INT && t1 <= NTYPE &&
        t2 >= INT && t2 <= NTYPE &&
        t3 >= INT && t3 <= NTYPE &&
        t4 >= INT && t4 <= NTYPE &&
        t5 >= INT && t5 <= NTYPE &&
        t0 >= INT && t0 <= NTYPE) {
        arg_types[0] = t0;
        arg_types[1] = t1;
        arg_types[2] = t2;
        arg_types[3] = t3;
        arg_types[4] = t4;
        arg_types[5] = t5;
    } else
        return E_INVAL;

    // calculate the number of args used
    if (t0 != NTYPE) args_num++;
    if (t1 != NTYPE) args_num++;
    if (t2 != NTYPE) args_num++;
    if (t3 != NTYPE) args_num++;
    if (t4 != NTYPE) args_num++;
    if (t5 != NTYPE) args_num++;

    return 0;
}

// This is the actuall rpc function. The response is limited to a string.
int rpc_client(void* res, int a0, int a1, int a2, int a3, int a4, int a5)
{
    int i, len;
    struct rpc_pkt pkt;
    char buffer[BUFFSIZE];
    unsigned int received = 0;

    // Create the TCP socket
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        return -E_INVAL;

    // Establish connection
    if (connect(sock, (struct sockaddr *)&rpcserver, sizeof(rpcserver)) < 0)
        return -E_NO_CONN;

    // formulate the rpc packet
    memset(&pkt, 0, sizeof(pkt));
    pkt.num = args_num; // limit number of args to 2 for now
    len = 0;
    for (i = 0; i < RPCMAXARGS; i++)
        pkt.types[i] = arg_types[i];

    len += arg_copy(pkt.data + len, a0, pkt.types[0]);
    len += arg_copy(pkt.data + len, a1, pkt.types[1]);
    len += arg_copy(pkt.data + len, a2, pkt.types[2]);
    len += arg_copy(pkt.data + len, a3, pkt.types[3]);
    len += arg_copy(pkt.data + len, a4, pkt.types[4]);
    len += arg_copy(pkt.data + len, a5, pkt.types[5]);

    // write 1024 bytes should be ok since max packet
    // length is 1518
    if (write(sock, &pkt, PKTLEN) != PKTLEN)
        return -E_BAD_REQ;

    // Receive the result back from the server
    if ((received = read(sock, buffer, BUFFSIZE)) < 1)
        return -E_BAD_REQ;
    buffer[received] = '\0';
    memcpy(res, buffer, BUFFSIZE);

    close(sock);
    sock = -1;
    return 0;
}
