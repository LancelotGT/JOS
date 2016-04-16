#include "ns.h"

#define debug 0

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
  binaryname = "ns_input";
  struct jif_pkt *pkt = (struct jif_pkt*)REQVA;
  int r, i;

  // LAB 6: Your code here:
  //  - read a packet from the device driver
  //	- send it to the network server
  // Hint: When you IPC a page to the network server, it will be
  // reading from it for a while, so don't immediately receive
  // another packet in to the same physical page.

  if ((r = sys_page_alloc(0, pkt, PTE_P|PTE_U|PTE_W)) < 0)
    panic("sys_page_alloc: %e", r);

  while (1) {
    if ((r = sys_e1000_rx(pkt->jp_data)) < 0) {
      sys_yield();
      continue;
    }
    pkt->jp_len = r;

    if (debug)
       cprintf("ns req %d to %08x [page %08x]\n",
                  NSREQ_INPUT, ns_envid, uvpt[PGNUM(REQVA)]);

    ipc_send(ns_envid, NSREQ_INPUT, pkt, PTE_U | PTE_P | PTE_W);

    // Wait for network server to process the pkt
    // Note: if we don't wait long enough, the previous pkt sometimes
    // will be overwritten by later pkt and we cannot pass tests in
    // testinput.c
    for (i = 0; i < 10; i++)
        sys_yield();
  }
}
