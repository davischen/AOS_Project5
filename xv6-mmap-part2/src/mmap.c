#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "mman.h"
#include "proc.h"

#define NULL (mmap_region*)0

//helper functions

static void remove_region(mmap_region* node, mmap_region* prev)
{
  if(node == myproc()->mmap_regions_head)
  {
    if(myproc()->mmap_regions_head->next_mmap_region != 0)
      myproc()->mmap_regions_head = myproc()->mmap_regions_head->next_mmap_region;
    else
      myproc()->mmap_regions_head = 0;
  }
  else
    prev->next_mmap_region = node->next_mmap_region;
  kmfree(node);
}
static void search_region(struct proc *curproc ,void *addr, uint length)
{
  if (curproc->mmap_regions_head == NULL) {
    return;
  }
  int size = 0;
  mmap_region* curnode = curproc->mmap_regions_head;
  mmap_region* next_node = curproc->mmap_regions_head;

  //iterate over the list to check if the parameters passed to munmap are in the list
  //reduce the process size, delete the node.
  while(next_node != 0)
  {
    if(next_node->start_addr == addr && next_node->length == length)
    {
      curproc->sz = deallocuvm(curproc->pgdir, curproc->sz, curproc->sz - length);
      switchuvm(curproc);
      curproc->number_regions--;
      // close the file we were mapping to
      if(next_node->region_type == MAP_FILE)
      {
        fileclose(curproc->ofile[next_node->fd]);
        curproc->ofile[next_node->fd] = 0;
      }
      if(next_node->next_mmap_region != (struct mmap_region*)0) {
        size = next_node->next_mmap_region->length;
        curnode->next_mmap_region->length = size;
      }
      remove_region(next_node, curnode);
    }
    curnode = next_node;
    next_node = curnode->next_mmap_region;
  }
}
/*int
munmap2(void *addr, uint length)
{
  struct mmap_region *mmap, *prev_mmap;
  struct proc *curproc = myproc();

  for (mmap = curproc->mmap_regions_head, prev_mmap = (struct mmap_region*)0;
       mmap != (struct mmap_region*)0;
       prev_mmap = mmap, mmap = mmap->next_mmap_region) {
    if ((uint)addr == (uint)mmap->start_addr && length == mmap->length) {
      deallocuvm(curproc->pgdir, (uint)addr + length, (uint)addr);
      if (prev_mmap != (struct mmap_region*)0) {
        prev_mmap->next_mmap_region = mmap->next_mmap_region;
      } else {
        curproc->mmap_regions_head = mmap->next_mmap_region;
      }
      kmfree(mmap);
      return 0;
    }
  }
  return -1;
}*/
static mmap_region* mmap_region_alloc(void* addr, uint length, int flags, int offset, int prot)
{
  //allocate new region memory
  mmap_region* new_region = (mmap_region*)kmalloc(sizeof(mmap_region));
  if(new_region == NULL)
    return NULL;

  //store the mapping information for the newly allocated memory region
  new_region->start_addr = addr;
  new_region->length = length;
  new_region->region_type = flags;
  new_region->offset = offset;
  new_region->prot = prot;
  new_region->next_mmap_region = 0;
  return new_region;
}
/*void*
mmap(void* addr, uint length, int prot, int flags, int fd, int offset)
{
  struct mmap_region *mmap;
  struct proc *curproc = myproc();
  int bad_addr;

  addr = (void*)PGROUNDUP((uint)addr + MMAPBASE);
  if ((uint)addr >= KERNBASE ||
      (uint)addr + PGROUNDUP(length) >= KERNBASE) {
    addr = (void*)MMAPBASE;
    if ((uint)addr + PGROUNDUP(length) >= KERNBASE) {
      return (void*)0;
    }
  }
  for (mmap = curproc->mmap_regions; mmap != (struct mmap_region*)0
         ;) {
    bad_addr = 0;
    if ((uint)addr >= (uint)mmap->start_addr &&
        (uint)mmap->start_addr + PGROUNDUP(mmap->length) >= (uint)addr) {
      bad_addr = 1;
    }
    if ((uint)mmap->start_addr >= (uint)addr &&
        (uint)addr + PGROUNDUP(length) >= (uint)mmap->start_addr) {
      bad_addr = 1;
    }
    if (bad_addr) {
      addr = (void*)(mmap->start_addr + PGROUNDUP(mmap->length) + PGSIZE);
      mmap = curproc->mmap_regions;
      continue;
    }
    mmap = mmap->next_mmap_region;
  }
  if ((uint)addr >= KERNBASE ||
      (uint)addr + PGROUNDUP(length) >= KERNBASE) {
    return (void*)0;
  }
  if (allocuvm_mmap(curproc->pgdir, (uint)addr, (uint)addr + length) == 0) {
    return (void*)0;
  }
  if ((mmap = kmalloc(sizeof(struct mmap_region))) ==
      (struct mmap_region*)0) {
    deallocuvm(curproc->pgdir, (uint)addr + length, (uint)addr);
    return (void*)0;
  }
  mmap->start_addr = (uint)addr;
  mmap->length = length;
  mmap->prot = prot;
  mmap->flags = flags;
  mmap->fd = fd;
  mmap->offset = offset;
  mmap->next_mmap_region = curproc->mmap_regions;
  curproc->mmap_regions = mmap;
  return addr;
}*/

void *mmap(void *addr, uint length, int prot, int flags, int fd, int offset)
{
  //it checks if the requested address is page-aligned
  if ((uint)addr % PGSIZE != 0) {
    return (void*)-1;
  }
  //check argument inputs 
  if (addr < (void*)0 || addr == (void*)KERNBASE || addr > (void*)KERNBASE || length < 1)
  {
    return (void*)-1;
  }

  //get current point
  struct proc *curproc = myproc();
  //it gets the current process size
  uint oldsz = curproc->sz;
  //adds the requested length to the new size of the process
  uint newsz = curproc->sz + length;

  // Expand process size
  curproc->sz = newsz;
  //if((curproc->sz = allocuvm_mmap(curproc->pgdir, oldsz, newsz))==0)
  //   return (void*)-1;

  //read pagetable of current process
  switchuvm(curproc);
  //==========new code==========
  //if allocation fails
  addr = (void*)(PGROUNDDOWN((uint)oldsz)+ MMAPBASE );//replace addr with oldsz
  if (addr >= (void *) KERNBASE ||
      addr + PGROUNDUP(length) >= (void *) KERNBASE) {
    addr = (void*)MMAPBASE;
    if (addr + PGROUNDUP(length) >= (void *) KERNBASE) {
      return (void*)0;
    }
  }
  //==========new code==========

  //if allocation fails
  addr = (void*)(PGROUNDDOWN(oldsz)+ MMAPBASE );//
  mmap_region* new_region = mmap_region_alloc(addr, length, flags, offset, prot);
  if(new_region == NULL)
  {
    //to free up previously new process size
    deallocuvm(curproc->pgdir, newsz, oldsz);
    return (void*)-1;
  }

  //check the flags and file descriptor argument
  if (flags == MAP_ANONYMOUS)
  {
    if (fd != -1) //fd must be -1 
    {
      deallocuvm(curproc->pgdir, newsz, oldsz);
      kmfree(new_region);
      return (void*)-1;
    }
  }
  else if (flags == MAP_FILE)
  {
    if (fd > -1)
    {
      if((fd=fdalloc(curproc->ofile[fd])) < 0)
      {
        deallocuvm(curproc->pgdir, newsz, oldsz);
        kmfree(new_region);
        return (void*)-1;
      }
      filedup(curproc->ofile[fd]);
      new_region->fd=fd;
    }
    else
    {
      deallocuvm(curproc->pgdir, newsz, oldsz);
      kmfree(new_region);
      return (void*)-1;
    }
  }

  if (curproc->number_regions == MAX_MMAP_REGIONS) {
    deallocuvm(curproc->pgdir, newsz, oldsz);
    kmfree(new_region);
    return (void*)-1;
  }

  // if it is the first region in current process, set header point to new region
  if(curproc->number_regions == 0)
    curproc->mmap_regions_head = new_region;
  // else iterate over the mapped list and check memory region in boundary of current process
  else
  {
    //it iterates over the mapped list and checks whether the memory region is in the boundary of the current process.
    mmap_region* curnode = curproc->mmap_regions_head;
    while(curnode->next_mmap_region != 0)
    {
      //from starting address to iterate all mapped list
      if(addr == curnode->start_addr)
      {
        addr += PGROUNDDOWN(PGSIZE+curnode->length);
        curnode = curproc->mmap_regions_head;
      }
      //check if addr is larger than upper limit (KERNBASE)
      else if(addr == (void*)KERNBASE || addr > (void*)KERNBASE)
      {
        //fail to allocate new region
        deallocuvm(curproc->pgdir, newsz, oldsz);
        kmfree(new_region);
        return (void*)-1;
      }
      //move to next until last region
      curnode = curnode->next_mmap_region;
    }
    //
    if (addr == curnode->start_addr)
      addr += PGROUNDDOWN(PGSIZE+curnode->length);
    curnode->next_mmap_region = new_region;

        //==========new code==========
    // the starting address of the memory mapping request or the starting address plus the length of the mapping exceeds the kernel's virtual address space (KERNBASE)
    if (addr >= (void*) KERNBASE ||
        addr + PGROUNDUP(length) >= (void*) KERNBASE) {
      return (void*)0;
    }
    // the memory allocation failed, so the function returns a null pointer.
    if (allocuvm_mmap(curproc->pgdir, (uint)addr, (uint)addr + length) == 0) {
      return (void*)0;
    }
    //if kmalloc
    /*if ((curnode = kmalloc(sizeof(struct mmap_region))) ==
        (struct mmap_region*)0) {
      deallocuvm(curproc->pgdir, (uint)addr + length, (uint)addr);
      return (void*)0;
    }*/
    //==========new code==========
  }

  //the number of regions add 1 to increment region count
  curproc->number_regions++;
  //assign new region's starting address
  new_region->start_addr = addr;
  return new_region->start_addr;  
}

void *mmap_bak(void *addr, uint length, int prot, int flags, int fd, int offset)
{
  //it checks if the requested address is page-aligned
  if ((uint)addr % PGSIZE != 0) {
    return (void*)-1;
  }

  //check argument inputs 
  if (addr < (void*)0 || addr == (void*)KERNBASE || addr > (void*)KERNBASE || length < 1)
  {
    return (void*)-1;
  }
  
  //get current point
  struct proc *curproc = myproc();
  //it gets the current process size
  uint oldsz = curproc->sz;
  //adds the requested length to the new size of the process
  uint newsz = curproc->sz + length;

  // Expand process size
  curproc->sz = newsz;
  //if((curproc->sz = allocuvm_mmap(curproc->pgdir, oldsz, newsz))==0)
  //   return (void*)-1;

  //read pagetable of current process
  switchuvm(curproc);
  //==========new code==========
  //if allocation fails
  addr = (void*)(PGROUNDDOWN((uint)oldsz)+ MMAPBASE );//replace addr with oldsz
  if (addr >= (void *) KERNBASE ||
      addr + PGROUNDUP(length) >= (void *) KERNBASE) {
    addr = (void*)MMAPBASE;
    if (addr + PGROUNDUP(length) >= (void *) KERNBASE) {
      return (void*)0;
    }
  }
  //==========new code==========

  mmap_region* new_region = mmap_region_alloc(addr, length, flags, offset, prot);
  if(new_region == NULL)
  {
    //to free up previously new process size
    deallocuvm(curproc->pgdir, newsz, oldsz);
    return (void*)-1;
  }

  //check the flags and file descriptor argument
  if (flags == MAP_ANONYMOUS)
  {
    if (fd != -1) //fd must be -1 
    {
      deallocuvm(curproc->pgdir, newsz, oldsz);
      kmfree(new_region);
      return (void*)-1;
    }
  }
  else if (flags == MAP_FILE)
  {
    if (fd > -1)
    {
      if((fd=fdalloc(curproc->ofile[fd])) < 0)
      {
        deallocuvm(curproc->pgdir, newsz, oldsz);
        kmfree(new_region);
        return (void*)-1;
      }
      filedup(curproc->ofile[fd]);
      new_region->fd=fd;
    }
    else
    {
      deallocuvm(curproc->pgdir, newsz, oldsz);
      kmfree(new_region);
      return (void*)-1;
    }
  }

  if (curproc->number_regions == MAX_MMAP_REGIONS) {
    deallocuvm(curproc->pgdir, newsz, oldsz);
    kmfree(new_region);
    return (void*)-1;
  }


  // if it is the first region in current process, set header point to new region
  if(curproc->number_regions == 0)
    curproc->mmap_regions_head = new_region;
  // else iterate over the mapped list and check memory region in boundary of current process
  else
  {
    //it iterates over the mapped list and checks whether the memory region is in the boundary of the current process.
    mmap_region* curnode = curproc->mmap_regions_head;
    while(curnode->next_mmap_region != 0)
    {
      //from starting address to iterate all mapped list
      //==========new code==========
      // the requested region overlaps with the current mapped region
      // there is any overlap between the existing mmap region and the new region being requested
      if((addr >= curnode->start_addr  &&
        (uint)curnode->start_addr + PGROUNDUP(curnode->length) >= (uint)addr) || ((uint)curnode->start_addr >= (uint)addr &&
        (uint)addr + PGROUNDUP(length) >= (uint)curnode->start_addr))
      {
        //cprintf("addr += PGROUNDDOWN(PGSIZE+curnode->length)");
        addr += PGROUNDDOWN(PGSIZE+curnode->length);
        curnode = curproc->mmap_regions_head;
        continue;
      }
      //==========new code==========
      //check if addr is larger than upper limit (KERNBASE)
      else if(addr == (void*)KERNBASE || addr > (void*)KERNBASE)
      {
        //fail to allocate new region
        deallocuvm(curproc->pgdir, newsz, oldsz);
        kmfree(new_region);
        return (void*)-1;
      }
      //move to next until last region
      curnode = curnode->next_mmap_region;
    }
    //
    if (addr == curnode->start_addr)
      addr += PGROUNDDOWN(PGSIZE+curnode->length);
    curnode->next_mmap_region = new_region;

    //==========new code==========
    // the starting address of the memory mapping request or the starting address plus the length of the mapping exceeds the kernel's virtual address space (KERNBASE)
    if (addr >= (void*) KERNBASE ||
        addr + PGROUNDUP(length) >= (void*) KERNBASE) {
      return (void*)0;
    }
    // the memory allocation failed, so the function returns a null pointer.
    if (allocuvm_mmap(curproc->pgdir, (uint)addr, (uint)addr + length) == 0) {
      return (void*)0;
    }
    //if kmalloc
    /*if ((curnode = kmalloc(sizeof(struct mmap_region))) ==
        (struct mmap_region*)0) {
      deallocuvm(curproc->pgdir, (uint)addr + length, (uint)addr);
      return (void*)0;
    }*/
    //==========new code==========
  }
  //the number of regions add 1 to increment region count
  curproc->number_regions++;
  //assign new region's starting address
  new_region->start_addr = addr;
  return new_region->start_addr;  
}


int munmap(void *addr, uint length)
{
  /*if (addr == NULL ||  length == 0)
    return -1;*/

  //verify that the address and length passed is indeed a valid mapping
  //addr has to smaller than KERNBASE, and its length must be larger than 1
  if (addr == (void*)KERNBASE || addr > (void*)KERNBASE || length < 1)
  {
    return -1;
  }
  struct proc *curproc = myproc();
  
  if (curproc->number_regions == 0)
    return -1;

  
  search_region(curproc,addr,length);
  
  return 0;
}

int msync (void* start_addr, uint length)
{
  struct proc *curproc = myproc();

  // If nothing has been allocated, no need to do msync
  if (curproc->number_regions == 0)
  {
    return -1;
  }
  int result=0;
  mmap_region *curnode = curproc->mmap_regions_head;
  //iterates over all the memory mapped regions of the process using a linked list of mmap regions
  while(curnode)
  {
    //checks whether any regions have been allocated to the process.
    if(curnode->start_addr == start_addr && curnode->length == length)
    {
      if(curnode->fd<0)
      {
        return -1;
      }
      //If it is marked as dirty
      //the memory region has been modified and needs to be written back to the file on disk.
      pte_t* ret = walkpgdir(curproc->pgdir, start_addr, 0);
      if((uint)ret & PTE_D)
      {
        //do nothing
      }

      if((result=fileseek(curproc->ofile[curnode->fd], curnode->offset + (start_addr - curnode->start_addr)))==-1)
      {
        return -1;
      }
      if((result=filewrite(curproc->ofile[curnode->fd], start_addr, length))==-1)
      {
        return -1;
      }
      return 0;
    }

    curnode = curnode->next_mmap_region;
  }
  
  return -1; //No match
}