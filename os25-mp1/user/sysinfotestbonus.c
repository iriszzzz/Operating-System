#include "kernel/types.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

void
testforkburst() {
  struct sysinfo info;
  int maxchildren = 128;
  int pids[maxchildren];
  int i;

  sysinfo(&info);
  int baseNproc = info.nproc;

  for (i = 0; i < maxchildren; i++) {
    int pid = fork();
    if (pid < 0) {
      // End if fork fails (resources exhausted)
      printf("testforkburst: fork failed at %d\n", i);
      break;
    } else if (pid == 0) {
      sleep(100);  // Child process lives for a while
      exit(0);
    } else {
      pids[i] = pid;
    }
  }

  sysinfo(&info);

  if (info.nproc < baseNproc + i) {
    printf("testforkburst: FAIL: expected nproc >= %d, got %d\n", baseNproc + i, info.nproc);
    exit(1);
  }
  for (int j = 0; j < i; j++) {
    wait(0);
  }

  printf("testforkburst: OK (forked %d children)\n", i);
}

void
testforktreerecursive(int depth) {
  if (depth <= 0)
    return;

  int pid = fork();
  if (pid < 0) {
    printf("testforktree: fork failed at depth %d\n", depth);
    exit(1);
  } else if (pid == 0) {
    sleep(50);  // Child process sleeps
    testforktreerecursive(depth - 1);
    exit(0);
  } else {
    wait(0);
  }
}

void
testforktree() {
  struct sysinfo info;
  sysinfo(&info);
  int base = info.nproc;

  testforktreerecursive(5); // Create a tree of depth 5

  sysinfo(&info);
  if (info.nproc != base) {
    printf("testforktree: FAIL: expected nproc = %d, got %d\n", base, info.nproc);
    exit(1);
  }

  printf("testforktree: OK\n");
}

int
main(int argc, char *argv[])
{
  printf("sysinfotestbonus: start\n");
  testforkburst();
  testforktree();
  printf("sysinfotestbonus: OK\n");
  exit(0);
}
