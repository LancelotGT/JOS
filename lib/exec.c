#include <inc/lib.h>
#include <inc/elf.h>

#define UTEMP2USTACK(addr)      ((void*)(addr) + (USTACKTOP - PGSIZE) - UTEMP)
#define UTEMP2                  (UTEMP + PGSIZE)
#define UTEMP3                  (UTEMP2 + PGSIZE)
#define CODETEMP                0xe0000000

// Exec a process from a program image loaded from the file system.
// prog: the pathname of the program to run.
// argv: pointer to null-terminated array of pointers to strings,
//   which will be passed to the child as its command-line arguments.
// Returns 0 on success, < 0 on failure.
int
exec(const char *prog, const char **argv)
{
  unsigned char elf_buf[512];

  int fd, i, r, n;
  struct Elf *elf;
  int perm;
  intptr_t temp = CODETEMP;
  struct Stat statbuf;

  if ((r = open(prog, O_RDONLY)) < 0)
    return r;
  fd = r;

  if ((r = fstat(fd, &statbuf)) < 0) {
    cprintf("cannot read stat of file %s\n", prog);
    return -E_INVAL;
  }

  for (i = temp; i < ROUNDUP(temp + statbuf.st_size, PGSIZE); i += PGSIZE) {
     if ((r = sys_page_alloc(0, (void*)i, PTE_P | PTE_U | PTE_W)) < 0)
       goto error;
     n = readn(fd, (void*)i, PGSIZE);
  }

  elf = (struct Elf*)temp;
  if (elf->e_magic != ELF_MAGIC) {
    close(fd);
    cprintf("elf magic %08x want %08x\n", elf->e_magic, ELF_MAGIC);
    return -E_NOT_EXEC;
  }

  if ((r = sys_exec((void*)temp, argv)) < 0) {
     cprintf("sys_exec error %e\n", r);
     goto error;
  }

  close(fd);
  fd = -1;
  return 0;

error:
  sys_env_destroy(0);
  close(fd);
  return r;
}

// Exec, taking command-line arguments array directly on the stack.
// NOTE: Must have a sentinal of NULL at the end of the args
// (none of the args may be NULL).
int
execl(const char *prog, const char *arg0, ...)
{
  // We calculate argc by advancing the args until we hit NULL.
  // The contract of the function guarantees that the last
  // argument will always be NULL, and that none of the other
  // arguments will be NULL.
  int argc = 0;
  va_list vl;

  va_start(vl, arg0);
  while (va_arg(vl, void *) != NULL)
    argc++;
  va_end(vl);

  // Now that we have the size of the args, do a second pass
  // and store the values in a VLA, which has the format of argv
  const char *argv[argc+2];
  argv[0] = arg0;
  argv[argc+1] = NULL;

  va_start(vl, arg0);
  unsigned i;
  for (i = 0; i < argc; i++)
    argv[i+1] = va_arg(vl, const char *);
  va_end(vl);
  return exec(prog, argv);
}
