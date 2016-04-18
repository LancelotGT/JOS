#include <inc/lib.h>
#include <inc/rpcclient.h>

// A simple distributed key-value store used to demonstrate
// the RPC package. This is the client side program.

// The IP and PORT points to the forwarding port on ths host.
// We use port forwarding to communicate with RPC server to
// avoid some configuration issues with QEMU
#define IP "10.0.2.2"
#define PORT 26003
#define BUFFSIZE 32

#define put(res, key, val) \
    rpc_client(res, key, val)

#define NBUCKETS 100

void umain(int argc, char **argv)
{
    int r, i;
    char result[BUFFSIZE];

    r = rpc_client_init(IP, PORT);
    if (r < 0)
        exit();

    for (i = 0; i < NBUCKETS; i++) {
        // The value to put should be changed for different
        // clients to distinguish each client from each other
        put(result, i, 100 * (i + 1));
        cprintf("RPC client received: \n");
        cprintf("%s\n", result);
    }
}
