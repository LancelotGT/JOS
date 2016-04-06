#include <kern/e1000.h>
#include <inc/string.h>

static struct tx_desc tx_descs[NTDESC];
static char tx_packets[MAXPKTLEN * NTDESC];

// LAB 6: Your driver code here
int e1000_attach(struct pci_func *pcif)
{
    int i;

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

    // init transmit descriptors
    for (i = 0; i < NTDESC; i++) {
        tx_descs[i].addr = PADDR(&tx_packets[i * MAXPKTLEN]);
        tx_descs[i].cmd |= E1000_TXD_CMD_RS;
        tx_descs[i].status |= E1000_TXD_STA_DD;
    }

    // test for transmitting packets in kernel space
    int int_packet[200];
    for (i = 0; i < 5 * NTDESC; i++)
        e1000_tx(&int_packet, 4 * 200);
    return 0;
}

int e1000_tx(void* addr, uint16_t length) {
    int tail = e1000[E1000_TDT];

    if (!(tx_descs[tail].status & E1000_TXD_STA_DD)) {
        // if the dd field is not set, the desc is not free
        // we simply drop the packet in this case
        cprintf("Transmit descriptors array is full\n");
        return -1;
    }

    memmove(KADDR(tx_descs[tail].addr), addr, length);
    tx_descs[tail].length = length;
    tx_descs[tail].status &= ~E1000_TXD_STA_DD; 
    e1000[E1000_TDT] = (tail + 1) % NTDESC;
    return 0;
}
