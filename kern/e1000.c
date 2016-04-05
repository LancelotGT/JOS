#include <kern/e1000.h>

// LAB 6: Your driver code here
int e1000_attach(struct pci_func *pcif)
{
    pci_func_enable(pcif);
    e1000 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]); // VM mapping for BAR 0
    cprintf("Status is: 0x%x. Desired: 0x80080783\n", e1000[E1000_STATUS_REG]);
    return 0;
}
