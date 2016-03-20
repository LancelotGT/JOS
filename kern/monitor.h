#ifndef JOS_KERN_MONITOR_H
#define JOS_KERN_MONITOR_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

struct Trapframe;

// Activate the kernel monitor,
// optionally providing a trap frame indicating the current state
// (NULL if none).
void monitor(struct Trapframe *tf);

// Functions implementing monitor commands.
int mon_help(int argc, char **argv, struct Trapframe *tf);
int mon_infokern(int argc, char **argv, struct Trapframe *tf);
int mon_infopg(int argc, char **argv, struct Trapframe *tf);
int mon_backtrace(int argc, char **argv, struct Trapframe *tf);
int mon_setcolor(int argc, char **argv, struct Trapframe *tf);
int mon_showmappings(int argc, char **argv, struct Trapframe *tf);
int mon_setpermission(int argc, char **argv, struct Trapframe *tf);
int mon_vmemdump(int argc, char **argv, struct Trapframe *tf);
int mon_pmemdump(int argc, char **argv, struct Trapframe *tf);
int mon_continue(int argc, char **argv, struct Trapframe *tf);
int mon_stepi(int argc, char **argv, struct Trapframe *tf);

#endif  // !JOS_KERN_MONITOR_H
