// mp2-l1-preempt.c  —— L1 arrival preempts a running L1 (no waiting involved)
// clang-format off
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
// clang-format on

#define NULL 0

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(2, "Usage: mp2-l1-preempt <workload>\n");
    exit(1);
  }
  int workload = atoi(argv[1]);

  int a = priorfork(/*priority=*/120, /*init_est_hint=*/1000); // A：L1、長
  if (a < 0) { fprintf(2, "priorfork A failed\n"); exit(1); }
//   if (a == 0) {
//    
//     for (int i = 0; i < workload * 1000; i++) {
//     volatile int x = i * i;
//     (void)x; // 
//     }
//     exit(0);
//   }
    if (a == 0) {
        simulate_work(workload * 10);
        sleep(1);
        simulate_work(workload * 20);
        sleep(1);
        simulate_work(workload * 5);
        exit(0);
    }


  int b = priorfork(/*priority=*/120, /*init_est_hint=*/5);   // B：同在 L1，短
  if (b < 0) { fprintf(2, "priorfork B failed\n"); exit(1); }
  if (b == 0) {
    simulate_work(workload * 5);
    sleep(1);
    simulate_work(workload * 5);
    exit(0);
  }

  wait(NULL);
  wait(NULL);
  proclog(-1);
  exit(0);
}
