// hello, world
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
  cprintf("hello, world\n");
  double result = 1.0 / 5;
  cprintf("1.0 divided by 0.5 is %d\n", (int)result); 
  cprintf("i am environment %08x\n", thisenv->env_id);
}
