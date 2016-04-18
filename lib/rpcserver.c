#include <inc/lib.h>
#include <inc/error.h>
#include <inc/rpc.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>

// Implement remote procedure call server side.
// The PRC is built on top of network driver and its TCP/IP
// protocol stack. It unmarshalls user input arguments from
// network packets and invoke the actual function on behalf
// of the client. Then its send only the results back.
// Currently only supports TCP/IP.

#define BUFFSIZE 32
#define MAXPENDING 5    // Max connection requests

static int serversock = -1;

static void
die(char *m)
{
  cprintf("%s\n", m);
  exit();
}

// This function init a RPC server session
int rpc_server_init(uint16_t port)
{
    struct sockaddr_in rpcserver;

    cprintf("Start RPC server session.\n");

    // Create the TCP socket
    if ((serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        return -E_INVAL;

    cprintf("RPC server opened a socket for communication\n");

    // Construct the server sockaddr_in structure
    memset(&rpcserver, 0, sizeof(rpcserver));             // Clear struct
    rpcserver.sin_family = AF_INET;                        // Internet/IP
    rpcserver.sin_addr.s_addr = htonl(INADDR_ANY);         // IP address
    rpcserver.sin_port = htons(port);                      // server port

    cprintf("trying to bind\n");

    // Bind the server socket
    if (bind(serversock, (struct sockaddr *)&rpcserver,
                sizeof(rpcserver)) < 0) {
        die("Failed to bind the server socket");
    }
    cprintf("bound\n");

    // Listen on the server socket
    if (listen(serversock, MAXPENDING) < 0) {
        die("Failed to listen on server socket");
    }

    return 0;
}



// This is the actuall rpc function. The response is limited
void rpc_server(serve_function serve)
{
    struct sockaddr_in rpcclient;
    struct rpc_pkt pkt;
    int i, len, clientsock = -1;
    int args[MAXARGS];
    char buff[BUFFSIZE];
    int received = -1;
    int r;

    // Run until canceled
    while (1) {
        unsigned int clientlen = sizeof(rpcclient);
        // Wait for client connection
        if ((clientsock =
                    accept(serversock, (struct sockaddr *)&rpcclient,
                        &clientlen)) < 0) {
            die("Failed to accept client connection");
        }

        cprintf("Client connected: %s\n", inet_ntoa(rpcclient.sin_addr));

        // Read RPC message
        if ((received = read(clientsock, &pkt, PKTLEN)) < 0) {
            close(clientsock);
            die("Failed to receive initial bytes from client");
        }

        for (i = 0; i < pkt.num; i++)
            args[i] = ((int*)pkt.data)[i];

        cprintf("Server receive packet: ");
        for (i = 0; i < pkt.num; i++)
            cprintf("%d ", args[i]);
        cprintf("\n");

        // Assume only two args for now
        if((r = (*serve)(buff, args[0], args[1])) < 0) {
            close(clientsock);
            die("Failed to get results\n");
        };

        cprintf("Server results: %s\n", buff);

        // Finished processing. buff should store the results now
        len = strlen(buff);
        if (write(clientsock, buff, len) != len) {
            close(clientsock);
            die("Failed to send bytes to client");
        }
        close(clientsock);
    }
}
