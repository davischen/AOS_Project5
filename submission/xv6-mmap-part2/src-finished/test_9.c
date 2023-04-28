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
#include "fcntl.h"
#include "stddef.h"

#define TEST_COUNT 20
#define MAX_SIZE 1000

#define PGSIZE 4096
//=====

#define MAP_SIZE 4096
#define ITERATIONS 1


#define TEST_FILENAME "test_file.txt"
#define FILE_SIZE 1024//(10 * 1024 * 1024) // 10MB
#define FILE_OFFSET 0x1000

int main()
{

    /*
   * Prepare a file which is filled with raw integer values. These
   * integer values are their offsets in the file.
   */
  int rc;
  char tmp_filename[FILE_SIZE];
  strcpy(tmp_filename, "test_file.txt");


  printf(1, "XV6_TEST_OUTPUT : copy data suceeded\n");
  // Create file
  int fd = open(tmp_filename, O_WRONLY | O_CREATE);
  if(fd<=0)
  {
    printf(1, "XV6_TEST_OUTPUT : file creation failed %d\n", fd);
    exit();
  }
  printf(1, "XV6_TEST_OUTPUT : file creation suceeded\n");



  // Write raw integer to the file
  const int map_size = 0x10;
  for (int i = 0; i < map_size * 2; i += sizeof(i))
  {
    uint written = write(fd, &i, sizeof(i));
    printf(1, "XV6_TEST_OUTPUT : writing %d to file\n", i);

    if(written != sizeof(i))
    {
      printf(1, "XV6_TEST_OUTPUT : file write failed\n");
      exit();
    }
  }
  printf(1, "XV6_TEST_OUTPUT : file write suceeded\n");



  // close file
  rc = close(fd);
  if(rc != 0)
  {
    printf(1, "XV6_TEST_OUTPUT : file close failed\n");
    exit();
  }
  printf(1, "XV6_TEST_OUTPUT : file close suceeded\n");



  // Open file again
  fd = open(tmp_filename, O_RDWR);
  if(fd<=0)
  {
    printf(1, "XV6_TEST_OUTPUT : file open failed\n");
    exit();
  }
  printf(1, "XV6_TEST_OUTPUT : file open suceeded\n");



  // mmap the file with a offset
  /* A valid mmap call with an offset specified. */
  int file_offset = 0x10;
  char *addr = (char *) mmap(0, map_size, PROT_WRITE, MAP_FILE, fd, file_offset);

  if (addr<=0)
  {
    printf(1, "XV6_TEST_OUTPUT : mmap failed\n");
    exit();
  }
  printf(1, "XV6_TEST_OUTPUT : mmap suceeded\n");



  //Validate the contents of the file from the specified offset.
  for (int i = 0; i < map_size; i += sizeof(i))
  {
    int expected = i + file_offset;
    int actual = *(int *) (addr + i);

    printf(1, "XV6_TEST_OUTPUT : Expected val : %d Actual val : %d\n",expected, actual);

    if(actual != expected)
    {
      printf(1, "XV6_TEST_OUTPUT : file is Incorrectly mapped\n");
      exit();
    }
  }
  printf(1, "XV6_TEST_OUTPUT : file is correctly mapped\n");



  //munmap
  rc = munmap(addr, map_size);
  if (rc < 0) 
  {
    printf(1, "XV6_TEST_OUTPUT : munmap failed\n");
    exit();
  }
  printf(1, "XV6_TEST_OUTPUT : munmap suceeded\n");



  // close file
  rc = close(fd);
  if(rc != 0)
  {
    printf(1, "XV6_TEST_OUTPUT : file close failed\n");
    exit();
  }
  printf(1, "XV6_TEST_OUTPUT : file close suceeded\n");
  
  
  exit();
}

void generate_data(char *data)
{
    for (int i = 0; i < FILE_SIZE; i++) {
        data[i] = i;
    }
}
int main22()
{
    char *filename = "test_file.txt";
    char data[FILE_SIZE];
    int fd, rc, offset;
    char *addr;

    printf(1, "XV6_TEST_OUTPUT : start\n");
    generate_data(data);

    printf(1, "XV6_TEST_OUTPUT : data creation suceeded\n");
    // Create file
    fd = open(filename, O_WRONLY | O_CREATE);

    if (fd < 0) {
        printf(1, "Error: cannot create file %s\n", filename);
        exit();
    }

    printf(1, "XV6_TEST_OUTPUT : file creation suceeded\n");
    rc = write(fd, data, FILE_SIZE);
    if (rc < 0) {
        printf(1, "Error: cannot write to file %s\n", filename);
        exit();
    }

    printf(1, "XV6_TEST_OUTPUT : writing file suceeded\n");
    close(fd);

    // Open file for read-write
    fd = open(filename, O_RDWR);
    if (fd < 0) {
        printf(1, "Error: cannot open file %s\n", filename);
        exit();
    }

    printf(1, "XV6_TEST_OUTPUT : file open suceeded\n");
    // mmap the file

    offset = 0x10;
    addr = mmap(0, MAP_SIZE, PROT_WRITE, MAP_FILE, fd, offset);
    if (addr<=0) {
        printf(1, "Error: cannot mmap file %s\n", filename);
        exit();
    }

    printf(1, "XV6_TEST_OUTPUT : perform msync suceeded\n");
    // Perform msync in a loop
    rc = msync(addr, MAP_SIZE);
    if (rc < 0) {
        printf(1, "Error: cannot msync file %s\n", filename);
        exit();
    }
    

    // Unmap the file and close it
    munmap(addr, MAP_SIZE);
    close(fd);

    printf(1, "Pressure test completed successfully.\n");
    exit();
}

int main33(int argc, char *argv[]) {
  // Allocate memory using mmap
  void *addr = mmap(0, PGSIZE,  PROT_WRITE, MAP_ANONYMOUS, -1, 0);
  if (addr <=0) {
    printf(1,"mmap failed\n");
    exit();
  }

  // Write some data to the allocated memory
  char *str = "hello, world!";
  strcpy((char*)addr, str);

  // Verify that the data is written correctly
  if (strcmp((char*)addr, str) != 0) {
    printf(1,"data verification failed\n");
    exit();
  }

  // Unmap the memory
  if (munmap(addr, PGSIZE) < 0) {
    printf(1,"munmap failed\n");
    exit();
  }

  // Verify that the memory is no longer accessible
  if (strcmp((char*)addr, str) == 0) {
    printf(1,"memory still accessible after munmap\n");
    exit();
  }

  printf(1,"test passed\n");
  exit();
}
