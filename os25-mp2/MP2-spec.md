# MP2 Scheduler

## Introduction

- This assignment is designed to help you understand concepts of process scheduling in an operating system.
- You will extend the base round-robin scheduler with time quantum of 10 ticks in the modified xv6 operating system to implement a multilevel feedback queue scheduler.

### Modifications (optional reading)

In mp2, we have modified the original xv6-riscv

- Added process list
- Added sorted process list
- Added priorfork system call to fork a process with a specified initial priority
- Added proclog system call to log state of process execution
- Decreased time interval of timer interrupt from 1000000 cycles(about 1/10 second) to 10000 cycles(about 1 millisecond)

### Instructions

- Refer to link [student guide](https://git.lsalab.cs.nthu.edu.tw/os25/os25_shared_xv6/src/branch/student/) to launch the development environment.

## Trace Code

- Explain how timer interrupt works in xv6, including how xv6 switches between user space interrupt handler and kernel space interrupt handler. Refer to [xv6 handbook section 5.4](https://pdos.csail.mit.edu/6.828/2023/xv6/book-riscv-rev3.pdf#page=54.62).
  1. Setup timer interrupt
     - `timerinit` -> `kernel/kernelvec.S:timervec`
  2. User space interrupt handler
     - `usertrapret` -> `kernel/trampoline.S:uservec` -> `usertrap` -> `devintr` -> `clockintr`
  3. Kernel space interrupt handler
     - `usertrap` -> `kernel/kernelvec.S:kernelvec` -> `kerneltrap` -> `devintr` -> `clockintr`
- Find out the mapping relationship of `kernel/proc.h` `enum procstate` to the process states in the lecture notes ("New", "Ready", "Running", "Waiting", "Terminated").
- Then explain what each functions in the state transition function path does in the context of process state transitions. (Note that it is also important to explain the interaction of the waiting queues and ready queues, how scheduling works, and how context switching works in xv6.)
  1. New -> Ready
     - `userinit` -> `allocproc` -> `pushreadylist`
     - `fork` or `priorfork` -> `allocproc` -> `pushreadylist`
  2. Running -> Ready
     - `kerneltrap`, `usertrap` -> `yield` -> `pushreadylist` -> `sched` -> `kernel/switch.S:swtch`
  3. Running -> Waiting (Consider the case of `sleep` system call)
     - `sys_sleep` -> `sleep` -> `sched`
  4. Waiting -> Ready
     - `clockintr` -> `wakeup`
  5. Running -> Terminated
     - `sys_exit` -> `exit` -> `sched`
  6. Ready -> Running
     - `scheduler` -> `kernel/switch.S:swtch` -> `popreadylist` -> `kernel/switch.S:swtch`

## Implementation

Multilevel feedback queue scheduler

Process and queue priority:

- L1, L2, L3
- L1 is the highest priority
- L3 is the lowest priority
- Each process has a scheduling priority from 0 to 149
- 0 to 49 is L3
- 50 to 99 is L2
- 100 to 149 is L1

L1 Queue:

- Preemptive SJF(shortest job first)
- Contains processes with priority 100 to 149
- $t_i$: integer type, the i-th approximated burst time of the process
- $T$: integer type, the total running ticks within a CPU burst
- $t_i - T$: integer type, the current approximated remaining burst time
- $t_i = \lfloor(T + t_{i-1}) / 2\rfloor, \quad i > 0, t_0 = 0$: integer type
- The process with the smallest $t_i - T$ is scheduled first
- Update timing:
  - Update the approximated burst time when a process changes from running to waiting state, and set $T$ to $0$
  - Accumulate $T$ if and only if the process is in running state
  - $T$ should be stop accumulating when the process is in ready state, and resume accumulating when the process is in running state again
- **Tie breaker**:
  - If two processes have the same $t_i - T$, the one with the smaller pid is scheduled first

L2 Queue:

- Non-preemptive Priority Scheduling
- Contains processes with priority 50 to 99
- **Tie breaker**:
  - If two processes have the same priority, the one with the smaller pid is scheduled first

L3 Queue:

- Round-robin scheduling
- Contains processes with priority 0 to 49
- With time slice of 10 ticks (or time quantum of 10 ticks)
- When each process is joined to the L3 queue, it is append to the end of the queue

Preemption between queues:

- All L1 processes preempt L2 and L3 processes
- All L2 processes preempt L3 processes

Aging:

- The priority of a process is increased by 1 for every 20 ticks it has been waiting in the ready queue
- Cap each priority in range $[0, 149]$
- Remember to move the process to the appropriate queue when its priority changes

### Rules

- DO NOT modify or add any calls to `procstatelog`

## Grading

1. Implementation correctness (60%)
   - Correctness of l3 queue scheduling
   - Correctness of l2 queue scheduling
   - Correctness of l1 queue scheduling
   - Correctness of multilevel feedback queue scheduling
   - Correctness of aging
   - Correctness of preemption
   - Passing all public testcases gets (60% * 70% = 42%). The test case will be same as `./grade-mp2-public`.
   - Passing the remaining private testcases gets the rest (60% * 30% = 18%)
2. Report (20%)
   - Section including team members, team member contribution.
   - Section answering the questions/explanation of the trace code section.
   - Section explaining your implementation.
   - Name the report `mp2-report.md`, and push to the repo.
3. Demo (20%)
   - You will have a 20-minute session answering TA questions regarding your implementation and specific details of the trace code.
4. Bonus (5%)
   - Refer to section "Rule for bonus" in [student guide](https://git.lsalab.cs.nthu.edu.tw/os25/os25_shared_xv6/src/branch/student/)
   - Write the bonus testcase script in `grade-mp2-bonus`.
   - Testcase for mp2 is more complicated, read `grade-mp2-public` once again before you start.
5. Plagiarism check
   - Never show your code to others.
   - If your code is found to be similar to others, including sources from the internet, and you cannot answer questions properly about your code during the demo, you will be considered as plagiarizing and will receive a score of 0 for the assignment.

## References

1. [xv6: a simple, Unix-like teaching operating system](https://pdos.csail.mit.edu/6.828/2022/xv6/book-riscv-rev3.pdf)
2. [MIT xv6 labs](https://pdos.csail.mit.edu/6.828/2022/reference.html)
3. [RISC-V Instruction Set Specifications](https://msyksphinz-self.github.io/riscv-isadoc/html/index.html)
4. Operating Systems Course in National Taiwan University
