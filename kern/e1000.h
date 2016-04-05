#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pmap.h>
#include <kern/pci.h>

#define E1000_STATUS_REG 0x00008/4 // 4 byte register starting at 8th byte of reg space

int e1000_attach(struct pci_func *pcif); 
volatile uint32_t *e1000;

#endif	// JOS_KERN_E1000_H

