#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "mp2-mfqs.h"

#define L3_MAX     49      // 0..49 放 L3（RR）
#define L2_MIN     50      // 50..99 放 L2（Priority）
#define L2_MAX     99
#define L1_MIN     100     // >=100 放 L1（SJF）
#define RR_QUANTUM 10      // L3 quantum

static struct sortedproclist l1q; // L1：PSJF）→ 用排序佇列
static struct sortedproclist l2q; // L2：Priority）→ 用排序佇列
static struct proclist      l3q;  // L3：RR）→ 用一般佇列


static int cmp_l1(struct proc *a, struct proc *b) {
int remain_a = a->est_burst - a->psjf_T;  // 剩餘時間
  int remain_b = b->est_burst - b->psjf_T;
  
  if (remain_a == remain_b) return (b->pid - a->pid);  // pid 小者先
  return remain_b - remain_a;  // 剩餘時間小者先           
}

static int cmp_l2(struct proc *a, struct proc *b) {
  if (a->priority == b->priority) return (b->pid - a->pid); // pid 小者先 → 正數
  return a->priority - b->priority;                         // a 比 b 大 → 正數
}

// static inline int level_of(struct proc *p){
int level_of(struct proc *p){
  if(p->priority >= L1_MIN) return 1;
  if(p->priority >= L2_MIN && p->priority <= L2_MAX) return 2;
  return 3;
}

// 準備三條隊伍（L1/L2/L3）
void mfqs_init(void){
  initsortedproclist(&l1q, cmp_l1);
  initsortedproclist(&l2q, cmp_l2);
  initproclist(&l3q);
}

// 把「已經是 RUNNABLE」的行程丟進正確隊伍
void mfqs_enqueue(struct proc *p){
    struct proclistnode *pn;
    pn = allocproclistnode(p);
    if(pn == 0)
        panic("mfqs_enqueue: no proclistnode");
    int level = level_of(p);
    if(level == 1){
        pushsortedproclist(&l1q, pn);
    }else if(level == 2){
        pushsortedproclist(&l2q, pn);
    }else{
        p->rr_budget = RR_QUANTUM; // L3 新來的行程
        pushbackproclist(&l3q, pn);
    }
}

// 依 L1 > L2 > L3 拿出要跑的人給 scheduler
struct proc* mfqs_dequeue(void){
    struct proclistnode *pn;    
    // 1) L1_SJF
    if((pn = popsortedproclist(&l1q))) {
      struct proc *p = pn->p;   // 拿到真正的行程指標
      freeproclistnode(pn);     // 用完節點，記得釋放（node 只是殼）
      return p;                 // 交給 scheduler 跑
    }   
    // 2) else L2_Priority
    if((pn = popsortedproclist(&l2q))) {
      struct proc *p = pn->p;
      freeproclistnode(pn);
      return p;
    }   
    // 3) else L3_RR
    if((pn = popfrontproclist(&l3q))) {
      struct proc *p = pn->p;
      freeproclistnode(pn);
      return p;
    }   
    // 4) all_none -> 0
    return 0;
}



// L3：每個 tick 扣一次量子；用完要讓位（回 ready）
void mfqs_rr_on_tick(struct proc *p) {
  if (level_of(p) == 3 && p->rr_budget > 0) p->rr_budget--;
}
int  mfqs_rr_timeslice_up(struct proc *p){
    return (level_of(p) == 3 && p->rr_budget <= 0);
}
// l2
int mfqs_l2_nonempty(void) {
    return sizesortedproclist(&l2q) > 0;
}
// L1 preempt
int mfqs_l1_nonempty(void) {
    return sizesortedproclist(&l1q) > 0;
}
int mfqs_l1_top_preempt(struct proc *p) {
    int r = cmptopsortedproclist(&l1q, p);
    return (r < 0); //top更優先
}

void mfqs_update_est_burst(struct proc *p){ //only at Running -> Waiting
    if(level_of(p) != 1) return;
    p->est_burst = (p->est_burst + p->psjf_T) / 2;   // t_i = ⌊(T + t_{i-1})/2⌋
    p->psjf_T = 0;
}

void mfqs_remove(struct proc *p) { // Add
  int level = level_of(p);
  struct proclistnode *pn;

  if (level == 1) {
    pn = findsortedproclist(&l1q, p);
    if (pn) {
      removesortedproclist(&l1q, pn);
      freeproclistnode(pn);
    }
  } else if (level == 2) {
    pn = findsortedproclist(&l2q, p);
    if (pn) {
      removesortedproclist(&l2q, pn);
      freeproclistnode(pn);
    }
  } else {
    pn = findproclist(&l3q, p);
    if (pn) {
      removeproclist(&l3q, pn);
      freeproclistnode(pn);
    }
  }
}

// void mfqs_update_queue(struct proc *p) { // Add
//   mfqs_remove(p);  // remove from old queue

//   // Update queue_level
//   if (p->priority >= L1_MIN) {
//     p->queue_level = 2;
//   } else if (p->priority >= L2_MIN) {
//     p->queue_level = 1;
//   } else {
//     p->queue_level = 0;
//   }

//   mfqs_enqueue(p);  // re-enqueue into correct queue
// }

struct proclistnode* findsortedproclist(struct sortedproclist *pl, struct proc *p) { // Add
  struct proclistnode *cur = pl->head->next;
  while (cur != pl->tail) {
    if (cur->p == p)
      return cur;
    cur = cur->next;
  }
  return 0;
}

void removesortedproclist(struct sortedproclist *pl, struct proclistnode *pn) { // Add
  if (!pn || pn == pl->head || pn == pl->tail)
    return;
  pn->prev->next = pn->next;
  pn->next->prev = pn->prev;
  pl->size--;
}
