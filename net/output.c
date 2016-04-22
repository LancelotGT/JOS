#include "ns.h"

#define debug 0

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
  binaryname = "ns_output";
  int r, perm;
  uint32_t req;
  int32_t whom;
  void* pg;
  struct jif_pkt *pkt = (struct jif_pkt*)REQVA;

  // LAB 6: Your code here:
  //  - read a packet from the network server
  //	- send the packet to the device driver
  while(1) {
      req = ipc_recv((int32_t*)&whom, (void*)REQVA, &perm);
      if (debug)
          cprintf("ns req %d from %08x [page %08x]\n",
                  req, whom, uvpt[PGNUM(REQVA)]);

      assert(whom == ns_envid);

      // All requests must contain an argument page
      if (!(perm & PTE_P)) {
          cprintf("Invalid request from %08x: no argument page\n",
                  whom);
          continue; // just leave it hanging...
      }

      pg = NULL;
      if (req == NSREQ_OUTPUT) {
          r = sys_e1000_tx(pkt->jp_data, pkt->jp_len);
          if (r < 0)
              cprintf("e1000 output request failed\n");
      } else {
          cprintf("Invalid request code %d from %08x\n", req, whom);
      }
  }
}
