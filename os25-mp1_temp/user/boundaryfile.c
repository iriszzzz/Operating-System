#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"   
#include "user/user.h"

#define BSIZE 1024

#define NDIRECT   11
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE   (NDIRECT + NINDIRECT + NINDIRECT * NINDIRECT)

void
write_blocks(int fd, int nblocks, char pattern)
{
  char buf[BSIZE];
  memset(buf, pattern, BSIZE);

  for(int i = 0; i < nblocks; i++){
    if(write(fd, buf, BSIZE) != BSIZE){
      printf("write failed at block %d\n", i);
      exit(1);
    }
  }
}

void
verify_blocks(int fd, int nblocks, char pattern)
{
  char buf[BSIZE];
  for(int i = 0; i < nblocks; i++){
    if(read(fd, buf, BSIZE) != BSIZE){
      printf("read failed at block %d\n", i);
      exit(1);
    }
    for(int j = 0; j < BSIZE; j++){
      if(buf[j] != pattern){
        printf("data mismatch at block %d byte %d\n", i, j);
        exit(1);
      }
    }
  }
}

int
main(void)
{
  int fd;
  unlink("boundaryfile");
  fd = open("boundaryfile", O_CREATE | O_RDWR);
  if(fd < 0){
    printf("open failed\n");
    exit(1);
  }
  write_blocks(fd, NDIRECT, 'A');
  write_blocks(fd, 1, 'B');
  write_blocks(fd, NINDIRECT - 1, 'C');
  write_blocks(fd, 1, 'D');
  //write_blocks(fd, NINDIRECT * NINDIRECT - 1, 'E');
  write_blocks(fd, 10, 'E');
  close(fd);

  fd = open("boundaryfile", O_RDONLY);
  if(fd < 0){
    printf("reopen failed\n");
    exit(1);
  }

  verify_blocks(fd, NDIRECT, 'A');
  verify_blocks(fd, 1, 'B');
  verify_blocks(fd, NINDIRECT - 1, 'C');
  verify_blocks(fd, 1, 'D');
  // verify_blocks(fd, NINDIRECT * NINDIRECT - 1, 'E');
  verify_blocks(fd, 10, 'E');
  close(fd);

  printf("boundaryfile: ok\n");
  exit(0);
}

