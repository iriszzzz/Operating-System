// clang-format off
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
// clang-format on

#define NULL 0
#define NPROCS 3

// All processes in L3 queue (0-49)
int priorities[NPROCS] = {
    0,  // Process 0
    0,  // Process 1
    0,  // Process 2
};

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(2, "Usage: mp2-rr <workload>\n");
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
