// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>
#include <kern/env.h>

#define CMDBUF_SIZE 80 // enough for one VGA text line


struct Command {
  const char *name;
  const char *desc;
  // return -1 to force monitor to exit
  int (*func)(int argc, char **argv, struct Trapframe * tf);
};

static struct Command commands[] = {
  { "help",      "Display this list of commands",        mon_help       },
  { "info-kern", "Display information about the kernel", mon_infokern   },
  { "backtrace", "Display a stack backtrace of the kernel", mon_backtrace  },
  { "setcolor", "Change the color for console output", mon_setcolor    },
  { "showmappings", "Display the physical page mappings and permission bits that apply to particular range of virtual address", mon_showmappings},
  { "setpermission", "Set, clear or change the permission bits of any mapping", mon_setpermission},
  { "vmemdump", "Dump the contents of a range of memory given a virtual address range", mon_vmemdump},
  { "pmemdump", "Dump the contents of a range of memory given a physical address range", mon_pmemdump},
  { "continue", "Continue execution from the current location if stops at a break point", mon_continue},
  { "stepi", "single step execution of the current environment", mon_stepi}
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
  int i;

  for (i = 0; i < NCOMMANDS; i++)
    cprintf("%s - %s\n", commands[i].name, commands[i].desc);
  return 0;
}

int
mon_infokern(int argc, char **argv, struct Trapframe *tf)
{
  extern char _start[], entry[], etext[], edata[], end[];

  cprintf("Special kernel symbols:\n");
  cprintf("  _start                  %08x (phys)\n", _start);
  cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
  cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
  cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
  cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
  cprintf("Kernel executable memory footprint: %dKB\n",
          ROUNDUP(end - entry, 1024) / 1024);
  return 0;
}


int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
  // Your code here.
  uint32_t ebp = read_ebp();
  uint32_t eip, args[5];
  uint32_t cnt = 0;

  cprintf("Stack backtrace:\n");
  while (ebp) {
    cnt++;
    int i, offset;
    // get value from address where eip is pushed on stack
    eip = *(uint32_t *)(ebp + 4);
    // get argments value from upper stack
    for (i = 0; i < 5; i++)
        args[i] = *(uint32_t *)(ebp + 8 + i * 4);
    // get function name from symbol table
    struct Eipdebuginfo info;
    debuginfo_eip(eip, &info);
    offset = eip - info.eip_fn_addr;

    // print those stack info
    cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n",
            ebp, eip, args[0], args[1], args[2], args[3], args[4]);
    cprintf("         %s:%d: %.*s+%d\n", info.eip_file, info.eip_line,
            info.eip_fn_namelen, info.eip_fn_name, offset);
    // next stack frame
    ebp = *(uint32_t *)ebp;
  }
  return 0;
}

int
mon_setcolor(int argc, char **argv, struct Trapframe *tf)
{
  if (argc != 2) {
    cprintf("usage: %s color\n", argv[0]);
    return -1;
  }

  if (!strcmp(argv[1], "green"))
      set_color(GREEN);
  else if (!strcmp(argv[1], "cyan"))
      set_color(CYAN);
  else if (!strcmp(argv[1], "red"))
      set_color(RED);
  else if (!strcmp(argv[1], "magenta"))
      set_color(MAGENTA);
  else if (!strcmp(argv[1], "brown"))
      set_color(BROWN);
  else if (!strcmp(argv[1], "light_gray"))
      set_color(LIGHT_GRAY);
  else if (!strcmp(argv[1], "light_green"))
      set_color(LIGHT_GREEN);
  else if (!strcmp(argv[1], "light_cyan"))
      set_color(LIGHT_CYAN);
  else if (!strcmp(argv[1], "light_red"))
      set_color(LIGHT_RED);
  else if (!strcmp(argv[1], "light_magenta"))
      set_color(LIGHT_MAGENTA);
  else if (!strcmp(argv[1], "yellow"))
      set_color(YELLOW);
  else if (!strcmp(argv[1], "white"))
      set_color(WHITE);
  else cprintf("No such color.\n");

  return 0;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
  extern pde_t *kern_pgdir;
  uintptr_t addr[2], i;

  if (argc != 3) {
    cprintf("usage: %s addr1 addr2\n", argv[0]);
    return -1;
  }
  addr[0] = ROUNDDOWN(strtol(argv[1], NULL, 16), PGSIZE);
  addr[1] = ROUNDDOWN(strtol(argv[2], NULL, 16), PGSIZE);
  if (addr[0] > addr[1]) {
    cprintf("addr1 cannot be larger than addr2\n");
    return -1;
  }

  cprintf("Show mappings from 0x%08x to 0x%08x\n", addr[0], addr[1]);
  for (i = addr[0]; i <= addr[1]; i += PGSIZE) {
    pte_t* pte = pgdir_walk(kern_pgdir, (void*)i, 0);
    if (!pte)
      cprintf("    va  0x%08x  pa  not mapped\n", i);
    else
      cprintf("    va  0x%08x  pa  0x%08x perm  PTE_P  %x  PTE_W  %x  PTE_U  %x\n",
        i, *pte & (~0xfff), *pte & PTE_P, *pte & PTE_W, *pte & PTE_U);
  }
  return 0;
}

int
mon_setpermission(int argc, char **argv, struct Trapframe *tf)
{
  extern pde_t *kern_pgdir;
  uintptr_t addr, perm, old_perm;

  if (argc != 3) {
    cprintf("usage: %s addr perm\n", argv[0]);
    return -1;
  }
  addr = ROUNDDOWN(strtol(argv[1], NULL, 16), PGSIZE);
  perm = strtol(argv[2], NULL, 16);

  cprintf("Change permission at 0x%08x\n", addr);
  pte_t* pte = pgdir_walk(kern_pgdir, (void*)addr, 0);
  if (!pte)
    cprintf("    va  0x%08x  pa  not mapped\n", addr);
  else {
    old_perm = *pte & 0xfff;
    *pte = (*pte & (~0xfff)) | (perm & 0xfff);
    cprintf("    va  0x%08x  pa  0x%08x perm  PTE_P  %x  PTE_W  %x  PTE_U  %x\n",
      addr, *pte & (~0xfff), *pte & PTE_P, *pte & PTE_W, *pte & PTE_U);
    cprintf("    previous perm  PTE_P  %x  PTE_W  %x  PTE_U  %x\n",
      old_perm & PTE_P, old_perm & PTE_W, old_perm & PTE_U);
  }
  return 0;
}

int
mon_vmemdump(int argc, char **argv, struct Trapframe *tf)
{
  extern pde_t *kern_pgdir;
  uintptr_t addr[2], i, j;

  if (argc != 3) {
    cprintf("usage: %s addr1 addr2\n", argv[0]);
    return -1;
  }
  addr[0] = strtol(argv[1], NULL, 16);
  addr[1] = strtol(argv[2], NULL, 16);
  if (addr[0] > addr[1]) {
    cprintf("addr1 cannot be larger than addr2\n");
    return -1;
  }

  cprintf("Dump contents from va 0x%08x to va 0x%08x\n", addr[0], addr[1]);
  for (i = addr[0]; i <= addr[1]; i += 1) {
    j = ROUNDDOWN(i, PGSIZE);
    pte_t* pte = pgdir_walk(kern_pgdir, (void*)j, 0);
    if (!pte)
      cprintf("    0x%08x:    contents is not mapped\n", i);
    else {
      cprintf("    0x%08x:    0x%08x\n", i, *(uint32_t*)i);
    }
  }
  return 0;
}

int
mon_pmemdump(int argc, char **argv, struct Trapframe *tf)
{
  uintptr_t addr[2], i;

  if (argc != 3) {
    cprintf("usage: %s addr1 addr2\n", argv[0]);
    return -1;
  }
  addr[0] = strtol(argv[1], NULL, 16);
  addr[1] = strtol(argv[2], NULL, 16);
  if (addr[0] > addr[1]) {
    cprintf("addr1 cannot be larger than addr2\n");
    return -1;
  }

  cprintf("Dump contents from pa 0x%08x to pa 0x%08x\n", addr[0], addr[1]);
  for (i = addr[0]; i <= addr[1]; i += 1)
      cprintf("    0x%08x:    0x%08x\n", i, *(uint32_t*)KADDR(i));
  return 0;
}

int mon_continue(int argc, char**argv, struct Trapframe *tf) {
  if (!tf)
    panic("No available environment to continue");
  assert(tf->tf_trapno == T_BRKPT || tf->tf_trapno == T_DEBUG);

  // if continue after doing single-step, we need to reset
  // trap flag bit (#8 bit in eflags) to be 0
  if (tf->tf_trapno == T_DEBUG)
    tf->tf_eflags ^= (0x1 << 8);

  // restore execution of suspended environment
  env_run(curenv);
  return 0;
}

int mon_stepi(int argc, char**argv, struct Trapframe *tf) {
  if (!tf)
    panic("No available environment to execute");
  assert(tf->tf_trapno == T_BRKPT ||  tf->tf_trapno == T_DEBUG);
  tf->tf_eflags |= (0x1 << 8);

  // restore execution of suspended environment
  env_run(curenv);
  return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
  int argc;
  char *argv[MAXARGS];
  int i;

  // Parse the command buffer into whitespace-separated arguments
  argc = 0;
  argv[argc] = 0;
  while (1) {
    // gobble whitespace
    while (*buf && strchr(WHITESPACE, *buf))
      *buf++ = 0;
    if (*buf == 0)
      break;

    // save and scan past next arg
    if (argc == MAXARGS-1) {
      cprintf("Too many arguments (max %d)\n", MAXARGS);
      return 0;
    }
    argv[argc++] = buf;
    while (*buf && !strchr(WHITESPACE, *buf))
      buf++;
  }
  argv[argc] = 0;

  // Lookup and invoke the command
  if (argc == 0)
    return 0;
  for (i = 0; i < NCOMMANDS; i++)
    if (strcmp(argv[0], commands[i].name) == 0)
      return commands[i].func(argc, argv, tf);
  cprintf("Unknown command '%s'\n", argv[0]);
  return 0;
}

void
monitor(struct Trapframe *tf)
{
  char *buf;

  cprintf("Welcome to the JOS kernel monitor!\n");
  cprintf("Type 'help' for a list of commands.\n");

  if (tf != NULL)
    print_trapframe(tf);

  while (1) {
    buf = readline("K> ");
    if (buf != NULL)
      if (runcmd(buf, tf) < 0)
        break;
  }
}
