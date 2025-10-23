// mp2_mfqs.h
#ifndef MP2_MFQS_H
#define MP2_MFQS_H

struct proc;

void mfqs_init(void);
void mfqs_enqueue(struct proc *p);
struct proc* mfqs_dequeue(void);
void mfqs_rr_on_tick(struct proc *p);
int mfqs_rr_timeslice_up(struct proc *p);
int level_of(struct proc *p);
int mfqs_l1_nonempty(void);
int mfqs_l1_top_preempt(struct proc *p);
int mfqs_l2_nonempty(void);
int mfqs_update_est_burst(struct proc *p);

#endif // MP2_MFQS_H
