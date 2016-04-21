// hello, world
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
  char s[2][50] = {
    "/echo",
    "Hey, greetings from a new hello by exec"
  };
  char *argv2[3] = {s[0], s[1], 0};

  cprintf("hello, world\n");
  cprintf("i am environment %08x\n", thisenv->env_id);
  exec("/echo", (const char**) argv2);
}
