# MP2 report 
  112062527 陳俞靜
  114065501 蔡宛秦

## Table of contents

## Trace Code

- CSR （控制暫存器）: 
     - sstatus：狀態暫存器（S-mode 版）
     - sepc : 儲存 trap 前的 PC，回到 user mode 時要從這裡繼續（usertrapret 寫回）
     - stvec : Trap 入口點位址，指定中斷發生時跳去哪（設成 uservec 或 kernelvec）
     - sscratch：暫存暫存器（S-mode），存放 user stack pointer（在 trampoline.S 用）。
     - satp：Page table 根指標，控制虛擬記憶體（切換 user/kernel page table）
     - sie：Interrupt Enable，哪些中斷允許（timer、software、external）
     - sip：Interrupt Pending，哪些中斷正在等待處理（bit1=SSIP），每一個 bit 代表不同類型的中斷是否「掛起」（正在等待被處理）：
       > 0：USIP，使用者模式軟體中斷\
       > 1：SSIP，主管（Supervisor）模式軟體中斷，sip裡的一個位元（bit 1）。\
       > 5：STIP，定時器中斷（Timer interrupt）\
       > 9：SEIP，外部裝置中斷（External interrupt）
     - scause：為什麼 CPU 進入 trap」（例如外部中斷、timer、syscall...）
     - mstatus：Machine 狀態暫存器，M-mode 的版本，比 sstatus 權限更高
     - mie：Machine Interrupt Enable，M-mode 開關各種中斷
     - mip：Machine Interrupt Pending，M-mode 正在等待的中斷
     - mtvec：M-mode Trap 向量位址，設定 machine trap 入口（例如 timervec）
     - mscratch：暫存暫存器（M-mode），在 timervec 裡暫存暫存器內容

### 1. `kernel/kernelvec.S:timervec`
 - Setup timer interrupt
 - `timerinit` -> `kernel/kernelvec.S:timervec`
 - 是 RISC-V 的 machine-mode timer interrupt handler，負責設定下一次中斷的時間，然後轉交控制權給 xv6 的 S-mode。
   > 當 CPU 從某個模式跳到另一個模式時（例如 user → kernel、或 machine → supervisor），
暫存器的內容會被破壞，但我們又需要先暫存一些值\
   > `sscratch`: S-mode 暫存暫存器\
   > `mscratch`: U-mode 暫存暫存器


   ```asm
   .globl timervec
   .align 4
   timervec:
   ```
  - 宣告全域標籤 timervec，4 位元組對齊。這是硬體在 machine mode timer interrupt 時跳入的入口。
    ```asm
    # start.c has set up the memory that mscratch points to:
    # scratch[0,8,16] : register save area.
    # scratch[24] : address of CLINT's MTIMECMP register.
    # scratch[32] : desired interval between interrupts.
    ```
  - 在 start.c 啟動時，xv6 會設定 mscratch 指向一個暫存區：
    ```asm
    csrrw a0, mscratch, a0
    sd a1, 0(a0)
    sd a2, 8(a0)
    sd a3, 16(a0)
    ```

   - `csrrw` : `CSR Read and Write` 從 CSR（控制暫存器）讀一個值、同時把新值寫回去。
     > 取得 mscratch 裡儲存的 scratch 區基址（給 a0 用）
同時暫存目前的 a0 值（放回 mscratch），以免 a0 被破壞
  

     ```asm 
     # schedule the next timer interrupt
     # by adding interval to mtimecmp.
     ld a1, 24(a0) # CLINT_MTIMECMP(hart)
     ld a2, 32(a0) # interval
     ld a3, 0(a1)  # 取出目前的 mtimecmp ： 「下次中斷的時間點」
     add a3, a3, a2 # 加上間隔值，得到下一次觸發時間
     sd a3, 0(a1) # 寫回 mtimecmp
 
     # arrange for a supervisor software interrupt
     # after this handler returns.
     li a1, 2 # 把常數 2 載入 a1，表示要設定 bit 1 = 1（二進位）。
     csrw sip, a1 # Write to CSR，寫進指定的控制暫存器（這裡是 sip）
     #「我這邊（machine mode）已經處理完下一次時間設定，請作業系統（S-mode）等會處理 supervisor interrupt。」
 
     ld a3, 16(a0)
     ld a2, 8(a0)
     ld a1, 0(a0)
     csrrw a0, mscratch, a0 # 把原本的 a0 值放回
 
     mret # 從 machine mode 回到觸發前的模式
     ```


### 2. `kernel/trampoline.S:uservec`
 - User space interrupt handler `為什麼要分 user 與 kernel handler`
 - `usertrapret` -> `kernel/trampoline.S:uservec` -> `usertrap` -> `devintr` -> `clockintr`
#### I. `usertrapret`
 - 前言：怎麼到`usertrapret`？
   > `usertrap()`裡最後會呼叫`usertrapret`，而每次進入`trampoline.S`裡的`uservec`都在結尾跳入`usertrap()`\
   > 每次在usertrap裡執行完任務後，回 user mode 前都會執行一次 usertrapret()，它會設定下次中斷時該跳回 uservec。\
   > 從上次設好的返回點（usertrapret）開始，
中斷發生 → 進 uservec → usertrap → devintr → clockintr → 再回 usertrapret

   ```c
   //
   // return to user space
   //
   void
   usertrapret(void)
   {
     struct proc *p = myproc();
   
     intr_off();
   
     // send syscalls, interrupts, and exceptions to uservec in    trampoline.S
     uint64 trampoline_uservec = TRAMPOLINE + (uservec -    trampoline);
     w_stvec(trampoline_uservec);
   ```
 - `TRAMPOLINE` 是一個固定高位址，`uservec - trampoline` 是在計算出組譯時 uservec 在 trampoline 裡的相對位移，加上 TRAMPOLINE（整塊放到記憶體的起點），就得到 uservec 的實際執行位址。
 - `w_stvec` 把這個位址寫入 stvec。


   ```c
   
     // set up trapframe values that uservec will need when
     // the process next traps into the kernel.
     p->trapframe->kernel_satp = r_satp();         // kernel    page table
     p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's    kernel stack
     p->trapframe->kernel_trap = (uint64)usertrap;
     p->trapframe->kernel_hartid = r_tp();         // hartid    for cpuid()
   
     // set up the registers that trampoline.S's sret will use
     // to get to user space.
     
     // set S Previous Privilege mode to User.
     unsigned long x = r_sstatus();
     x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
     x |= SSTATUS_SPIE; // enable interrupts in user mode
     w_sstatus(x);
   
     // set S Exception Program Counter to the saved user pc.
     w_sepc(p->trapframe->epc);
   
     // tell trampoline.S the user page table to switch to.
     uint64 satp = MAKE_SATP(p->pagetable);
   
     // jump to userret in trampoline.S at the top of memory,    which 
     // switches to the user page table, restores user    registers,
     // and switches to user mode with sret.
     uint64 trampoline_userret = TRAMPOLINE + (userret -    trampoline);
     ((void (*)(uint64))trampoline_userret)(satp);
   }
   ```
#### II. `kernel/trampoline.S:uservec`
   ```asm
   #include "riscv.h"
   #include "memlayout.h"

   .section trampsec
   .globl trampoline
   trampoline:
   .align 4
   .globl uservec
   uservec:    
    #
    # trap.c sets stvec to point here, so
    # traps from user space start here,
    # in supervisor mode, but with a
    # user page table.
    #

    # save user a0 in sscratch so
    # a0 can be used to get at TRAPFRAME.
    csrw sscratch, a0

    # each process has a separate p->trapframe memory area,
    # but it's mapped to the same virtual address
    # (TRAPFRAME) in every process's user page table.
    li a0, TRAPFRAME
    
    # save the user registers in TRAPFRAME
    sd ra, 40(a0)
    sd sp, 48(a0)
    sd gp, 56(a0)
    ...略
    sd t4, 264(a0)
    sd t5, 272(a0)
    sd t6, 280(a0)

       # save the user a0 in p->trapframe->a0
    csrr t0, sscratch
    sd t0, 112(a0)

    ld sp, 8(a0) # initialize kernel stack pointer

    ld tp, 32(a0) # make tp hold the current hartid,

    ld t0, 16(a0) # load the address of usertrap()

    ld t1, 0(a0) # fetch the kernel page table address, 

    # wait for any previous memory operations to complete, so that
    # they use the user page table.
    sfence.vma zero, zero

    # install the kernel page table.
    csrw satp, t1

    # flush now-stale user entries from the TLB.
    sfence.vma zero, zero

    jr t0 # jump to usertrap()

   ```
#### III. `usertrap`

   ```c
   void
   usertrap(void)
   {
     int which_dev = 0;
   
     if((r_sstatus() & SSTATUS_SPP) != 0)
       panic("usertrap: not from user mode");
   
     w_stvec((uint64)kernelvec);
   
     struct proc *p = myproc();
     
     p->trapframe->epc = r_sepc();
     
     if(r_scause() == 8){// system call
   
       if(killed(p))
         exit(-1);
   
       p->trapframe->epc += 4;
   
       // an interrupt will change sepc, scause, and sstatus,
       // so enable only now that we're done with those    registers.
       intr_on();
   
       syscall();
     } else if((which_dev = devintr()) != 0){
       // ok
     } else {
       printf("usertrap(): unexpected scause %p pid=%d\n",    r_scause(), p->pid);
       printf("            sepc=%p stval=%p\n", r_sepc(),    r_stval());
       setkilled(p);
     }
   
     if(killed(p))
       exit(-1);
   
     // give up the CPU if this is a timer interrupt.
     if(which_dev == 2)
       implicityield();
   
     usertrapret();
   }
   ```
  #### IX. `devintr`
   ```c
   // check if it's an external interrupt or software interrupt,
   int
   devintr()
   {
     uint64 scause = r_scause();
   
     if((scause & 0x8000000000000000L) &&
        (scause & 0xff) == 9){
       // this is a supervisor external interrupt, via PLIC.
   
       // irq indicates which device interrupted.
       int irq = plic_claim();
   
       if(irq == UART0_IRQ){
         uartintr();
       } else if(irq == VIRTIO0_IRQ){
         virtio_disk_intr();
       } else if(irq){
         printf("unexpected interrupt irq=%d\n", irq);
       }
   
       // the PLIC allows each device to raise at most one
       // interrupt at a time; tell the PLIC the device is
       // now allowed to interrupt again.
       if(irq)
         plic_complete(irq);
   
       return 1;
     } else if(scause == 0x8000000000000001L){
       // software interrupt from a machine-mode timer    interrupt,
       // forwarded by timervec in kernelvec.S.
   
       if(cpuid() == 0){
         clockintr();
       }
       
       // acknowledge the software interrupt by clearing
       // the SSIP bit in sip.
       w_sip(r_sip() & ~2); //r_sip() 讀出目前 sip 的值，& ~2 把第 1 bit（SSIP）清成 0 ，w_sip(...) 寫回 sip → 表示我們已經處理完中斷
   
       return 2;
     } else {
       return 0;
     }
   }
   ```
  - devintr() 回傳值代表中斷類型：0（not recognized） / 2（timer interrupt） / 1（other device,）
  - `PLIC`：Platform-Level Interrupt Controller，負責「管理外部中斷」的硬體控制器。\
  - CPU 可能會同時接收到很多外部裝置的中斷（像 UART、磁碟、網路卡），
但 CPU 自己沒辦法知道是哪個外設發的。
這時就由 PLIC 來幫忙「統一收集、排隊、分發」這些中斷訊號。
  - `irq`：Interrupt Request，每個外部裝置（像 UART、磁碟、網卡）
都會被分配一個 中斷編號（irq number）。當裝置要通知 CPU「我有事要你處理」時，透過這個編號向 PLIC 發出中斷請求。
#### IV. `clockintr`
   - `clockintr()`：更新時間與喚醒睡眠程式
    
     ```c
     void
     clockintr()
     {
       acquire(&tickslock);
       ticks++;
       wakeup(&ticks); // 喚醒所有在等待時間的 process
       release(&tickslock);
     }
     ```

     > ticks 是全域變數（系統時鐘計數）。\
每次 timer 中斷 → ticks++。\
有些程式在睡眠時會呼叫 sleep(&ticks, ...)，\
→ 當 wakeup(&ticks) 執行時，那些睡在 &ticks 上的程式會被喚醒。
   - `CLINT`： 在xv6裡，，硬體本身有「timer」裝置，它會定期觸發中斷。
     - `mtime`：系統當前時間（不斷遞增的計數器）
     - `mtimecmp`：下一次要產生中斷的時間點
     > 只要 mtime >= mtimecmp，硬體就會發出一個 machine timer interrupt。\
     > 這個中斷首先會被 timervec（在 kernel/kernelvec.S 裡）接到。
timervec 做兩件事：
把 mtimecmp 加上固定的「間隔」值（例如 10,000 個 cycle），排定下一次中斷。
設定 sip 的 bit 2（Supervisor Interrupt Pending），讓 kernel 知道要處理時鐘中斷。
   - 系統的時鐘（CLINT）是硬體自動在「固定時間間隔」發出中斷的，
xv6 的 kernel 只需在 timervec 接收後處理這些中斷，更新 ticks 並喚醒被 sleep 的 process。

### 3. `kernel/kernelvec.S:kernelvec`
 - Kernel space interrupt handler
  > `usertrap` -> `kernel/kernelvec.S:kernelvec` -> `kerneltrap` -> `devintr` -> `clockintr`
   ```asm
   .globl kerneltrap
   .globl kernelvec
   .align 4
   kernelvec:
    # make room to save registers.
    addi sp, sp, -256

    # save the registers.
    sd ra, 0(sp)
    sd sp, 8(sp)
    sd gp, 16(sp)
    sd tp, 24(sp)
    sd t0, 32(sp)
    ...略
    sd t4, 224(sp)
    sd t5, 232(sp)
    sd t6, 240(sp)

    # call the C trap handler in trap.c
    call kerneltrap

    # restore registers.
    ld ra, 0(sp)
    ld sp, 8(sp)
    ld gp, 16(sp)
    # not tp (contains hartid), in case we moved CPUs
    ld t0, 32(sp)
    ld t1, 40(sp)
    ld t2, 48(sp)
    ld s0, 56(sp)
    ...略
    ld t4, 224(sp)
    ld t5, 232(sp)
    ld t6, 240(sp)

    addi sp, sp, 256

    # return to whatever we were doing in the kernel.
    sret

    #
    # machine-mode timer interrupt.
    #
   ```

   ```c
   // interrupts and exceptions from kernel code go here via kernelvec,
   // on whatever the current kernel stack is.
   void 
   kerneltrap()
   {
     int which_dev = 0;
     uint64 sepc = r_sepc();
     uint64 sstatus = r_sstatus();
     uint64 scause = r_scause();
     
     if((sstatus & SSTATUS_SPP) == 0)
       panic("kerneltrap: not from supervisor mode");
     if(intr_get() != 0)
       panic("kerneltrap: interrupts enabled");
   
     if((which_dev = devintr()) == 0){
       printf("scause %p\n", scause);
       printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
       panic("kerneltrap");
     }
   
     // give up the CPU if this is a timer interrupt.
     if(which_dev == 2 && myproc() != 0 && myproc()->state ==    RUNNING)
       implicityield();
   
     // the yield() may have caused some traps to occur,
     // so restore trap registers for use by kernelvec.S's sepc    instruction.
     w_sepc(sepc);
     w_sstatus(sstatus);
   }
   ```
  - [devintr](#ix-devintr)
   ```c
   extern int devintr();

   int
   devintr()
   {
     uint64 scause = r_scause();
   
     if((scause & 0x8000000000000000L) &&
        (scause & 0xff) == 9){

       int irq = plic_claim();
   
       if(irq == UART0_IRQ){
         uartintr();
       } else if(irq == VIRTIO0_IRQ){
         virtio_disk_intr();
       } else if(irq){
         printf("unexpected interrupt irq=%d\n", irq);
       }
   
       if(irq)
         plic_complete(irq);
   
       return 1;
     } else if(scause == 0x8000000000000001L){
   
       if(cpuid() == 0){
         clockintr();
       }
       
       w_sip(r_sip() & ~2);
   
       return 2;
     } else {
       return 0;
     }
    }
   ```
  - [clockintr()](#iv-clockintr)
   ```c
   void
   clockintr()
   {
     acquire(&tickslock);
     ticks++;
     wakeup(&ticks);
     release(&tickslock);
   }
   ```




### 4. `kernel/proc.h`
 -  mapping relationship of kernel/proc.h enum procstate to the process states ("New", "Ready", "Running", "Waiting", "Terminated").
 
   ```c
   enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };
   ```
- mapping
    - UNUSED
    - USED -> New
    - SLEEPING -> Waiting
    - RUNNABLE -> Ready
    - RUNNING -> Running
    - ZOMBIE -> Terminated
