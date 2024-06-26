#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

//Global variables
static struct proc *initproc;
int nextpid = 1;
uint LCG_MULTIPLIER = 1664525;
uint LCG_INCREMENTOR = 1013904223;
int DEFAULT_TICKETS = 150;

struct processschedulerinfo {
  struct proc proc;
  int fifoorder;
  int tickets;

  //For analysis purpose. This will help in calculating metrics like response time and waiting time
  int createdtimeinticks;
  int firstscheduledtimeinticks;
};

struct {
  struct spinlock lock;
  struct processschedulerinfo processschedulinginfo[NPROC];
} ptable;


extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//Using Linear Congruential Generator method and system ticks to generate random number
int get_random(int x, int y) {
    uint xticks = ticks;
    uint rand = (LCG_MULTIPLIER * xticks + LCG_INCREMENTOR) % (y - x) + x;   
    return rand;
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct processschedulerinfo *psi;
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++)
    if(psi->proc.state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p = &(psi->proc);
  p->state = EMBRYO;
  p->pid = nextpid++;
  psi->fifoorder = p->pid;
  psi->tickets = DEFAULT_TICKETS;
  p->ticks_running = 0;
  psi->createdtimeinticks = ticks;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct processschedulerinfo *psi;
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++){
    p = &(psi->proc);
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct processschedulerinfo *psi;
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++){
      p = &(psi->proc);
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

void scheduleprocessincpu(struct cpu *c, struct proc *p)
{
  c->proc = p;
  switchuvm(p);
  p->state = RUNNING;
  p->ticks_running++;

  swtch(&(c->scheduler), p->context);
  switchkvm();
  // Process is done running for now.
  // It should have changed its p->state before coming back.
  c->proc = 0;
}

void
scheduler_fifo(struct cpu *c)
{
  struct processschedulerinfo *psi;
  struct proc *p;
  struct processschedulerinfo *selectedproc = 0; // Initialize to NULL

  // Find the earliest RUNNABLE process in the list. 
  // Note: We don't need to create new queue for FIFO implementation. We can use existing ptable.proc array since
  // it stores processes with incremental processId values. So less pid means it came first. This implementation will 
  // save extra space and computation cost. This is required too because of less space by xv6 kernel and that will trap other processes while calling balloc.
  for(psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++){
    p = &(psi->proc);
    if(p->state == RUNNABLE) {
      if (selectedproc == 0 || psi->fifoorder < selectedproc->fifoorder) {
        // If no process is selected yet or the current process has a lower PID,
        // update the selected process to the current one
        selectedproc = psi;
      }
    }
  }

  // If a RUNNABLE process is found, schedule it
  if (selectedproc != 0) {
    p = &(selectedproc->proc);
    if(p->ticks_running == 0) {
      selectedproc->firstscheduledtimeinticks = ticks;
    }
    scheduleprocessincpu(c, p);
  }
}

void scheduler_lottery(struct cpu *c)
{
  struct processschedulerinfo *psi;
  struct proc *p;
  int total_tickets = 0;

  for (psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++)
  {
    p = &(psi->proc);
    if (p->state == RUNNABLE)
    {
      total_tickets += psi->tickets;
    }
  }

  // No process to run
  if(total_tickets == 0)
  {
    return;
  }

  int ticket = get_random(0, total_tickets);
  int chosen_tickets = 0;
  for (psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++)
  {
    p = &(psi->proc);
    if (p->state == RUNNABLE)
    {
      chosen_tickets += psi->tickets;
      if (chosen_tickets > ticket)
      {
        // Schedule the chosen process
        if(p->ticks_running == 0) {
          psi->firstscheduledtimeinticks = ticks;
        }
        scheduleprocessincpu(c, p);
        break;
      }
    }
  }
}

void
scheduler_default(struct cpu *c)
{
  struct processschedulerinfo *psi;
  struct proc *p;
  for(psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++){
      p = &(psi->proc);
      if(p->state != RUNNABLE)
        continue;

      if(p->ticks_running == 0) {
        psi->firstscheduledtimeinticks = ticks;
      }
      scheduleprocessincpu(c, p);
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;)
  {
    // Enable interrupts on this processor.
    sti();
    
    acquire(&ptable.lock);  

    #ifdef SCHEDULER_FIFO
        scheduler_fifo(c);
    #elif defined(SCHEDULER_LOTTERY)
        scheduler_lottery(c);
    #else
        scheduler_default(c);
    #endif

    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct processschedulerinfo *psi;
  struct proc *p;

  for (psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++)
  {
    p = &(psi->proc);
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
  }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct processschedulerinfo *psi;
  struct proc *p;

  acquire(&ptable.lock);
  for(psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++){
    p = &(psi->proc);
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct processschedulerinfo *psi;
  struct proc *p;
  char *state;
  uint pc[10];

  for(psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++){
    p = &(psi->proc);
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int ticks_running(void)
{
  int pid;
  int ticksrunning;
  if (argint(0, &pid) < 0)
    return -1;

  struct processschedulerinfo *psi;
  struct proc *p;
  acquire(&ptable.lock);
  for (psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++)
  {
    p = &(psi->proc);
    if (p->pid == pid && (p->state == RUNNABLE || p->state == RUNNING || p->state == EMBRYO || p->state == SLEEPING))
    {
      ticksrunning = p->ticks_running;
      release(&ptable.lock);
      return ticksrunning;
    }
  }
  release(&ptable.lock);
  return -1;
}

int fifo_position(void)
{
  int pid;
  int fifoposition = 1;
  int processexists = 0;
  if (argint(0, &pid) < 0)
    return -1;

  struct processschedulerinfo *psi;
  struct proc *p;
  acquire(&ptable.lock);
  for (psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++)
  {
    p = &(psi->proc);
    if (p->pid < pid && (p->state == RUNNABLE || p->state == RUNNING))
    {
      fifoposition++;
    }
    else if (p->pid == pid && (p->state == RUNNABLE || p->state == RUNNING))
    {
      processexists = 1;
    }
  }
  release(&ptable.lock);
  if (processexists == 1)
  {
    return fifoposition;
  }
  return -1;
}

int
get_lottery_tickets(void)
{
  int pid;
  int lotterytickets;

  if (argint(0, &pid) < 0)
    return -1;

  struct processschedulerinfo *psi;
  struct proc *p;
  acquire(&ptable.lock);
  for (psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++)
  {
    p = &(psi->proc);
    if (p->pid == pid && (p->state == RUNNABLE || p->state == RUNNING))
    {
      lotterytickets = psi->tickets;
      release(&ptable.lock);
      return lotterytickets;
    }
  }
  release(&ptable.lock);
  return -1;
}

int set_lottery_tickets(void)
{
  int lotterytickets;
  struct proc *p;
  struct proc *currproc;
  struct processschedulerinfo *psi;

  if (argint(0, &lotterytickets) < 0)
    return -1;

  currproc = myproc();
  acquire(&ptable.lock);

  for (psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++)
  {
    p = &(psi->proc);
    if (p->pid == currproc->pid)
    {
      psi->tickets = lotterytickets;
      release(&ptable.lock);
      return lotterytickets;
    }
  }
  release(&ptable.lock);
  return -1;
}

int get_first_scheduled_time(void)
{
  int pid;
  struct proc *p;
  struct processschedulerinfo *psi;
  int firstscheduledtime = -1;

  if (argint(0, &pid) < 0)
    return -1;

  acquire(&ptable.lock);
  for (psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++)
  {
    p = &(psi->proc);
    if (p->pid == pid && (p->state == RUNNABLE || p->state == RUNNING))
    {
      firstscheduledtime = psi->firstscheduledtimeinticks;
      break;
    }
  }
  release(&ptable.lock);
  return firstscheduledtime;
}

int get_created_time(void)
{
  int pid;
  struct proc *p;
  struct processschedulerinfo *psi;
  int createdtime = -1;

  if (argint(0, &pid) < 0)
    return -1;

  acquire(&ptable.lock);
  for (psi = ptable.processschedulinginfo; psi < &ptable.processschedulinginfo[NPROC]; psi++)
  {
    p = &(psi->proc);
    if (p->pid == pid && (p->state == RUNNABLE || p->state == RUNNING))
    {
      createdtime = psi->createdtimeinticks;
      break;
    }
  }
  release(&ptable.lock);
  return createdtime;
}