// clang-format off
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
// clang-format on

#define NULL 0
#define NPROCS 4

int priorities[NPROCS] = {
    46,   // Process 0
    46,   // Process 1
    99,   // Process 2
    149,  // Process 3
};

int bursts[NPROCS] = {
    3,  // Process 0
    3,  // Process 1
    3,  // Process 2
    2,  // Process 3
};

int multiplier[NPROCS] = {
    2,  // Process 0
    2,  // Process 1
    1,  // Process 2
    5,  // Process 3
};

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(2, "Usage: mp2-aging <workload>\n");
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
    for (i = 0; i < bursts[cid]; i++) {
      // Each process does multiple short bursts
      simulate_work(workload * multiplier[cid]);  // Simulate some work
      if (i < bursts[cid] - 1) {
        sleep(1);
      }
    }
  }

  exit(0);
}
