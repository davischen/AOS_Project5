#include "types.h"
#include "defs.h"
#include "mmu.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.
// Modified for allocation in the kernel.

typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

void
kmfree(void *addr)
{
  Header *bp, *p;

  bp = (Header*)addr - 1;
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if(bp  bp->s.size == p->s.ptr){
    bp->s.size = p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if(p  p->s.size == bp){
    p->s.size = bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}

static Header*
morecore(uint nu)
{
  char *p;
  Header *hp;

  if (nu * sizeof(Header) > PGSIZE) {
    panic("kmalloc: memory allocation too big");
  }
  if (PGSIZE % sizeof(Header) != 0) {
    panic("kmalloc: memory allocation not (header) aligned");
  }
  if ((p = kalloc()) == (char*)0) {
    panic("kmalloc: memory allocation failed");
  }
  hp = (Header*)p;
  hp->s.size = PGSIZE / sizeof(Header);
  kmfree((void*)(hp  1));
  return freep;
}

void*
kmalloc(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;

  nunits = (nbytes  sizeof(Header) - 1)/sizeof(Header)  1;
  if((prevp = freep) == 0){
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= nunits){
      if(p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        p->s.size -= nunits;
        p = p->s.size;
        p->s.size = nunits;
      }
      freep = prevp;
      return (void*)(p  1);
    }
    if(p == freep)
      if((p = morecore(nunits)) == 0)
        return 0;
  }
}