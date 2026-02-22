// clang-format off
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
// clang-format on

#define NULL 0
#define NPROCS 2

int priorities[NPROCS] = {
    10,
    60,
};

int multiplier[NPROCS] = {
    20,
    1,
};

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(2, "Usage: mp2-preempt-b <workload>\n");
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
    } else if (i == 0) {
      // Parent process sleeps for a short time
      sleep(10);
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
    simulate_work(workload * multiplier[cid]);
  }

  exit(0);
}
