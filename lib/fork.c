// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW         0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
  void *addr = (void*)utf->utf_fault_va;
  uint32_t err = utf->utf_err;
  int r;

  // Check that the faulting access was (1) a write, and (2) to a
  // copy-on-write page.  If not, panic.
  // Hint:
  //   Use the read-only page table mappings at uvpt
  //   (see <inc/memlayout.h>).

  if (!(err & FEC_WR) || !(uvpt[(uint32_t)addr / PGSIZE] & PTE_COW))
    panic("pgfault: faulting access %e", err);

  // Allocate a new page, map it at a temporary location (PFTEMP),
  // copy the data from the old page to the new page, then move the new
  // page to the old page's address.
  // Hint:
  //   You should make three system calls.

  if ((r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
    panic("sys_page_alloc: %e", r);
  memmove(PFTEMP, (void*)((uint32_t)addr / PGSIZE * PGSIZE), PGSIZE);
  if ((r = sys_page_map(0, PFTEMP, 0, (void*)((uint32_t)addr / PGSIZE * PGSIZE), PTE_P|PTE_U|PTE_W)) < 0)
    panic("sys_page_map: %e", r);
  if ((r = sys_page_unmap(0, PFTEMP)) < 0)
    panic("sys_page_unmap: %e", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
  int r;

  // if PTE_SHARE is set, just copy the mapping
  if (uvpt[pn] & PTE_SHARE) {
    if ((r = sys_page_map(0, (void*)(pn * PGSIZE), envid,
      (void*)(pn * PGSIZE), uvpt[pn] & PTE_SYSCALL)) < 0) {
      cprintf("sys_page_map failed: %e\n", r);
      return r;
    } 
    return 0;
  }

  if (uvpt[pn] & PTE_W || uvpt[pn] & PTE_COW) {

    if ((r = sys_page_map(0, (void*)(pn * PGSIZE), envid,
      (void*)(pn * PGSIZE), PTE_P | PTE_U | PTE_COW)) < 0)
        return r;

    // set PTE_COW bit for pgtable
    if ((r = sys_page_map(envid, (void*)(pn * PGSIZE), 0,
      (void*)(pn * PGSIZE), PTE_P | PTE_U | PTE_COW)) < 0)
        return r;

  } else {
    // if it is a read-only page, just copy the mapping
    if ((r = sys_page_map(0, (void*)(pn * PGSIZE), envid,
      (void*)(pn * PGSIZE), uvpt[pn] & PTE_SYSCALL)) < 0) {
      cprintf("sys_page_map failed: %e\n", r); 
      return r;
    }
  }
  return 0;
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
sduppage(envid_t envid, unsigned pn, int cow)
{
  int r;

  if (uvpt[pn] & PTE_W || uvpt[pn] & PTE_COW) {

    if (cow) {
      if ((r = sys_page_map(0, (void*)(pn * PGSIZE), envid,
      (void*)(pn * PGSIZE), PTE_P | PTE_U | PTE_COW)) < 0)
        return r;

      // set PTE_COW bit for pgtable
      if ((r = sys_page_map(envid, (void*)(pn * PGSIZE), 0,
        (void*)(pn * PGSIZE), PTE_P | PTE_U | PTE_COW)) < 0)
          return r;
    } else {
      if ((r = sys_page_map(0, (void*)(pn * PGSIZE), envid,
      (void*)(pn * PGSIZE), PTE_P | PTE_U | PTE_W)) < 0)
        return r;
    }

  } else {
    // if it is a read-only page, just copy the mapping
    if ((r = sys_page_map(0, (void*)(pn * PGSIZE), envid,
      (void*)(pn * PGSIZE), PGOFF(uvpt[pn]))) < 0) {
      cprintf("sys_page_map failed\n");
      return r;
    }
  }
  return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
  envid_t envid;
  int r;
  uint32_t i, j, pn;
  extern volatile pte_t uvpt[];
  extern volatile pde_t uvpd[];
  extern char end[];
  if (!thisenv->env_pgfault_upcall)
    set_pgfault_handler(pgfault);

  envid = sys_exofork();
  if (envid < 0)
    panic("sys_exofork: %e", envid);
  if (envid == 0) {
    // child process
    thisenv = &envs[ENVX(sys_getenvid())];
    return 0;
  }

  // parent process
  for (i = 0; i < NPDENTRIES; i++) {
    for (j = 0; j < NPTENTRIES; j++) {
      pn = i * NPDENTRIES + j;
      if (pn * PGSIZE < UTOP && uvpd[i] && uvpt[pn]
        && (pn * PGSIZE != UXSTACKTOP - PGSIZE)) {
        if ((r = duppage(envid, pn)))
          cprintf("duppage: %e\n", r);
      }
    }
  }

  if ((r = sys_page_alloc(envid, (void*)(UXSTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W)) < 0)
    panic("sys_page_alloc: %e", r);
  if ((r = sys_page_map(envid, (void*)(UXSTACKTOP - PGSIZE), 0, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
    panic("sys_page_map: %e", r);
  memmove(PFTEMP, (void*)(UXSTACKTOP - PGSIZE), PGSIZE);
  if ((r = sys_page_unmap(0, PFTEMP)) < 0)
    panic("sys_page_unmap: %e", r);

  sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall);
  sys_env_set_status(envid, ENV_RUNNABLE);
  return envid;
}

// Challenge!
int
sfork(void)
{
  envid_t envid;
  int r;
  uint32_t i, j, pn;
  extern volatile pte_t uvpt[];
  extern volatile pde_t uvpd[];
  extern char end[];
  if (!thisenv->env_pgfault_upcall)
    set_pgfault_handler(pgfault);

  envid = sys_exofork();
  if (envid < 0)
    panic("sys_exofork: %e", envid);
  if (envid == 0) {
    // child process
    thisenv = &envs[ENVX(sys_getenvid())];
    return 0;
  }

  // parent process
  for (i = 0; i < NPDENTRIES; i++) {
    for (j = 0; j < NPTENTRIES; j++) {
      pn = i * NPDENTRIES + j;
      if (pn * PGSIZE < UTOP && uvpd[i] && uvpt[pn]
        && (pn * PGSIZE != UXSTACKTOP - PGSIZE)
        && (pn * PGSIZE != USTACKTOP - PGSIZE)) {
        if ((r = sduppage(envid, pn, 0)))
          cprintf("sduppage: %e\n", r);
      }
    }
  }

  // map stack page as COW
  if ((r = sduppage(envid, (USTACKTOP - PGSIZE) / PGSIZE, 1)))
     cprintf("sduppage: %e\n", r);

  if ((r = sys_page_alloc(envid, (void*)(UXSTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W)) < 0)
    panic("sys_page_alloc: %e", r);
  if ((r = sys_page_map(envid, (void*)(UXSTACKTOP - PGSIZE), 0, PFTEMP, PTE_P|PTE_U|PTE_W)) < 0)
    panic("sys_page_map: %e", r);
  memmove(PFTEMP, (void*)(UXSTACKTOP - PGSIZE), PGSIZE);
  if ((r = sys_page_unmap(0, PFTEMP)) < 0)
    panic("sys_page_unmap: %e", r);

  sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall);
  sys_env_set_status(envid, ENV_RUNNABLE);
  return envid;
}
