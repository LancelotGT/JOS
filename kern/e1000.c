#include <kern/e1000.h>

static struct tx_desc tx_descs[NTDESC];

// LAB 6: Your driver code here
int e1000_attach(struct pci_func *pcif)
{
    pci_func_enable(pcif);
    e1000 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]); // VM mapping for BAR 0
    cprintf("Status is: 0x%x. Desired: 0x80080783\n", e1000[E1000_STATUS]);

    assert(sizeof(tx_descs) % 128 == 0); // should be 128-byte aligned

    // perform transmit initialization
    e1000[E1000_TDBAL] = PADDR(tx_descs);
    e1000[E1000_TDLEN] = sizeof(tx_descs);
    e1000[E1000_TDH] = 0;
    e1000[E1000_TDT] = 0;
    e1000[E1000_TCTL] |= E1000_TCTL_EN;
    e1000[E1000_TCTL] |= E1000_TCTL_PSP;
    e1000[E1000_TCTL] |= E1000_TCTL_CT_INIT;
    e1000[E1000_TCTL] |= E1000_TCTL_COLD_INIT;
    e1000[E1000_TIPG] |= E1000_TIPG_INIT;
    return 0;
}
