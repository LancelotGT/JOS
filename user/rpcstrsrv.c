#include <inc/lib.h>
#include <inc/rpcserver.h>

// A simple distributed key-value store used to demonstrate
// the RPC package. This is the server side program.

// The RPC server binds to the port 8080 of JOS.
// This port is forwarded as port 26003 of the host OS.
#define PORT 8080
#define NBUCKETS 40
#define BUFFSIZE 32

struct list_node {
    struct list_node* next;
    char msg[20];
};

static struct list_node* table[NBUCKETS];

int proc(void* res, int key, int value, int a3, int a4, int a5, int a6)
{
    int i;
    struct list_node* l = table[key % NBUCKETS];

    // Traverse the bucket list, allocate new node
    // as needed
    if (!l) {
        l = malloc(sizeof(struct list_node));
        table[key % NBUCKETS] = l;
    } else {
        while (l->next != NULL)
            l = l->next;
        l->next = malloc(sizeof(struct list_node));
        l = l->next;
    }
    strcpy(l->msg, (char*) value);

    snprintf(res, BUFFSIZE, "index: %d value: %s", key % NBUCKETS, l->msg);

    // Traverse the entire hash table and print out the buckets
    cprintf("Show buckets: \n");
    for (i = 0; i < NBUCKETS; i++) {
        l = table[i];
        cprintf("index: %d values:", i);
        if (!l) {
            cprintf("empty\n");
            continue;
        }

        while (l) {
            cprintf(" %s", l->msg);
            l = l->next;
        }
        cprintf("\n");
    }
    return 0;
}

void umain(int argc, char **argv)
{
    int r, i;

    for (i = 0; i < NBUCKETS; i++)
        table[i] = NULL;

    r = rpc_server_init(PORT);
    if (r < 0)
        exit();

    rpc_server(proc);
    cprintf("Should not return\n");
}
