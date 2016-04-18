#include <inc/lib.h> 

// A simple distributed key-value store used to demonstrate
// the RPC package. This is the server side program.

#define PORT 8080
#define NBUCKETS 100
#define BUFFSIZE 32

static int table[NBUCKETS];

int proc(void* res, int key, int value)
{
    int i, len = 0;
    table[key % NBUCKETS] = value;

    for (i = 0; i < NBUCKETS; i++) {
       len += snprintf(res + len, BUFFSIZE, "%d ", table[i]); 
    }
    return 0;
}

void umain(int argc, char **argv)
{
    int r;

    r = rpc_server_init(PORT);
    if (r < 0)
        exit();

    rpc_server(proc);
    cprintf("Should not return\n");
}
