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

static void remove_region(mmap_region* node, mmap_region* prev)
{
  if(node == myproc()->region_head)
  {
    if(myproc()->region_head->next_region != 0)
      myproc()->region_head = myproc()->region_head->next_region;
    else
      myproc()->region_head = 0;
  }
  else
    prev->next_region = node->next_region;
  kmfree(node);
}
static void search_region(struct proc *curproc ,void *addr, uint length)
{
  if (curproc->region_head == NULL) {
    return;
  }
  int size = 0;
  mmap_region* curnode = curproc->region_head;
  mmap_region* next_node = curproc->region_head;

  //iterate over the list to check if the parameters passed to munmap are in the list
  //reduce the process size, delete the node.
  while(next_node != 0)
  {
    if(next_node->start_addr == addr && next_node->length == length)
    {
      curproc->sz = deallocuvm(curproc->pgdir, curproc->sz, curproc->sz - length);
      switchuvm(curproc);
      curproc->number_regions--;
      remove_region(next_node, curnode);
      size = next_node->next_region->length;
      curnode->next_region->length = size;
    }
    curnode = next_node;
    next_node = curnode->next_region;
  }
}
void *mmap(void *addr, uint length, int prot, int flags, int fd, int offset)
{
  //get pointer to current process
  struct proc *curpro = myproc();
  uint oldsz = curpro->sz;
  uint newsz = curpro->sz + length;

  //check argument inputs 
  if (addr < (void*)0 || addr == (void*)KERNBASE || addr > (void*)KERNBASE || length < 1)
  {
    return (void*)-1;
  }

  curpro->sz = newsz;

  //List nodes are allocated using kmalloc (from kmalloc.c)
  //assigning memory to struct variable
  mmap_region* new_mapp_region = (mmap_region*)kmalloc(sizeof(mmap_region));
  if (new_mapp_region == NULL)
  {
    return (void*)-1;
  }

  //assign list data and meta data to the new region
  new_mapp_region->start_addr = (addr = (void*)(PGROUNDDOWN(oldsz) + MMAPBASE));
  new_mapp_region->length = length;
  new_mapp_region->prot = prot;
  new_mapp_region->region_type = flags;
  new_mapp_region->offset = offset;
  new_mapp_region->next_region = 0;

  //check the flags and file descriptor argument
  /*if (flags == MAP_ANONYMOUS)
  {
    if (fd != -1) //fd must be -1 
    {
      kmfree(new_mapp_region);
      return (void*)-1;
    }
   
  }
  else if (flags == MAP_FILE)
  {
    if (fd > -1)
    {
      if((fd=fdalloc(curpro->ofile[fd])) < 0)
        return (void*)-1;
      filedup(curpro->ofile[fd]);
      new_mapp_region->fd = fd;
    }
    else
    {
      kmfree(new_mapp_region);
      return (void*)-1;
    }
  }*/

  //handle first call to mmap
  if (curpro->number_regions == 0)
  {
    curpro-> region_head = new_mapp_region;
  }
  else //add region to an already existing mapped_regions list
  {
    mmap_region* itr = curpro->region_head;
    while (itr->next_region != 0)
    {
      if (addr == itr->start_addr)
      {
        addr += PGROUNDDOWN(PGSIZE+itr->length);
        itr = curpro-> region_head; //start over, we may overlap past regions
      }
      else if (addr == (void*)KERNBASE || addr > (void*)KERNBASE) //when run out of memory!
      {
        kmfree(new_mapp_region);
        return (void*)-1;
      }
      itr = itr->next_region;
    }
    // Catch the final node that isn't checked in the loop
    if (addr == itr->start_addr)
    {
      addr += PGROUNDDOWN(PGSIZE+itr->length);
    }

    //add new region to the end of our mmap_regions list
    itr->next_region = new_mapp_region;
  }

  //increase region count and retrun the new region's starting address
  curpro->number_regions++;
  new_mapp_region->start_addr = addr;

  return new_mapp_region->start_addr;
}
//mmap is to create a new mapping in the calling process's address space
void *mmap2(void *addr, uint length, int prot, int flags, int fd, int offset)
{
  //it checks if the requested address is page-aligned
  if ((uint)addr % PGSIZE != 0) {
    return (void*)-1;
  }

  //get current point
  struct proc *curproc = myproc();
  //it gets the current process size
  uint oldsz = curproc->sz;
  //adds the requested length to the new size of the process
  uint newsz = curproc->sz + length;

  //allocuvm to extend memory size
  //extend the memory size of the process by the requested length.
  if((curproc->sz = allocuvm_proc(curproc->pgdir, oldsz, newsz))==0)
     return (void*)-1;

  //read pagetable of current process
  switchuvm(curproc);

  //allocate new region memory
  mmap_region* new_region = (mmap_region*)kmalloc(sizeof(mmap_region));
  //if allocation fails
  if(new_region == NULL)
  {
    //to free up previously new process size
    deallocuvm(curproc->pgdir, newsz, oldsz);
    return (void*)-1;
  }

  //store the mapping information for the newly allocated memory region
  //addr = (void*)(PGROUNDDOWN(oldsz)+MMAPBASE);
  new_region->start_addr = (addr = (void*)(PGROUNDDOWN(oldsz) + MMAPBASE));;
  new_region->length = length;
  new_region->region_type = flags;
  new_region->offset = offset;
  new_region->prot = prot;
  new_region->next_region = 0;

  // Check the flags and file descriptor argument (flags, fd)
  if (flags == MAP_ANONYMOUS)
  {
    if (fd != -1) //fd must be -1 in this case (mmap man page sugestion for mobility)
    {
      kmfree(new_region);
      return (void*)-1;
    }
    // do not set r->fd. Not needed for Anonymous mmap
  }
  else if (flags == MAP_FILE)
  {
    if (fd > -1)
    {
      if((fd=fdalloc(curproc->ofile[fd])) < 0)
        return (void*)-1;
      filedup(curproc->ofile[fd]);
      new_region->fd = fd;
    }
    else
    {
      kmfree(new_region);
      return (void*)-1;
    }
  }

  // if it is the first region in current process, set header point to new region
  if(curproc->number_regions == 0)
    curproc->region_head = new_region;
  // else iterate over the mapped list and check memory region in boundary of current process
  else
  {
    //it iterates over the mapped list and checks whether the memory region is in the boundary of the current process.
    mmap_region* curnode = curproc->region_head;
    while(curnode->next_region != 0)
    {
      //from starting address to iterate all mapped list
      if(addr == curnode->start_addr)
      {
        addr += PGROUNDDOWN(PGSIZE+curnode->length);
        curnode = curproc->region_head;
      }
      //check if addr is larger than upper limit (KERNBASE)
      else if(addr == (void*)KERNBASE || addr > (void*)KERNBASE)
      {
        //fail to allocate new region
        kmfree(new_region);
        deallocuvm(curproc->pgdir, newsz, oldsz);
        return (void*)-1;
      }
      //move to next until last region
      curnode = curnode->next_region;
    }
    //
    if (addr == curnode->start_addr)
      addr += PGROUNDDOWN(PGSIZE+curnode->length);
    curnode->next_region = new_region;
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
  //use same search method as in munmap
  struct proc *p = myproc();

  // If nothing has been allocated, there is nothing to msync
  if (p->number_regions == 0)
  {
    return -1;
  }

  // Travese our mmap dll to see if address and length are valid
  mmap_region *cursor = p->region_head;

  while(cursor)
  {
    if(cursor->start_addr == start_addr && cursor->length == length)
    {
      // Make sure that the address was actually allocated
      pte_t* ret = walkpgdir(p->pgdir, start_addr, 0);
      if((uint)ret & PTE_D)
      {
        //do not write back non-dirty pages
      }

      fileseek(p->ofile[cursor->fd], cursor->offset);
      filewrite(p->ofile[cursor->fd], start_addr, length);
      return 0;
    }

    cursor = cursor->next_region;
  }

  // If we went through the whole ll without finding a match
  return -1;
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
  //=============
  // Check the flags and file descriptor argument (flags, fd)
  if (flags == MAP_ANONYMOUS)
  {
    if (fd != -1) //fd must be -1 in this case (mmap man page sugestion for mobility)
    {
      kmfree(mmap);
      return (void*)-1;
    }
    // do not set r->fd. Not needed for Anonymous mmap
  }
  else if (flags == MAP_FILE)
  {
    if (fd > -1)
    {
      if((fd=fdalloc(curproc->ofile[fd])) < 0)
        return (void*)-1;
      filedup(curproc->ofile[fd]);
      //mmap->fd = fd;
    }
    else
    {
      kmfree(mmap);
      return (void*)-1;
    }
  }
  //===========
  mmap->start_addr = (uint)addr;
  mmap->length = length;
  mmap->prot = prot;
  mmap->flags = flags;
  mmap->fd = fd;
  mmap->offset = offset;
  mmap->next_mmap_region = curproc->mmap_regions;
  curproc->mmap_regions = mmap;
  return addr;
}

int
munmap(void *addr, uint length)
{
  struct mmap_region *mmap, *prev_mmap;
  struct proc *curproc = myproc();

  for (mmap = curproc->mmap_regions, prev_mmap = (struct mmap_region*)0;
       mmap != (struct mmap_region*)0;
       prev_mmap = mmap, mmap = mmap->next_mmap_region) {
    if ((uint)addr == (uint)mmap->start_addr && length == mmap->length) {
      deallocuvm(curproc->pgdir, (uint)addr + length, (uint)addr);
      if (prev_mmap != (struct mmap_region*)0) {
        prev_mmap->next_mmap_region = mmap->next_mmap_region;
      } else {
        curproc->mmap_regions = mmap->next_mmap_region;
      }
      kmfree(mmap);
      return 0;
    }
  }
  return -1;
}
int msync (void* start_addr, uint length)
{
  //use same search method as in munmap
  struct proc *curproc = myproc();

  // If nothing has been allocated, there is nothing to msync
  if (curproc->mmap_regions == 0)
  {
    return -1;
  }

  // Travese our mmap dll to see if address and length are valid
  *//*struct mmap_region *cursor = curproc->mmap_regions;

  while(cursor)
  {
    if(cursor->start_addr == start_addr && cursor->length == length)
    {
      // Make sure that the address was actually allocated
      pte_t* ret = walkpgdir(curproc->pgdir, start_addr, 0);
      if((uint)ret & PTE_D)
      {
        //do not write back non-dirty pages
      }

      fileseek(curproc->ofile[cursor->fd], cursor->offset);
      filewrite(curproc->ofile[cursor->fd], start_addr, length);
      return 0;
    }

    cursor = cursor->next_mmap_region;
  }*/

  // If we went through the whole ll without finding a match
  //return -1;
//}