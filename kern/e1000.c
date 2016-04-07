#include <kern/e1000.h>
#include <inc/string.h>
#include <inc/error.h>

static struct tx_desc tx_descs[NTDESC];
static char tx_packets[MAXPKTLEN * NTDESC];

static struct rx_desc rx_descs[NRDESC];
static char rx_packets[NRDESC];

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

    // perform receive initialization
    e1000[E1000_RAL] = 0x12005452; // hardcoded 52:54:00:12:34:56
    e1000[E1000_RAH] = 0x00005634 | E1000_RAH_AV; // hardcoded 52:54:00:12:34:56
    e1000[E1000_RDBAL] = PADDR(rx_descs);
    e1000[E1000_RDLEN] = sizeof(rx_descs);
    e1000[E1000_RDH] = 0;
    e1000[E1000_RDT] = NRDESC;
    e1000[E1000_RCTL] |= E1000_RCTL_EN;
    e1000[E1000_RCTL] |= (!E1000_RCTL_LPE);
    e1000[E1000_RCTL] |= E1000_RCTL_LBM_NO;
    e1000[E1000_RCTL] |= E1000_RCTL_RDMTS_HALF;
    e1000[E1000_RCTL] |= E1000_RCTL_MO_0;
    e1000[E1000_RCTL] |= E1000_RCTL_BAM;
    e1000[E1000_RCTL] |= E1000_RCTL_BSEX;
    e1000[E1000_RCTL] |= E1000_RCTL_SZ_4096; // ? this could be different, just chose one
    /* TODO: Size of receive buffers? Does it need to be over 2048 bytes?
    Configure the Receive Buffer Size (RCTL.BSIZE) bits to reflect the size of the receive buffers
    software provides to hardware. Also configure the Buffer Extension Size (RCTL.BSEX) bits if
    receive buffer needs to be larger than 2048 bytes */

    e1000[E1000_RCTL] |= E1000_RCTL_SECRC;

    // init transmit descriptors
    for (i = 0; i < NTDESC; i++) {
        tx_descs[i].addr = PADDR(&tx_packets[i * MAXPKTLEN]);
        tx_descs[i].cmd |= E1000_TXD_CMD_RS;
        tx_descs[i].status |= E1000_TXD_STA_DD;
    }
    for (i = 0; i < NRDESC; i++){
        rx_descs[i].addr = PADDR(&rx_packets[i * MAXPKTLEN]);
        // rx_descs[i].cmd |= E1000_RXD_CMD_RS;
    }
    // test for transmitting packets in kernel space
    //int int_packet[200];

    //for (i = 0; i < 200; i++)
    //    int_packet[i] = i;
    //for (i = 0; i < 5 * NTDESC; i++)
    //    e1000_tx(&int_packet, 4 * 200);
    return 0;
}

int e1000_tx(void* addr, uint16_t length) {
    int tail = e1000[E1000_TDT];

    if (length > MAXPKTLEN)
        return -E_INVAL;

    if (!(tx_descs[tail].status & E1000_TXD_STA_DD)) {
        // if the dd field is not set, the desc is not free
        // we simply drop the packet in this case
        cprintf("Transmit descriptors array is full\n");
        return -1;
    }

    memmove(KADDR(tx_descs[tail].addr), addr, length);
    tx_descs[tail].length = length;
    tx_descs[tail].status &= ~E1000_TXD_STA_DD;
    tx_descs[tail].cmd |= E1000_TXD_CMD_EOP;
    e1000[E1000_TDT] = (tail + 1) % NTDESC;
    return 0;
}
