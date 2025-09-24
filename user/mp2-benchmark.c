// clang-format off
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
// clang-format on

#define NULL 0

int min(int a, int b) {
  return (a < b) ? a : b; // Return the minimum of two integers
}

/**
 * @brief Return the minimum time taken to complete a workload
 *
 * @param iterations Number of repetitions to run the benchmark
 * @param workload Amount of work to simulate
 * @return int Benchmarked time in ticks
 */
int benchmark_workload(int iterations, int workload) {
  int iteration, start_time, min_time;
  min_time = __INT_MAX__;
  for (iteration = 0; iteration < iterations; iteration++) {
    start_time = uptime();                           // Get the start time
    simulate_work(workload);                         // Simulate some work
    min_time = min(min_time, uptime() - start_time); // Aggregate the time taken
  }
  return min_time;
}

/**
 * @brief Binary search to find the precise workload
 *
 * @param iterations Number of repetitions to run the benchmark
 * @param low Minimum workload to consider
 * @param high Maximum workload to consider
 * @return int The minimum workload that takes at least 10 ticks
 */
int binary_search_workload(int iterations, int low, int high) {
  if (benchmark_workload(iterations, low) >= 10) {
    return low;
  }
  while (low <= high) {
    int mid = low + (high - low) / 2;
    if (benchmark_workload(iterations, mid) >= 10) {
      high = mid; // Try to find a smaller workload that also satisfies the
                  // condition
    } else {
      low = mid + 1; // The workload is too small, so we need to increase it
    }
  }
  return low;
}

int main(int argc, char *argv[]) {
  int low, high, result;
  // Exponential search to find an rough lower and upper bound
  for (low = 1, high = 1; high <= (1 << 30); high *= 2) {
    if (benchmark_workload(10, high) >= 10) {
      // Found a workload that takes at least 10 ticks
      break;
    }
    low = high;
  }

  // Binary search to find the precise minimum workload within the bounds
  result = binary_search_workload(10, low, high);

  proclog(result); // Log the found workload
  exit(0);
}
