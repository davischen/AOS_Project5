#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"
#include "mmu.h"
#include "mman.h"

int main(int argc, char *argv[])
{

  int size =  3*PGSIZE;
    // Allocate memory using mmap
  char* str = mmap(0, size, PROT_WRITE, MAP_FILE, -1, 0);

  if (str<=0)
  {
    printf(1, "XV6_TEST_OUTPUT : mmap failed\n");
    exit();
  }
  printf(1, "XV6_TEST_OUTPUT : mmap good\n");

  // Write some data to the memory region
  str[0] = 'x';

  printf(1, "XV6_TEST_OUTPUT : write good\n");

  // Cause a page fault by accessing unmapped memory
  char c = str[size*2];//fragment fault
  
  printf(1, "XV6_TEST_OUTPUT : this shouldn't print\n");

  if (c == 'x') {
    printf(1, "Test passed!\n");
  } else {
    printf(1, "Test failed!\n");
  }

  exit();
}