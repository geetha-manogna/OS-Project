#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[]; // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;
int pageallocator;

void tvinit(void)
{
  int i;

  for (i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE << 3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE << 3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void idtinit(void)
{
  lidt(idt, sizeof(idt));
}

int allocatememory(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;
  char *m, *last;
  pte_t *pte;
  void *va;
  uint pa;
  int perm = PTE_W | PTE_U;

  if (newsz >= KERNBASE)
    return 0;
  if (newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for (; a < newsz; a += PGSIZE)
  {
    mem = kalloc();
    if (mem == 0)
    {
      cprintf("allocuvm out of memory\n");
      cprintf("Number of pagefaults: %d\n", myproc()->pagefaults);
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    pa = V2P(mem);
    va = (char *)a;

    m = (char *)PGROUNDDOWN((uint)va);
    last = (char *)PGROUNDDOWN(((uint)va) + PGSIZE - 1);
    for (;;)
    {
      if ((pte = walkpgdir(pgdir, m, 1)) == 0)
      {
        cprintf("allocuvm out of memory (2)\n");
        deallocuvm(pgdir, newsz, oldsz);
        kfree(mem);
        return 0;
      }

      *pte = pa | perm | PTE_P;
      if (m == last)
        break;
      m += PGSIZE;
      pa += PGSIZE;
    }
  }
  return newsz;
}

// PAGEBREAK: 41
void trap(struct trapframe *tf)
{
  uint faulting_address; 
  struct proc *current_process;
  uint newsz = 0;
  // uint count = 0;
  uint i = 0;
  uint oldsz;

  if (tf->trapno == T_SYSCALL)
  {
    if (myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if (myproc()->killed)
      exit();
    return;
  }

  switch (tf->trapno)
  {
  case T_IRQ0 + IRQ_TIMER:
    if (cpuid() == 0)
    {
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE + 1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  case T_PGFLT:
  {
    faulting_address = rcr2(); // Getting the faulting address from CR2 register
    current_process = myproc();

#ifdef ALLOCATOR_LAZY
    pageallocator = LAZY_PAGES; // 1 is an indicator for number of page to be allocated.
#elif defined(ALLOCATOR_LOCALITY)
    pageallocator = LOCALITY_AWARE_PAGES; // 3 is an indicator for number of page to be allocated.
#endif

    // Validating whether the faulting address is within a valid range for the process
    if (faulting_address < KERNBASE && pageallocator != 0)
    {
      myproc()->pagefaults++;
      oldsz = PGROUNDDOWN(faulting_address);
      newsz = oldsz + PGSIZE;

      // cprintf("Page fault occured. Allocating %d page(s) based on Allocator used.\n", pageallocator);

      for (i = 0; i < pageallocator; i++)
      {
        if (newsz >= KERNBASE)
        {
          break;
        }

        if (allocatememory(current_process->pgdir, oldsz, newsz) == 0)
        {
          current_process->killed = 1; // Kill the process if allocation fails
          return;                      // Exit the trap function immediately
        }
        oldsz = newsz;
        newsz += PGSIZE;
      }
      switchuvm(current_process);
      return; // Successfully allocated a page, process should continue again
    }
  }
  // PAGEBREAK: 13
  default:
    if (myproc() == 0 || (tf->cs & 3) == 0)
    {
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if (myproc() && myproc()->state == RUNNING &&
      tf->trapno == T_IRQ0 + IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if (myproc() && myproc()->killed && (tf->cs & 3) == DPL_USER)
    exit();
}
