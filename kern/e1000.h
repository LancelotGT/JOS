#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pmap.h>
#include <kern/pci.h>

#define E1000_STATUS 0x00008 / 4 // 4 byte register starting at 8th byte of reg space

#define E1000_TDBAL 0x03800 / 4  /* TX Descriptor Base Address Low - RW */
#define E1000_TDLEN    0x03808 / 4  /* TX Descriptor Length - RW */ 
#define E1000_TDH      0x03810 / 4  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818 / 4  /* TX Descriptor Tail - RW */ 
#define E1000_TCTL     0x00400 / 4  /* TX Control - RW */
#define E1000_TIPG     0x00410 / 4  /* Transmit IPG Register */

#define E1000_RCTL     0x00100 / 4  /* RX Control - RW */
#define E1000_RDBAL    0x02800 / 4  /* RX Descriptor Base Address Low - RW */
#define E1000_RDLEN    0x02808 / 4  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810 / 4  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818 / 4  /* RX Descriptor Tail - RW */
#define E1000_RA       0x05400 / 4  /* Receive Address - RW Array */
#define E1000_RAL      0x05400 / 4  /* Receive Address Low - RW */
#define E1000_RAH      0x05404 / 4  /* Receive Address HIGH - RW */

/* Receive Control Registers */
#define E1000_RCTL_EN			0x00000002    /* enable */
#define E1000_RCTL_LPE			0x00000020    /* long packet enable */
#define E1000_RCTL_LBM_NO		0x00000000    /* no loopback mode */
#define E1000_RCTL_RDMTS_HALF	0x00000000    /* rx desc min threshold size */
#define E1000_RCTL_MO_0			0x00000000    /* multicast offset 11:0 */
#define E1000_RCTL_BAM			0x00008000    /* broadcast enable */
#define E1000_RCTL_SECRC		0x04000000    /* Strip Ethernet CRC */
#define E1000_RCTL_BSEX			0x02000000    /* Buffer size extension */
#define E1000_RCTL_SZ_2048      0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SZ_4096      0x00030000    /* rx buffer size 4096 */
#define E1000_RAH_AV			0x80000000    /* Receive descriptor valid */

/* Transmit Control Registers */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */

#define E1000_TXD_CMD_RS  0x08 /* Transmit Desc Report Status */
#define E1000_TXD_CMD_EOP 0x01 /* Transmit Desc End of Packet */
#define E1000_TXD_STA_DD  0x1 /* Transmit Desc Status DD field */

#define E1000_RXD_STA_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STA_EOP      0x02    /* End of Packet */
#define E1000_RXD_STA_IXSM     0x04    /* Ignore Checksum Indication */

#define E1000_TCTL_CT_INIT   0x00000010    /* initial collision threshold */
#define E1000_TCTL_COLD_INIT 0x00040000    /* initial collision distance */
#define E1000_TIPG_INIT      0x0060200a /* init values for TIPG in 13.4.34 */



#define NTDESC 32 // the number of the transmit descriptors, somehow arbitrary
#define NRDESC 128 // the number of receive descriptors, EX10 claims at least 128?
#define MAXPKTLEN 1518

volatile uint32_t *e1000;

int e1000_attach(struct pci_func *pcif);
int e1000_tx(void* addr, uint16_t length);
int e1000_rx(void* addr);

struct tx_desc
{
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__ ((packed));

struct rx_desc
{
    uint64_t addr;
    uint16_t length;
    uint16_t cso;
    uint8_t status;
    uint8_t errs;
    uint16_t special;
} __attribute__ ((packed));

#endif	// JOS_KERN_E1000_H

