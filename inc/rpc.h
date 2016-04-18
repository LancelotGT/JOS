#ifndef JOS_INC_RPC_H
#define JOS_INC_RPC_H

#define PKTLEN 1024
#define MAXARGS 10

// The rpc packet is formulated as follows so that
// the receiver can decode the argments
struct rpc_pkt {
    int num;                            // number of arguments
    int sizes[MAXARGS];                 // the sizes of arguments
    char data[PKTLEN                    // the rest of the pkt is data
        - (MAXARGS + 1) * sizeof(int)];
} __attribute__ ((packed));

// hardcode the number of arguments
// need to change to use va_args
typedef int (*serve_function)(void* result, int a1, int a2); 

#endif
