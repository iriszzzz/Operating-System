// clang-format off
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
// clang-format on

#define NULL 0
#define NPROCS 3

// All processes in L2 queue (50-99)
// Different priorities to test priority scheduling
int priorities[NPROCS] = {
    50,  // Process 0: Lowest priority in L2
    75,  // Process 1: Medium priority in L2
    95   // Process 2: Highest priority in L2
};

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(2, "Usage: mp2-priority <workload>\n");
    exit(1);
  }
  int i, cid;
  int pids[NPROCS];
  int workload = atoi(argv[1]);

  cid = -1;
  for (i = 0; i < NPROCS; i++) {
    pids[i] = priorfork(priorities[i], 1);
    if (pids[i] < 0) {
      fprintf(2, "priorfork failed\n");
      exit(1);
    } else if (pids[i] == 0) {
      cid = i;
      break;
    }
  }

  if (cid == -1) {
    // Parent
    for (i = 0; i < NPROCS; i++) {
      wait(NULL);
    }
    proclog(-1);
  } else {
    // Child
    simulate_work(workload * 5);  // Simulate some work
  }

  exit(0);
}
