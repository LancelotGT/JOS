#include <inc/lib.h> 

// A simple distributed key-value store used to demonstrate
// the RPC package. This is the server side program.

#define IP "10.0.2.2"
#define PORT 28080
#define BUFFSIZE 32

#define put(res, key, val) \
    rpc_client(res, key, val)

#define NBUCKETS 5

void umain(int argc, char **argv)
{
    int r, i;
    char result[BUFFSIZE];

    r = rpc_client_init(IP, PORT);
    if (r < 0)
        exit();

    for (i = 0; i < NBUCKETS; i++) {
        put(result, i, 100 * i);
    }
}
