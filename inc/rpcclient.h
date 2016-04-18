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

// This function init a RPC session based on
// RPC server ipaddress and port
int rpc_client_init(char* ipaddr, uint16_t port)
{
    cprintf("Start RPC client session. Connecting to:\n");
    cprintf("\tip address %s = %x\n", ipaddr, inet_addr(ipaddr));

    // Create the TCP socket
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        return -E_INVAL;

    cprintf("RPC client opened a socket for communication\n");

    // Construct the server sockaddr_in structure
    memset(&rpcserver, 0, sizeof(rpcserver));             // Clear struct
    rpcserver.sin_family = AF_INET;                        // Internet/IP
    rpcserver.sin_addr.s_addr = inet_addr(ipaddr);         // IP address
    rpcserver.sin_port = htons(port);                      // server port
    return 0;
}

// This is the actuall rpc function. The response is limited to a string.
int rpc_client(void* res, int a1, int a2)
{
    cprintf("trying to connect RPC server\n");

    // Establish connection
    if (connect(sock, (struct sockaddr *)&rpcserver, sizeof(rpcserver)) < 0)
        return -E_NO_CONN;

    cprintf("connected to RPC server\n");

    // formulate the rpc packet
    struct rpc_pkt pkt;
    char buffer[BUFFSIZE];
    unsigned int received = 0;

    pkt.num = 2; // limit number of args to 2 for now
    pkt.sizes[0] = sizeof(a1);
    pkt.sizes[1] = sizeof(a2);
    ((int*)pkt.data)[0] = a1;
    ((int*)pkt.data)[1] = a2;

    // write 1024 bytes should be ok since max packet
    // length is 1518
    if (write(sock, &pkt, PKTLEN) != PKTLEN)
        return -E_BAD_REQ;

    // Receive the result back from the server
    cprintf("RPC client received: \n");
    while (received < sizeof(buffer) - 1) {
        int bytes = 0;
        if ((bytes = read(sock, buffer, BUFFSIZE-1)) < 1)
            return -E_BAD_REQ;
        received += bytes;
        cprintf(buffer);
    }
    cprintf("\n");
    buffer[received] = '\0';
    memcpy(res, buffer, BUFFSIZE);

    close(sock);
    sock = -1;
    return 0;
}
