# MP2 report 
  112062527 陳俞靜
  114065501 蔡宛秦

## Table of contents

## Trace Code

### 1. `kernel/kernelvec.S:timervec`
 - Setup timer interrupt
  > `timerinit` -> `kernel/kernelvec.S:timervec`

   ```asm
   .globl timervec
   .align 4
   timervec:
    # start.c has set up the memory that mscratch points to:
    # scratch[0,8,16] : register save area.
    # scratch[24] : address of CLINT's MTIMECMP register.
    # scratch[32] : desired interval between interrupts.
    
    csrrw a0, mscratch, a0
    sd a1, 0(a0)
    sd a2, 8(a0)
    sd a3, 16(a0)

    # schedule the next timer interrupt
    # by adding interval to mtimecmp.
    ld a1, 24(a0) # CLINT_MTIMECMP(hart)
    ld a2, 32(a0) # interval
    ld a3, 0(a1)
    add a3, a3, a2
    sd a3, 0(a1)

    # arrange for a supervisor software interrupt
    # after this handler returns.
    li a1, 2
    csrw sip, a1

    ld a3, 16(a0)
    ld a2, 8(a0)
    ld a1, 0(a0)
    csrrw a0, mscratch, a0

    mret
   ```


### 2. `kernel/trampoline.S:uservec`
 - User space interrupt handler
  > `usertrapret` -> `kernel/trampoline.S:uservec` -> `usertrap` -> `devintr` -> `clockintr`
   ```c
   //
   // return to user space
   //
   void
   usertrapret(void)
   {
     struct proc *p = myproc();
   
     // we're about to switch the destination of traps from
     // kerneltrap() to usertrap(), so turn off interrupts until
     // we're back in user space, where usertrap() is correct.
     intr_off();
   
     // send syscalls, interrupts, and exceptions to uservec in    trampoline.S
     uint64 trampoline_uservec = TRAMPOLINE + (uservec -    trampoline);
     w_stvec(trampoline_uservec);
   
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
    sd tp, 64(a0)
    sd t0, 72(a0)
    sd t1, 80(a0)
    sd t2, 88(a0)
    sd s0, 96(a0)
    sd s1, 104(a0)
    sd a1, 120(a0)
    sd a2, 128(a0)
    sd a3, 136(a0)
    sd a4, 144(a0)
    sd a5, 152(a0)
    sd a6, 160(a0)
    sd a7, 168(a0)
    sd s2, 176(a0)
    sd s3, 184(a0)
    sd s4, 192(a0)
    sd s5, 200(a0)
    sd s6, 208(a0)
    sd s7, 216(a0)
    sd s8, 224(a0)
    sd s9, 232(a0)
    sd s10, 240(a0)
    sd s11, 248(a0)
    sd t3, 256(a0)
    sd t4, 264(a0)
    sd t5, 272(a0)
    sd t6, 280(a0)

       # save the user a0 in p->trapframe->a0
    csrr t0, sscratch
    sd t0, 112(a0)

    # initialize kernel stack pointer, from p->trapframe->kernel_sp
    ld sp, 8(a0)

    # make tp hold the current hartid, from p->trapframe->kernel_hartid
    ld tp, 32(a0)

    # load the address of usertrap(), from p->trapframe->kernel_trap
    ld t0, 16(a0)


    # fetch the kernel page table address, from p->trapframe->kernel_satp.
    ld t1, 0(a0)

    # wait for any previous memory operations to complete, so that
    # they use the user page table.
    sfence.vma zero, zero

    # install the kernel page table.
    csrw satp, t1

    # flush now-stale user entries from the TLB.
    sfence.vma zero, zero

    # jump to usertrap(), which does not return
    jr t0

   ```
   ```c
   //
   // handle an interrupt, exception, or system call from user    space.
   // called from trampoline.S
   //
   void
   usertrap(void)
   {
     int which_dev = 0;
   
     if((r_sstatus() & SSTATUS_SPP) != 0)
       panic("usertrap: not from user mode");
   
     // send interrupts and exceptions to kerneltrap(),
     // since we're now in the kernel.
     w_stvec((uint64)kernelvec);
   
     struct proc *p = myproc();
     
     // save user program counter.
     p->trapframe->epc = r_sepc();
     
     if(r_scause() == 8){
       // system call
   
       if(killed(p))
         exit(-1);
   
       // sepc points to the ecall instruction,
       // but we want to return to the next instruction.
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
   ```c
   // check if it's an external interrupt or software interrupt,
   // and handle it.
   // returns 2 if timer interrupt,
   // 1 if other device,
   // 0 if not recognized.
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
       w_sip(r_sip() & ~2);
   
       return 2;
     } else {
       return 0;
     }
   }
   ```

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
    sd t1, 40(sp)
    sd t2, 48(sp)
    sd s0, 56(sp)
    sd s1, 64(sp)
    sd a0, 72(sp)
    sd a1, 80(sp)
    sd a2, 88(sp)
    sd a3, 96(sp)
    sd a4, 104(sp)
    sd a5, 112(sp)
    sd a6, 120(sp)
    sd a7, 128(sp)
    sd s2, 136(sp)
    sd s3, 144(sp)
    sd s4, 152(sp)
    sd s5, 160(sp)
    sd s6, 168(sp)
    sd s7, 176(sp)
    sd s8, 184(sp)
    sd s9, 192(sp)
    sd s10, 200(sp)
    sd s11, 208(sp)
    sd t3, 216(sp)
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
    ld s1, 64(sp)
    ld a0, 72(sp)
    ld a1, 80(sp)
    ld a2, 88(sp)
    ld a3, 96(sp)
    ld a4, 104(sp)
    ld a5, 112(sp)
    ld a6, 120(sp)
    ld a7, 128(sp)
    ld s2, 136(sp)
    ld s3, 144(sp)
    ld s4, 152(sp)
    ld s5, 160(sp)
    ld s6, 168(sp)
    ld s7, 176(sp)
    ld s8, 184(sp)
    ld s9, 192(sp)
    ld s10, 200(sp)
    ld s11, 208(sp)
    ld t3, 216(sp)
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

   ```c
   extern int devintr();

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
       // software interrupt from a machine-mode timer interrupt,
       // forwarded by timervec in kernelvec.S.
   
       if(cpuid() == 0){
         clockintr();
       }
       
       // acknowledge the software interrupt by clearing
       // the SSIP bit in sip.
       w_sip(r_sip() & ~2);
   
       return 2;
     } else {
       return 0;
     }
    }
   ```

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
 -
  >
   ```c
   enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };
   ```

   > `kernel/proc.c`
   ```c
   // for mp2
   char*
   procstate2str(enum procstate state)
   {
   switch(state) {
       case USED:     return "new";
       case SLEEPING: return "waiting";
       case RUNNABLE: return "ready";
       case RUNNING:  return "running";
       case UNUSED:   return "exit";
       case ZOMBIE:   return "exit";
       default:  return "unknown";
   }
   }
   ```


   ```c
   void
   pushreadylist(struct proc *p)
   {
     struct proclistnode *pn;
     if((pn = allocproclistnode(p)) == 0) {
       panic("pushreadylist: allocproclistnode");
     }
     pushbackproclist(&readylist, pn);
   }
   ```