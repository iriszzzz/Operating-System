# MP1 SPEC
## Introduction
* This assignment is designed to deepen your understanding of a core operating system mechanism: the **system call**, which serves as the fundamental interface between user programs and the kernel.
* You will extend the functionality of the xv6 kernel by implementing two new system calls:
	1. **trace**: A practical debugging utility that will trace and print the system calls executed by a specific program, allowing you to visualize the interactions between an application and the kernel.
	2. **sysinfo**: An informational tool that will collect and report on the current state of the system, including available memory and the number of running processes.
### Instructions
Refer to link [student guide](https://git.lsalab.cs.nthu.edu.tw/os25/os25_shared_xv6/src/branch/student/) to launch the development environment.
## Trace Code
* Explain how does a user program trigger system call (take `read` for example)
* You can refer to Chapter 2 and Sections 4.3 and 4.4 of Chapter 4 of [xv6 handbook](https://pdos.csail.mit.edu/6.828/2023/xv6/book-riscv-rev3.pdf#page=54.62).
	1. `user/grep.c/read()`
	2. `user/usys.S/read`
	3. `kernel/trampoline.S/uservec`
	4. `kernel/trap.c/usertrap()`
	5. `kernel/syscall.c/syscall()`
	6. `kernel/sysfile.c/sys_read()`
## Implementation
### 1. Add a new system call: trace
> In this assignment you will add a system call tracing feature that may help you when debugging.
> 
> It should take one argument, an integer "mask", whose bits specify which system calls to trace. For example, to trace the fork system call, a program calls `trace(1 << SYS_fork)`, where `SYS_fork` is a syscall number from `kernel/syscall.h`.
> 
> You have to modify the xv6 kernel to print out a line when each system call is about to return, if the system call's number is set in the mask. The line should contain the process id, the name of the system call and the return value; you don't need to print the system call arguments.
> 
> The trace system call should enable tracing for the process that calls it and any children that it subsequently forks.
#### Example
* We provide a `trace` user-level program that runs another program with tracing enabled (see `user/trace.c`). When you're done, you should see output like this:

```
$ trace 32 grep hello README
3: syscall read -> 1023
3: syscall read -> 961
3: syscall read -> 321
3: syscall read -> 0
$ 
$ trace 2147483647 grep hello README
5: syscall trace -> 0
5: syscall exec -> 3
5: syscall open -> 3
5: syscall read -> 1023
5: syscall read -> 961
5: syscall read -> 321
5: syscall read -> 0
5: syscall close -> 0
$ 
$ grep hello README
$ 
$ trace 2 usertests forkforkfork
usertests starting
9: syscall fork -> 10
test forkforkfork: 9: syscall fork -> 11
11: syscall fork -> 12
12: syscall fork -> 13
12: syscall fork -> 14
13: syscall fork -> 15
14: syscall fork -> 16
12: syscall fork -> 17
13: syscall fork -> 18
...
$
```
* In the first example above, trace invokes grep tracing just the read system call. The 32 is `1<<SYS_read`. 
* In the second example, trace runs grep while tracing all system calls; the 2147483647 has all 31 low bits set. 
* In the third example, the program isn't traced, so no trace output is printed. 
* In the fourth example, the fork system calls of all the descendants of the forkforkfork test in usertests are being traced. Your solution is correct if your program behaves as shown above (though the process IDs may be different).
#### Hints
1. Add `$U/_trace` to `UPROGS` in Makefile.
2. Run `make qemu` and you will see that the compiler cannot compile `user/trace.c`, because the user-space stubs for the system call don't exist yet: add a prototype for the system call to `user/user.h`, a stub to `user/usys.pl`, and a syscall number to `kernel/syscall.h`. The Makefile invokes the perl script `user/usys.pl`, which produces `user/usys.S`, the actual system call stubs, which use the RISC-V ecall instruction to transition to the kernel. Once you fix the compilation issues, run `trace 32 grep hello README`; it will fail because you haven't implemented the system call in the kernel yet.
3. Add a `sys_trace()` function in `kernel/sysproc.c` that implements the new system call by remembering its argument in a new variable in the proc structure (see `kernel/proc.h`). The functions to retrieve system call arguments from user space are in `kernel/syscall.c`, and you can see examples of their use in `kernel/sysproc.c`.
4. `sys_trace()` should return 0 if it executes successfully, otherwise, return -1.
5. Modify `fork()` (see `kernel/proc.c`) to copy the trace mask from the parent to the child process.
6. Modify the `syscall()` function in `kernel/syscall.c` to print the trace output. You will need to add an array of syscall names to index into.
### 2. Add a new system call: sysinfo
> In this assignment you will add a system call, `sysinfo`, that collects information about the running system. The system call takes one argument: a pointer to a `struct sysinfo` (see `kernel/sysinfo.h`).
> 
> The kernel should fill out the fields of this struct: the `freemem` field should be set to the number of bytes of free memory, and the `nproc` field should be set to the number of processes whose state is **not** `UNUSED`.
> 
> We provide a test program `sysinfotest`.
#### Hints
1. Add `$U/_sysinfotest` to `UPROGS` in Makefile
2. Run `make qemu`; `user/sysinfotest.c` will fail to compile. Add the system call `sysinfo`, following the same steps as in the previous assignment. To declare the prototype for `sysinfo()` in `user/user.h` you need predeclare the existence of `struct sysinfo`:
```c=
struct sysinfo;
int sysinfo(struct sysinfo *);
```
3. Once you fix the compilation issues, run `sysinfotest`; it will fail because you haven't implemented the system call in the kernel yet.
4. `sysinfo` needs to copy a `struct sysinfo` back to user space; see `sys_fstat()` (`kernel/sysfile.c`) and `filestat()` (`kernel/file.c`) for examples of how to do that using `copyout()`.
5. `sysinfo` should return 0 if it executes successfully, otherwise, return -1.
6. Make sure `sysinfo` could be traced by the system call `trace`.
7. To collect the amount of free memory, add a function to `kernel/kalloc.c`
8. To collect the number of processes, add a function to `kernel/proc.c`
9. If you want to call these two new functions in `kernel/sysproc.c`, you may need to declare them in `kernel/defs.h`.
## Bonus: Design your own test case
* Design your own test cases to test your code. Describe your test cases in your report to earn points. You can select just a few functions to test, and your score will be determined by how complete your test cases are. 
## Grading
1. Implementation correctness (60%)
	* Public testcases (42%): you can execute `./grade-mp1-public` to test your code.
	* Private testcases (18%)
2. Report (20%)
	* The report must detail team member contributions, provide an explanation of the trace code, and offer a clear explanation of your implementation.
	* Name the report `mp1-report.md`, and upload it to repo.
3. Demo (20%)
	* You will have a 15-minute session to answer TA questions regarding your implementation and specific details of the trace code.
4. Bonus (5%)
	* Refer to section "Rule for bonus" in [student guide](https://git.lsalab.cs.nthu.edu.tw/os25/os25_shared_xv6/src/branch/student/)
	* Write the bonus testcase script in `grade-mp1-bonus`.
5. Plagiarism check
    * Never show your code to others.
    * If your code is found to be similar to others, including sources from the internet, and you cannot answer questions properly about your code during the demo, you will be considered as plagiarizing and will receive a score of **0** for the assignment.
## References
1. [xv6: a simple, Unix-like teaching operating system](https://pdos.csail.mit.edu/6.1810/2024/xv6/book-riscv-rev4.pdf)
2. [MIT xv6 labs](https://pdos.csail.mit.edu/6.828/2021/reference.html)
3. [RISC-V Instruction Set Specifications](https://msyksphinz-self.github.io/riscv-isadoc/html/index.html)