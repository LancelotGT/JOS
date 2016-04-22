/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/elf.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/e1000.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
  // Check that the user has permission to read memory [s, s+len).
  // Destroy the environment if not.

  // LAB 3: Your code here.
  user_mem_assert(curenv, s, len, PTE_U | PTE_P);
  // Print the string supplied by the user.
  cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
  return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
  return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
  int r;
  struct Env *e;

  if ((r = envid2env(envid, &e, 1)) < 0)
    return r;
  if (e == curenv)
      cprintf("[%08x] exiting gracefully\n", curenv->env_id);
  else
      cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
  env_destroy(e);
  return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
  sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
  // Create the new environment with env_alloc(), from kern/env.c.
  // It should be left as env_alloc created it, except that
  // status is set to ENV_NOT_RUNNABLE, and the register set is copied
  // from the current environment -- but tweaked so sys_exofork
  // will appear to return 0.

  struct Env *e;
  int ret;
  if ((ret = env_alloc(&e, curenv->env_id)))
    return ret;
  e->env_tf.tf_regs = curenv->env_tf.tf_regs;
  e->env_tf.tf_regs.reg_eax = 0;
  e->env_status = ENV_NOT_RUNNABLE;
  e->priority = curenv->priority;
  e->env_tf.tf_eip = curenv->env_tf.tf_eip;
  e->env_tf.tf_esp = curenv->env_tf.tf_esp;
  return e->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
  // Hint: Use the 'envid2env' function from kern/env.c to translate an
  // envid to a struct Env.
  // You should set envid2env's third argument to 1, which will
  // check whether the current environment has permission to set
  // envid's status.

  struct Env *e;
  int ret;
  if ((ret = envid2env(envid, &e, 1)) < 0)
    return ret;
  if (e->env_status != ENV_RUNNABLE && e->env_status != ENV_NOT_RUNNABLE)
    return -E_INVAL;
  e->env_status = status;
  return 0;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
  // LAB 5: Your code here.
  // Remember to check whether the user has supplied us with a good
  // address!
  struct Env *e;
  int ret;
  if ((ret = envid2env(envid, &e, 1)) < 0)
    return ret;
  user_mem_assert(curenv, tf, sizeof(struct Trapframe), PTE_U | PTE_P);
  e->env_tf = *tf;
  e->env_tf.tf_eflags |= FL_IF;
  return 0;
}

// Set envid's priority to priority, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE. 
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_priority(envid_t envid, int priority)
{
  struct Env *e;
  int ret;
  if ((ret = envid2env(envid, &e, 1)))
    return ret;
  if (e->env_status != ENV_RUNNABLE && e->env_status != ENV_NOT_RUNNABLE)
    return -E_INVAL;
  if (priority < 0 || priority > 99)
    return -E_INVAL;
  e->priority = priority;
  return 0;
} 

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
  struct Env *e;
  int ret;

  if ((ret = envid2env(envid, &e, 1)))
    return ret;
  user_mem_assert(e, func, PGSIZE, PTE_P | PTE_U | PTE_W);
  e->env_pgfault_upcall = func;
  return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
  // Hint: This function is a wrapper around page_alloc() and
  //   page_insert() from kern/pmap.c.
  //   Most of the new code you write should be to check the
  //   parameters for correctness.
  //   If page_insert() fails, remember to free the page you
  //   allocated!
  struct PageInfo *pp;
  struct Env* e;
  int ret;

  if ((ret = envid2env(envid, &e, 1)))
    return ret;
  if ((uint32_t)va >= UTOP || (uint32_t)va % PGSIZE)
    return -E_INVAL;

  // check permission bits
  if (!(perm & PTE_U) || !(perm & PTE_P) || (perm & ~PTE_SYSCALL))
    return -E_INVAL;

  pp = page_alloc(1);
  ret = page_insert(e->env_pgdir, pp, va, perm);
  return ret;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
             envid_t dstenvid, void *dstva, int perm)
{
  // Hint: This function is a wrapper around page_lookup() and
  //   page_insert() from kern/pmap.c.
  //   Again, most of the new code you write should be to check the
  //   parameters for correctness.
  //   Use the third argument to page_lookup() to
  //   check the current permissions on the page.

  struct Env *e_src, *e_dst;
  struct PageInfo *pp;
  pte_t* ppte;
  int ret;

  // check va range and page-aligned
  if ((uint32_t)srcva >= UTOP || (uint32_t)dstva >= UTOP ||
    (uint32_t)srcva % PGSIZE || (uint32_t) dstva % PGSIZE)
    return -E_INVAL;
  if ((ret = envid2env(srcenvid, &e_src, 0)))
    return ret;
  if ((ret = envid2env(dstenvid, &e_dst, 0)))
    return ret;

  pp = page_lookup(e_src->env_pgdir, srcva, &ppte);
  // srcva is not mapped
  if (!pp)
    return -E_INVAL;

  // if pp->pp_ref is 0, something is wrong,
  // probably doesn't manage reference count right
  // when install and remove page mappings
  assert(pp->pp_ref > 0);

  if (!(perm & PTE_U) || !(perm & PTE_P) || (perm & ~PTE_SYSCALL))
    return -E_INVAL;

  // cannot map read-only page as writable
  if ((perm & PTE_W) && !(*ppte & PTE_W))
    return -E_INVAL;
  ret = page_insert(e_dst->env_pgdir, pp, dstva, perm);
  return ret;
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
  // Hint: This function is a wrapper around page_remove().

  struct Env* e;
  int ret;
  if ((ret = envid2env(envid, &e, 1)))
    return ret;
  if ((uint32_t)va >= UTOP || (uint32_t)va % PGSIZE)
    return -E_INVAL;
  page_remove(e->env_pgdir, va);
  return 0;
}

// Implement the UNIX-style exec
// set the code segments using binary for current environment
// and re-initialize the stack using argv. Then sets up the initial
// eip and esp, then call sched-yield()
static int
sys_exec(void* binary, const char **argv)
{
  struct Elf *elfhdr = (struct Elf*)binary;
  struct Proghdr *ph, *eph;

  size_t string_size;
  int argc, i, r;
  char *string_store;
  uintptr_t *argv_store;
  uintptr_t init_esp;
  char argv_buf[100];
  char* p = argv_buf;

  if (elfhdr->e_magic != ELF_MAGIC)
      return -E_NOT_EXEC;

  ph = (struct Proghdr *)(binary + elfhdr->e_phoff);
  eph = ph + elfhdr->e_phnum;

  for (; ph < eph; ph++) {
    if (ph->p_type == ELF_PROG_LOAD) {
      // allocate pages for [ph->va, ph->va + ph->memsz)
      region_alloc(curenv, (void*)ph->p_va, ph->p_memsz);

      // copy filesz to va and zero out the rest memsz - filesz
      memcpy((void*)ph->p_va, binary + ph->p_offset, ph->p_filesz);
      memset((void*)(ph->p_va + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);
    }
  }


  // Count the number of arguments (argc)
  // and the total amount of space needed for strings (string_size).
  string_size = 0;
  for (argc = 0; argv[argc] != 0; argc++) {
     strcpy(argv_buf + string_size, argv[argc]);
     string_size += strlen(argv[argc]) + 1;
     argv_buf[string_size - 1] = '\0';
  }

  // Determine where to place the strings and the argv array.
  // Set up pointers into the temporary page 'UTEMP'; we'll map a page
  // there later, then remap that page into the child environment
  // at (USTACKTOP - PGSIZE).
  // strings is the topmost thing on the stack.
  string_store = (char*)USTACKTOP - string_size;
  // argv is below that.  There's one argument pointer per argument, plus
  // a null pointer.
  argv_store = (uintptr_t*)(ROUNDDOWN(string_store, 4) - 4 * (argc + 1));

  // Make sure that argv, strings, and the 2 words that hold 'argc'
  // and 'argv' themselves will all fit in a single stack page.
  if ((void*)(argv_store - 2) < (void*)USTACKTOP - PGSIZE)
    return -E_NO_MEM;

  // Copy the argv from kernel buffer p to user stack argv_store
  for (i = 0; i < argc; i++) {
    argv_store[i] = (intptr_t)string_store;
    strcpy(string_store, p);
    string_store += strlen(p) + 1;
    p += strlen(p) + 1;
  }
  argv_store[argc] = 0;
  assert(string_store == (char*)USTACKTOP);

  argv_store[-1] = (intptr_t)argv_store;
  argv_store[-2] = argc;

  init_esp = (intptr_t)&argv_store[-2];

  // set the program entry point
  curenv->env_tf.tf_eip = elfhdr->e_entry;
  curenv->env_tf.tf_esp = init_esp;
  sched_yield();
  return 0;
}


// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
  struct Env* e;
  int r;
  pte_t* ppte;
  struct PageInfo *pp;

  if((r = envid2env(envid, &e, 0)) < 0)
    return r;

  if ((uint32_t)srcva < UTOP && (uint32_t)srcva % PGSIZE)
    return -E_INVAL;

  if (!e->env_ipc_recving)
    return -E_IPC_NOT_RECV;

    // check permission bits
  if ((uint32_t)srcva < UTOP && 
    (!(perm & PTE_U) || !(perm & PTE_P) || (perm & ~PTE_SYSCALL)) )
    return -E_INVAL; 


  if ((uint32_t)srcva < UTOP) {
    pp = page_lookup(curenv->env_pgdir, srcva, &ppte);

    if (!pp || !ppte)
        return -E_INVAL;
        
    if (perm & PTE_W && !(*ppte & PTE_W))
        return -E_INVAL;

    if ((r = page_insert(e->env_pgdir, pp, e->env_ipc_dstva, perm)) < 0)
      return r;
    e->env_ipc_perm = perm;
  } else
    e->env_ipc_perm = 0;

  e->env_ipc_recving = 0;
  e->env_ipc_from = curenv->env_id;
  e->env_ipc_value = value;
  e->env_status = ENV_RUNNABLE;

  // the recving env will not actually return from sys_ipc_recv
  // but continue to run after the syscall
  // so we need to set the correct return value for them
  e->env_tf.tf_regs.reg_eax = 0;
  return 0;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
  if ((uint32_t)dstva % PGSIZE)
    return -E_INVAL;

  curenv->env_ipc_recving = 1; 

  if ((uint32_t) dstva < UTOP)
    curenv->env_ipc_dstva = dstva;

  curenv->env_status = ENV_NOT_RUNNABLE;
  sched_yield();
  return 0;
}

// Return the current time.
static int
sys_time_msec(void)
{
  return time_msec();
}

// Transmit a packet located in s with length len
static int
sys_e1000_tx(void *s, size_t len)
{
  // Check that the user has permission to read memory [s, s+len).
  // Destroy the environment if not.

  user_mem_assert(curenv, s, len, PTE_U | PTE_P);
  return e1000_tx((void*)s, len);
} 

// Receives a packet from specified address
static int
sys_e1000_rx(void *addr)
{
  // user cannot receive the packet to read-only memory
  user_mem_assert(curenv, addr, 0, PTE_U | PTE_P | PTE_W);
  return e1000_rx(addr);
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
  // Call the function corresponding to the 'syscallno' parameter.
  // Return any appropriate return value.
  // LAB 3: Your code here.

  switch (syscallno) {
  case SYS_cputs:
    sys_cputs((const char*)a1, a2);
    return 0;
  case SYS_cgetc:
    return sys_cgetc();
  case SYS_getenvid:
    return sys_getenvid();
  case SYS_env_destroy:
    return sys_env_destroy(a1);
  case SYS_yield:
    sys_yield();
  case SYS_exofork:
    return sys_exofork();
  case SYS_env_set_status:
    return sys_env_set_status(a1, a2);
  case SYS_env_set_trapframe:
    return sys_env_set_trapframe(a1, (struct Trapframe*)a2);
  case SYS_env_set_priority:
    return sys_env_set_priority(a1, a2);
  case SYS_page_alloc:
    return sys_page_alloc(a1, (void*)a2, a3);
  case SYS_page_map:
    return sys_page_map(a1, (void*)a2, a3, (void*)a4, a5);
  case SYS_page_unmap:
    return sys_page_unmap(a1, (void*)a2);
  case SYS_exec:
    return sys_exec((void*)a1, (const char**)a2);
  case SYS_env_set_pgfault_upcall:
    return sys_env_set_pgfault_upcall(a1, (void*)a2);
  case SYS_ipc_try_send:
    return sys_ipc_try_send(a1, a2, (void*)a3, a4);
  case SYS_ipc_recv:
    sys_ipc_recv((void*)a1);
    panic("sys_ipc_recv shouldn't return.");
  case SYS_time_msec:
    return sys_time_msec();
  case SYS_e1000_tx:
    return sys_e1000_tx((void*)a1, a2);
  case SYS_e1000_rx:
    return sys_e1000_rx((void*) a1);
  default:
    return -E_INVAL;
  }
}

