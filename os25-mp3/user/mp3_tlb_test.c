#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 由於移除了 kernel/defs.h，所以需要定義這些用戶態常數
#define MADV_DONTNEED 1
#define MADV_WILLNEED 2
#define MADV_NORMAL 0
#define PGSIZE 4096 

#define MAX_NPAGES 20  // 極限壓力測試頁面數
#define MAX_NPROCS 6   // 極限壓力測試程序數



// ======================================================================
// Test Case 1: 修改 swapped page 後立即讀取 (TLB 寫入測試)
// ======================================================================
int test_write_after_swap(void)
{
  printf("\n=== Test 1: Write after swap (TLB Write) ===\n");
  
  char *page = sbrk(PGSIZE);
  
  for(int i = 0; i < PGSIZE; i++){
    page[i] = i & 0xFF;
  }
  
  madvise((void *)page, PGSIZE, MADV_DONTNEED);
  
  // 寫入觸發 page fault
  page[0] = 0xAA;
  page[PGSIZE - 1] = 0xBB;
  
  // 立即讀取整頁
  if(page[0] != 0xAA || page[PGSIZE - 1] != 0xBB){
    printf("Test 1 FAIL: Write/Read mismatch at boundary.\n");
    sbrk(-PGSIZE);
    return -1;
  }
  
  // 檢查中間數據是否仍正確
  for(int i = 1; i < PGSIZE - 1; i++){
    if(page[i] != (i & 0xFF)){
      printf("Test 1 FAIL: Data corruption at offset %d.\n", i);
      sbrk(-PGSIZE);
      return -1;
    }
  }
  
  sbrk(-PGSIZE);
  printf("Test 1: PASS\n");
  return 0;
}

// ======================================================================
// Test Case 2: 多 process 同時存取 swapped pages (最大並發/競爭)
// ======================================================================
int test_concurrent_access(void)
{
  printf("\n=== Test 2: Concurrent access (Max Contention) ===\n");

  char *pages[MAX_NPAGES];
  
  // 分配頁面並寫入資料
  for(int i = 0; i < MAX_NPAGES; i++){
    pages[i] = sbrk(PGSIZE);
    if ((long)pages[i] == -1) {
        // 如果 sbrk 失敗，可能是記憶體不足，減少頁數繼續測試
        printf("WARN: Only allocated %d pages for Test 5.\n", i);
        break; 
    }
    for(int j = 0; j < PGSIZE; j++){
      pages[i][j] = i;
    }
  }
  int allocated_pages = (int)((long)sbrk(0) - (long)pages[0]) / PGSIZE;

  // Swap out 所有頁面
  for(int i = 0; i < allocated_pages; i++){
    madvise((void *)pages[i], PGSIZE, MADV_DONTNEED);
  }
  
  // Fork 多個 process 同時存取
  int pids[MAX_NPROCS];
  int active_procs = 0;

  for(int p = 0; p < MAX_NPROCS; p++){
    pids[p] = fork();
    if(pids[p] == 0){
      // Child: 隨機存取頁面
      for(int round = 0; round < 30; round++){ // 增加輪次
        int idx = (p * 11 + round * 5) % allocated_pages; // 增加隨機性
        if(pages[idx][0] != idx){
          printf("Child %d FAIL: page[%d][0] = %d (expected %d)\n", p, idx, pages[idx][0], idx);
          exit(1);
        }
      }
      exit(0);
    } else if (pids[p] > 0) {
      active_procs++;
    }
  }
  
  // Wait for all children
  int failed = 0;
  for(int p = 0; p < active_procs; p++){
    int status;
    wait(&status);
    if (status != 0) {
        failed = 1;
    }
  }

  sbrk(-PGSIZE * allocated_pages);

  if (failed) {
    printf("Test 2: FAIL (Child process failed)\n");
    return -1;
  }
  printf("Test 2: PASS\n");
  return 0;
}

// ======================================================================
// Test Case 3: 壓力測試 - 多次 swap in/out
// ======================================================================
int test_swap_stress(void)
{
  printf("\n=== Test 3: Swap stress test (Max Rounds) ===\n");

  char *page = sbrk(PGSIZE);
  
  for(int round = 0; round < 40; round++){ // 輪次加倍
    // 寫入資料
    for(int i = 0; i < PGSIZE; i++){
      page[i] = (round + i) & 0xFF;
    }
    
    // Swap out
    madvise((void *)page, PGSIZE, MADV_DONTNEED);
    
    // Swap in 並驗證
    for(int i = 0; i < PGSIZE; i++){
      if(page[i] != ((round + i) & 0xFF)){
        printf("Test 3 FAIL: round %d, offset %d\n", round, i);
        sbrk(-PGSIZE);
        return -1;
      }
    }
  }
  
  sbrk(-PGSIZE);
  printf("Test 3: PASS\n");
  return 0;
}

// ======================================================================
// Test Case 4: Fork TLB Consistency with COW Write (COW 隔離測試)
// ======================================================================
int test_fork_cow_write(void)
{
  printf("\n=== Test 4: Fork TLB with COW Write (Isolation) ===\n");

  char *page = sbrk(PGSIZE);
  *page = 0xAA; // 初始值
  
  // 必須 swap out 以確保 COW 在換入時發生
  madvise((void *)page, PGSIZE, MADV_DONTNEED); 
  
  int pid = fork();
  
  if(pid == 0){
    // Child: 嘗試寫入 (應觸發 Page Fault -> Swap In -> COW)
    *page = 0xCC; // 新值
    if(*page != 0xCC){
      printf("Child FAIL: Write failed. Got 0x%x\n", *page);
      exit(1);
    }
    exit(0);
  } else {
    // Parent
    wait(0);
    
    // 檢查父程序的值是否仍為 COW 前的舊值，以驗證隔離
    if(*page != 0xAA){ 
      printf("Parent FAIL: COW Isolation failure. Got 0x%x, expected 0xAA\n", *page);
      sbrk(-PGSIZE);
      return -1;
    }
  }
  
  sbrk(-PGSIZE);
  printf("Test 4: PASS\n");
  return 0;
}



// ======================================================================
// Main: 執行所有測試
// ======================================================================
int main(int argc, char *argv[])
{
  printf("==============================================\n");
  printf("  TLB Flush Tests for Swap Implementation\n");
  printf("==============================================\n");
  
  int failed = 0;

  if(test_write_after_swap() < 0) failed++;
  if(test_concurrent_access() < 0) failed++;
  if(test_swap_stress() < 0) failed++;
  if(test_fork_cow_write() < 0) failed++; // 新增 COW 寫入測試

  
  printf("\n==============================================\n");
  if(failed == 0){
    printf("  ALL TESTS PASSED! ✓\n");
  } else {
    printf("  %d TEST(S) FAILED! ✗\n", failed);
  }
  printf("==============================================\n");
  
  // 讓 main 函數返回，而不是呼叫 exit()，以避免與核心 defs.h 潛在的衝突
  return failed; 
}