// clang-format off
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
// clang-format on

#define NULL 0
#define NPROCS 3

// All processes in L1 queue (100-149)
int priorities[NPROCS] = {
    120,  // Process 0
    100,  // Process 1
    120,  // Process 2
};

// Different number of bursts for each process
int bursts[NPROCS] = {
    3,  // Long burst
    3,  // Medium burst
    3,  // Short burst
};

// Different multipliers of bursts for each process
int multiplier[NPROCS] = {
    10,  // Long burst
    5,   // Medium burst
    1,   // Short burst
};

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(2, "Usage: mp2-psjf <workload>\n");
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
      proclog(i);
      if (i < bursts[cid] - 1) {
        // Sleep for a short time between bursts
        sleep(1);
      }
    }
  }

  exit(0);
}
