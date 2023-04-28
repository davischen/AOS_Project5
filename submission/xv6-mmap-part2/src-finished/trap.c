#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "mman.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;


// jps - pagefault_handler()
/*
 *  This function helps implement the lazy mmap allocation required for this project.
 *  When a pagefault occurs, this function will be called and the mmap linked list
 *  will be searched to see if the faulting address has been decleared as allocated.
 *  If it has been "allocated", the page containing the faulting address will acutally
 *  be allocated to the process.
 * 
 *  INPUT: struct trapframe *tf - the trapframe for a faulting address
 *  OUTPUT: void
 */
void
pagefault_handler(struct trapframe *tf)
{
  struct proc *curproc = myproc();
  //get the address of the page that caused the fault,
  uint fault_addr = rcr2();

  // Start -- Required debugging statement -----
  cprintf("============trap pagefault_handler============\n");
  cprintf("pid %d %s: trap %d err %d on cpu %d "
  "eip 0x%x addr 0x%x--trap pagefault_handler\n", curproc->pid, curproc->name, tf->trapno, tf->err, cpuid(), tf->eip, fault_addr);
  // End -- Required debugging statement ----

  //iterating over the linked list of memory-mapped regions (mmap_regions_head) 
  //and checking if the faulting address falls within the bounds of any of the regions.
  //the faulting address belongs to a valid memory-mapped region of the process
  int valid = 0;
  mmap_region *mmap_node = curproc->mmap_regions_head;

  //round down the faulting address to the start of the page.
  fault_addr = PGROUNDDOWN(fault_addr);

  cprintf("============for loop============\n");
  while(mmap_node)
  {
    //if (fault_addr == (uint)mmap_node->start_addr)
    if ((uint)(mmap_node->start_addr) <= fault_addr && (uint)(mmap_node->start_addr + mmap_node->length) > fault_addr)
    { 
      cprintf("============check prot_write============\n");
      //If a valid memory-mapped region is found, the code checks if the page fault was caused by a write operation
      if ((mmap_node->prot & PROT_WRITE) || !(tf->err & T_ERR_PGFLT_W)) 
      {
        valid = 1;
      }
      break; 
    }
    else
    {
      mmap_node = mmap_node->next_mmap_region;
    }
  }

  if (valid == 1)
  {
    //cprintf("============valid is ============\n");
    char *mem;
    mem = kalloc();

    if(mem == 0)
    {
      cprintf("============check mem failure============\n");
      deallocuvm(curproc->pgdir,fault_addr+PGSIZE,fault_addr);
      kfree(mem);
      goto error;
    }
    memset(mem, 0, PGSIZE);
    
    // If the mmap region does not have write permission, then the page table entry is marked with only user permission.
    // Otherwise, it is marked with both user and write permission. 
    int perm;
    cprintf("============write permissions============\n");
    if (mmap_node->prot == PROT_WRITE)
    {
      perm = PTE_W|PTE_U; //give write permissions
    }
    else
    {
      perm = PTE_U; //do not give write permissions
    }
    //the mappages() function is used to map the page to the faulting virtual address in the current process’s page table.

    cprintf("============check mappages============\n");
    if(mappages(curproc->pgdir, (char*)fault_addr, PGSIZE, V2P(mem), perm) < 0)
    {
      deallocuvm(curproc->pgdir,fault_addr+PGSIZE,fault_addr);
      kfree(mem);
      goto error;
    }

    switchuvm(curproc);

    // checking if the region type of the page that caused the fault is MAP_FILE
    if (mmap_node->region_type == MAP_FILE)
    {
      cprintf("============MAP_FILE============\n");
      //checks if the file descriptor for the file being accessed exists in the current process's file descriptor table.
      if (curproc->ofile[mmap_node->fd])
      {
        cprintf("============check file============\n");
        int result=0;
        //the code seeks to the appropriate offset in the file and reads the required data into memory.
        if((result=fileseek(curproc->ofile[mmap_node->fd], mmap_node->offset))==-1)
        {
          deallocuvm(curproc->pgdir,fault_addr+PGSIZE,fault_addr);
          kfree(mem);
          goto error;
        }
        cprintf("============check file 2============\n");
        if((result=fileread(curproc->ofile[mmap_node->fd], mem, mmap_node->length))==-1)
        {
          deallocuvm(curproc->pgdir,fault_addr+PGSIZE,fault_addr);
          kfree(mem);
          goto error;
        }
        cprintf("============change flag============\n");
        //the code clears the dirty bit of the page table entry corresponding to the page that caused the fault. 
          pde_t* pde = &(myproc()->pgdir)[PDX(mmap_node->start_addr)];
          pte_t* pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
          pte_t* pte = &pgtab[PTX(mmap_node->start_addr)];

          *pte &= ~PTE_D;
      }
    }
  }
  else  
  {
    //If the page fault did not occur on a memory-mapped file
    //then the code prints an error message and sets the killed flag of the current process to 1.
    error:
      cprintf("============valid failure or not exists in mmap region list============\n");
      if(myproc() == 0 || (tf->cs&3) == 0){
        // In kernel
        //The error message contains information about the process ID, name, the type of trap, 
        //and the address that caused the page fault.
        cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
                tf->trapno, cpuid(), tf->eip, rcr2());
        panic("trap");
      }
      // In user space
      cprintf("pid %d %s: trap %d err %d on cpu %d "
              "eip 0x%x addr 0x%x--kill proc\n",
              myproc()->pid, myproc()->name, tf->trapno,
              tf->err, cpuid(), tf->eip, rcr2());
      myproc()->killed = 1;
  }
}
void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
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
  case T_IRQ0 + IRQ_IDE+1:
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
  case T_PGFLT: //jps - added pagefault check in the trap switch statement
    pagefault_handler(tf);
    break;
  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
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
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
