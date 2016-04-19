#ifndef JOS_INC_RPC_H
#define JOS_INC_RPC_H

#include <inc/types.h>

#define PKTLEN 1024
#define RPCMAXARGS 6

// supported data types for RPC
enum {
  INT = 0,
  CHAR,
  STRING,
  NTYPE    // indicate this arg is not used
};

// The rpc packet is formulated as follows so that
// the receiver can decode the argments
struct rpc_pkt {
    int num;                            // number of arguments
    int types[RPCMAXARGS];                 // the sizes of arguments
    char data[PKTLEN                    // the rest of the pkt is data
        - (RPCMAXARGS + 1) * sizeof(int)];
} __attribute__ ((packed));

// support at most six arguments
typedef int (*serve_function)(void* result, int32_t a1, int32_t a2,
        int32_t a3, int32_t a4, int32_t a5, int32_t a6);

#endif
