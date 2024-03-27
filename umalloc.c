#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

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
free(void *ap)
{
  Header *bp, *p;

  bp = (Header*)ap - 1;
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
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

  if(nu < 4096)
    nu = 4096;
  p = sbrk(nu * sizeof(Header));
  // printf(1, "geetha malloc 56\n");
  if(p == (char*)-1)
    return 0;
  // printf(1, "geetha printing p:%s\n", p);
  // printf(1, "geetha malloc 59\n");
  hp = (Header*)p;
  // printf(1, "geetha malloc 61\n");
  hp->s.size = nu;
  // printf(1, "geetha malloc 62\n");
  free((void*)(hp + 1));
  // printf(1, "geetha malloc 64\n");
  return freep;
}

void*
malloc(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;
  // printf(1, "geetha malloc 68\n");

  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;

  if((prevp = freep) == 0){
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  // printf(1, "geetha malloc 75\n");
  for (p = prevp->s.ptr;; prevp = p, p = p->s.ptr)
  {
    // printf(1, "geetha malloc 77\n");
    // printf(1, "Geetha malloc nunits and psize: %d %d\n", nunits, p->s.size);
    if (p->s.size >= nunits)
    {
      // printf(1, "geetha malloc 86\n");
      if (p->s.size == nunits)
      {
        // printf(1, "Geetha malloc 90\n");
        prevp->s.ptr = p->s.ptr;
        // printf(1, "Geetha malloc 92\n");
      }
      else
      {
        // printf(1, "Geetha malloc 95\n");
        p->s.size -= nunits;
        // printf(1, "Geetha malloc 97\n");
        p += p->s.size;
        // printf(1, "Geetha malloc 99\n");
        p->s.size = nunits;
        // printf(1, "Geetha malloc 101\n");
        // printf(1, "Geetha malloc after nunits and psize calculation: %d %d\n", nunits, p->s.size);
      }
      freep = prevp;
      return (void *)(p + 1);
    }
    // printf(1, "geetha malloc 89\n");
    if (p == freep)
    {
      // printf(1, "geetha malloc 91\n");
      if ((p = morecore(nunits)) == 0)
      {
        // printf(1, "geetha malloc 93\n");
        return 0;
      }
    }
  }
}
