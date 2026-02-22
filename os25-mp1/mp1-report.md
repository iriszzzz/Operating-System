# MP1 report 
  112062527 陳俞靜
  114065501 蔡宛秦

## Table of contents

- [Trace Code](#trace-code)
  - [前言：如何觸發 `user/grep.c` 中的 `read()`](#前言如何觸發-usergrepc-中的-read)
  - [1. `user/grep.c/read()`](#1-usergrepcread)
  - [2. `user/usys.S/read`](#2-userusyssread)
  - [3. `kernel/trampoline.S/uservec`](#3-kerneltrampolinesuservec)
  - [4. `kernel/trap.c/usertrap()`](#4-kerneltrapcusertrap)
  - [5. `kernel/syscall.c/syscall()`](#5-kernelsyscallcsyscall)
  - [6. `kernel/sysfile.c/sys_read()`](#6-kernelsysfilecsys_read)
- [Implementation](#implementation)
  - [1. Add a new system call: trace](#1-add-a-new-system-call-trace)
  - [2. Add a new system call: sysinfo](#2-add-a-new-system-call-sysinfo)
- [Test report](#test-report)
  - [1. trace](#1-trace)
  - [2. sysinfo](#2-sysinfo)
- [Contributions](#contributions)

## Trace Code

> Explain how a user program triggers a system call (take `read` for example)

```

User Space                                  Kernel Space
-----------                                 -------------
Terminal (shell)                            
    │
    ├── user/sh.c 解析指令 `grep`
    │       ├── fork()
    │       └── exec("/grep") ───────────────▶ kernel/exec.c 載入 ELF
    │                                               │
    │                                               └── 設定 entry point → main() in user/grep.c
    │
    └── grep.c: main()
            │
            └── 呼叫 read(fd, buf, n)
                    │
                    └── usys.S: a7 ← SYS_read; ecall ──▶ trap
                                                         │
                                                         ▼
                                               trampoline.S:uservec
                                               （保存暫存器、換頁表）
                                                         │
                                                         ▼
                                               trap.c:usertrap()
                                                         │
                                                         └── syscall()
                                                         │
                                                         ▼
                                               syscall.c:syscall()
                                                         │
                                                         └── sys_read() in sysfile.c
                                                                 │
                                                                 └── fileread() in fs.c
                                                                         │
                                                                         └── virtio_disk.c 讀磁碟
                                                                         
返回路徑
---------
virtio_disk.c → fs.c → sys_read() → copyout() 
       │
       ▼
usertrapret() → trampoline → sret
       │
       ▼
回到 user/grep.c，read() 得到資料繼續執行

```

### 前言：如何觸發 `user/grep.c` 中的 `read()`

1. `user/sh.c` Shell 執行指令並觸發 `exec("grep", argv)`

    shell 啟動後會反覆：
    - 讀取使用者輸入: (例如 `grep pattern file`) ，並解析字串、把他拆成指令與參數
    - 建立子行程: 呼叫 `fork1()`，子行程中呼叫 `runcmd()`，裡面執行 `exec(argv[0], argv)` (也就是將 grep 這個程式及其參數傳遞給kernel執行)
    - 父行程等待: 使用 `wait()` 等子行程完成

2. `kernel/syscall.c` 進入 kernel 的 `exec()` 系統呼叫

    子行程 `exec(argv[0], argv)` 呼叫後，會進入 `kernel/syscall.c/syscall()` 的系統呼叫
    - 系統呼叫進入 kernel: 接著 `syscall()` 呼叫 `kernel/sysfile.c/sys_exec()`，將 user space 的 `exec(path, argv)` 參數 (即 grep 路徑與參數) 複製到 kernel space
    - 載入新程式: 接著呼叫 `kernel/exec.c/exec()`  從檔案系統載入 /grep，這會分配新記憶體、清掉舊的行程內容，載入程式碼到對應記憶體區段，設定 stack 並把參數 `argc`, `argv` 放到新的 user stack，最後設定 entry point（即 `user/grep.c` 中 `_main`）
      - 執行檔 /grep: 在 xv6 build 時，所有 user/*.c 都會被編譯成 user binary (包含 `user/grep.c`)，編譯後的 ELF(Executable and Linkable Format) binary 被打包進 xv6 的初始檔案系統映像 (一個檔案格式的虛擬磁碟) 中，在系統啟動後就可以被載入與執行

---
### 1. `user/grep.c/read()`

  - 從第一個檔案grep.c開始trace如何從用戶端read function觸發system call(只摘錄程式碼裡重點部分)

    ```c
    #include "user/user.h"

    void
    grep(char *pattern, int fd)
    {
      int n, m;
      char *p, *q;

      m = 0;
      while((n = read(fd, buf+m, sizeof(buf)-m-1)) > 0)
      #剩餘省略
    }
    ```

  - `grep.c`：搜尋檔案內容裡符合特定字串或正規表示式
  - 程式碼反覆呼叫`read()`，這裡會觸發syscall，因為在`user.h`檔裡宣告了 `int read(int, void*, int);`，函式對應到的stub在`usys.S`檔裡實作如下

### 2. `user/usys.S/read`

  - 由 `user/usys.pl` 自動產生的 `stub`
    >`stub`：在 xv6 裡，stub 是由 usys.pl 自動產生的一小段組語函式（在 user/usys.S），他的角色只是一個接口，將上層caller的呼叫傳遞到下層的實作，也就是說，stub 負責把使用者態的函式呼叫（例如 read()）轉換成系統呼叫，藉由 ecall 進入 kernel，由核心端的 sys_read() 完成真正的功能

    ```asm
    #include "kernel/syscall.h"
    .global read
    read:
    li a7, SYS_read
    ecall 
    ret
    ```

  - `.global`： 用於標記一個符號（例如函式或變數）為全域可見，讓其他檔案或模組能夠存取它
  - `li` : 將 SYS_read : 5（系統呼叫編號）放到a7暫存器
  - `ecall` : RISC-V 架構中的一種「環境呼叫」指令，用於從user mode提升權限到kernel mode，以執行需要更高特權的操作
  - `ret` : 指令會從傳回位址堆疊擷取指令的位址

### 3. `kernel/trampoline.S/uservec`

  - `trampoline.S` ：跳板程式，user mode ↔ kernel mode 之間的切換 
  - `uservec` : trap入口點，當 CPU 在 user mode 發生 trap（例如剛剛的 ecall 或 page fault、中斷）時，會先跳到這裡 

    > 它會將 user 的暫存器內容先暫存至 trapframe，並將 kernel 所需的內容載入並初始化到暫存器

    > RISC-V trap 發生時，不會自動切 page table，還是沿用 user page table 

  - `stvec`: RISC-V 的硬體暫存器 (CSR)：Supervisor Trap Vector Base Address Register 
  - `csrw sscratch, a0`將使用者的暫存器放入sscratch，避免被覆蓋，因為接下來a0會被指向trapframe(xv6為每個process準備的記憶體空間暫存cpu暫存器)，
    ```asm
    uservec:    
      csrw sscratch, a0

      li a0, TRAPFRAME
      
      sd ra, 40(a0)
      sd sp, 48(a0)
      sd gp, 56(a0)
      sd tp, 64(a0)
      sd t0, 72(a0)
      # 略
      sd t6, 280(a0)

      csrr t0, sscratch
      sd t0, 112(a0)
    ```
  - 保存使用者暫存器
    ```asm
      ld sp, 8(a0)
      ld tp, 32(a0)
      ld t0, 16(a0)
      ld t1, 0(a0)
    ```
  - 初始化kernel執行環境
    ```asm
      sfence.vma zero, zero
      csrw satp, t1
      sfence.vma zero, zero
      jr t0
    ```
  - 切換 page table 並跳到 kernel

### 4. `kernel/trap.c/usertrap()`
  - 先確保 trap 是從 user mode 發出，SSTATUS_SPP (Supervisor Previous Privilege) = 1 ，表 trap 是從 Supervisor(Kernel) Mode 發出的 (不應該進入 usertrap)
  - 並將 stvec(Supervisor Trap Vector) 改成 kernelvec，這樣若在 kernel 裡又出現新的 trap，就會正確地跳到 kernelvec
      
    - 兩個trap入口: uservec 專門處理「從使用者空間來的trap（含 system call）」；kernvec 專門處理「在核心空間內發生的trap/中斷」，用 kernel stack 與 kernel page table 儲存
  - 將 trap 的 user mode 指令位置 (pc) 存入 trapframe 裡的 epc(exception program counter)，方便執行完將觸發 trap 後跳回 user mode 時繼續執行
  - 檢查 trap 類型: r_scause = 8 表示 user mode 下的 ecall（system call），如果 process 已經被標記為 killed 就結束它。否則把 epc 加 4，跳過 ecall 指令(否則回user mode 又再再次觸發 system call)，intr_on() 啟用 interrupt
    - intr_on() 啟用 interrupt: CPU 在 trap 進入 kernel 時自動把 interrupt 關掉避免處理暫存器時被中斷。開啟是為了允許 timer interrupt、device interrupt 等被處理
  - 當發現 trap 是來自 `ecall`，會進一步呼叫 `syscall()`

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
    
    if(r_scause() == 8){

      if(killed(p))
        exit(-1);

      p->trapframe->epc += 4;

      intr_on();

      syscall();
    } else if((which_dev = devintr()) != 0){
      // ok
    } else {
      printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
      printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
      setkilled(p);
    }

    if(killed(p))
      exit(-1);

    if(which_dev == 2)
      yield();

    usertrapret();
  }
  ```

### 5. `kernel/syscall.c/syscall()`
  - `syscall()` 會根據 `a7` 暫存器中的 system call 編號 (這邊是常數 `SYS_read`) 來決定呼叫哪個內部函數，執行該系統呼叫，並將結果放入 `a0`
  (`syscall.h` 定義 `SYS_read=63`，所以是對應於 `sys_read()`)
  - 如果系統呼叫無效，則會顯示錯誤訊息並將 a0 設為 -1，表示失敗

    ```c
    void
    syscall(void)
    {
      int num;
      struct proc *p = myproc();

      num = p->trapframe->a7;
      if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {

        p->trapframe->a0 = syscalls[num]();
      } else {
        printf("%d %s: unknown sys call %d\n",
                p->pid, p->name, num);
        p->trapframe->a0 = -1;
      }
    }
    ```

### 6. `kernel/sysfile.c/sys_read()`
  - 步驟對應 user 程式呼叫原型：`int read(int fd, void *buf, int n)`
  - 用 `argaddr()` 和 `argint()` 從 user stack 中抓取參數值，取第二、三個參數，對應使用者位址、長度`n` bytes
  - `argfd()` 會去透過目前的 process 裡的 `ofile[]` 陣列去找 `fd`(file descriptor) 對應的 `struct file *`，並存到 `struct file *f` 變數
    - ofile[0]: fd = 0（通常是stdin），`ofile[]` 是 process 目前的檔案清單
    - ofile[1]: fd = 1（stdout）
    - ofile[2]: fd = 2（stderr）
  - 呼叫 `fileread()`，從檔案 `f` 中讀取 n bytes，寫入到 使用者空間的位址 `p`。實際的資料傳輸是在這裡進行的，返回值是實際讀取到的 bytes 數，或者錯誤碼

  ```c
  uint64 sys_read(void)
  {
    struct file *f;
    int n;
    uint64 p;

    argaddr(1, &p);
    argint(2, &n);
    if(argfd(0, 0, &f) < 0)
      return -1;
    return fileread(f, p, n);
  }
  ```

## Implementation
### 1. Add a new system call: trace

  - `Makefile`

    ```makefile
    UPROGS=\
      $U/_trace\ #加上此行
      $U/_sysinfotest #加上此行
    ```
    - `UPROGS=\
      $U/_trace\` : 將 user/trace.c 檔編譯，並加入xv6檔案系統裡（打包進磁碟映像檔磁碟映像檔fs.img），使得這些程式像存在xv6根目錄" / "，從而可在xv6 shell 打trace執行

  - `user/user.h` 和 `usys.pl`

    ```c
    int trace(int); //加上此行
    ```
    - 提供編譯期的函式宣告，讓所有 user program 能看到 trace 函式原型
      ```perl
      sub entry {
          my $name = shift;
          print ".global $name\n";
          print "${name}:\n";
          print " li a7, SYS_${name}\n";
          print " ecall\n";
          print " ret\n";
      }

      entry("trace"); #加上此行
      ```
    - usys.pl 是一個 Perl 產生器，加上`entry("trace")`會根據指令自動輸出對應的 RISC-V 組語到 user/usys.S 可呼應至一開始 trace read() 的 `user/usys.S 解釋`，

  - `kernel/syscall.h`

    ```c
    #define SYS_trace  22 //加上此行
    ```
    - system call 編號（SYS_xxx）是 xv6 核心辨識不同 system call 的依據

  - user/usys.S

    ```asm
    .global trace
    trace:
    li a7, SYS_trace
    ecall
    ret
    ```
    - 藉由之前提到的在 usys.pl 增加的`entry("trace")`所自動產生的 system call stub，功能是把系統呼叫號碼 SYS_trace 放到 a7，再透過 ecall 進入 kernel。但還需要實作完整的trace func 才能真正執行，因此有下一步

  - `kernel/sysproc.c`

    ```c
    #include "proc.h"

    uint64
    sys_trace(void)
    {
      int mask;
      argint(0, &mask);
      myproc()->tracemask = mask;
      return 0;
    }
    ```
    - 在 kernel/sysproc.c 新增 trace func 系統呼叫函式
    - `mask`：呼叫者這次傳進來的設定值（臨時變數）
    是 int trace ( int ) 從 user program 傳進的參數，如 $ trace 14 < cmd >，14 則會作為變數 mask 存到kernel stack裡
    - `tracemask`：存放在 process 結構裡的欄位，通常設計成 bit mask（每個 bit 代表一種 syscall），用來記錄這個 process 之後每次執行 syscall 時要不要輸出 trace。 例如：14 的二進位為 `0b1110`，表示啟用追蹤為 1，2，3，沒有 0，

  - `kernel/proc.h`

    ```c
    struct proc {
      struct spinlock lock;
      //中間省略
      int tracemask;               //加入
    };
    ```
    - 在 kernel/proc.h 修改 proc 結構增添一個 tracemask 欄位。每個 process 則可各自設定則可各自設定追蹤狀態

  - `kernel/syscall.c`

    ```c
    #include "proc.h"
    #include "syscall.h"
    //省略
    extern uint64 sys_trace(void);  //加入
    ```
    - `extern`：此函式在別的程式 `kernel/sysproc.c` 檔案定義，在此宣告是告知編譯器此函式的存在，最後由 linker 把定義補上
    
      ```c
      static uint64 (*syscalls[])(void) = {
      [SYS_fork]    sys_fork,
      //中間省略
      [SYS_trace]   sys_trace,  //加入
      };
      ```

    - `static`：限定作用範圍在本檔案（syscall.c）內，不會被其他檔案引用
    - `(*syscalls[])(void)`：syscalls 是一個陣列 [ ] ，陣列的元素型別是 「指向函式的指標（void->沒有參數）」
      > 將在 `kernel/syscall.h` 定義的編號 `#define SYS_trace  22` 對應到 systrace 函式

        ```c
        static const char *syscallnames[] = { //加入整段
          [SYS_fork]   = "fork",
        //中間省略
          [SYS_trace]  = "trace",
        };
        ```

    - 為了能在呼叫印出 syscall 名稱訊息時，因此加入此列字串

      ```c
      void
      syscall(void)
      {
        int num;
        struct proc *p = myproc();

        num = p->trapframe->a7;
        if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {

          p->trapframe->a0 = syscalls[num]();
          //// 加入下述if這段
          if (p->tracemask & (1 << num)){
            printf("%d: syscall %s -> %d\n", p->pid, syscallnames[num], p->trapframe->a0);
          }

        } else {
          printf("%d %s: unknown sys call %d\n",
                  p->pid, p->name, num);
          p->trapframe->a0 = -1;
        }
      }
      ```

    - 取得系統呼叫號碼 : 從目前 `process` 的的 `trapframe` 中取出暫存器 `a7` （存放系統呼叫號碼的 reg ）的值
    - 合法性檢查 : 確保該值 > 0 ，小於 syscall 陣列大小 ‘`NELEM(syscalls)`’，該號碼對應的 syscall 函式存在，否則進入 else 
    - 執行對應 syscall ： 將 return value 放入 trapframe 的 a0 暫存器，以便返回 user mode 時，程式能讀到返回值
    - Trace 機制 : `p->tracemask & (1 << num)` 確認 tracemask 存在，且該 num 位移對應的 bit 值為 1，才印出「pid、syscall 名稱、回傳值」


  - kernel/proc.c
    - 在 `fork(void)` 函式中，加入 `np->tracemask = p->tracemask;`，確保當父程式呼叫 `fork` 時，子程式也能正確繼承父程式的 `tracemask` 設定值
      ```c
      int
      fork(void)
      {
        int i, pid;
        struct proc *np;
        struct proc *p = myproc();

        // Allocate and Copy Progress
        // 中間省略
        np->trapframe->a0 = 0;

        //// 加入這行copy trace mask from parent to child.
        np->tracemask = p->tracemask;

        // 省略後續
        return pid;
      }
      ```

### 2. Add a new system call: sysinfo
- `Makefile`

  UPROGS 讓 user program 能被編譯，並被打包進 xv6 的虛擬檔案系統，讓 xv6 shell 呼叫可以直接執行

  ```Makefile
  UPROGS=\
    ...
    $U/_trace\ 
    $U/_sysinfotest # 新增
  ```
- `user/user.h`, `user/usys.pl`
  - `user/user.h` 先宣告: 讓 user space 的程式可以使用 system call `sysinfo()`
    ```h
    // system calls
    int fork(void);
    ...
    struct sysinfo; # 新增
    int sysinfo(struct sysinfo *); # 新增
    ```
  - `user/usys.pl`: 建立 user space 和 kernel 的橋樑

    ```pl
    entry("fork");
    ...
    entry("uptime");
    entry("sysinfo"); # 新增
    ```
    因此執行 make 時，`usys.pl` 會重新產生 `user/usys.S`，並新增：

    ```pl
    .global sysinfo
    sysinfo:
        li a7, SYS_sysinfo
        ecall
        ret
    ```
    所以當 user space 呼叫 `sysinfo()` 時，實際會執行：設定 syscall 號碼到 a7、 `ecall` 進入 kernel (但需要實作完整的 `sysinfo()` 才能真正執行，因此要到 kernel 定義)，上述的兩步驟都會在之後實踐
    

- `kernel/syscall.h` 
  承接上述，為新增的 `sysinfo()` 系統呼叫分配一個 syscall 編號
  (user-space 呼叫系統函數時，是透過 `ecall`  + 把系統呼叫編號放在 `a7`，kernel 根據這個編號來決定要執行哪個函數)

  ```h
  #define SYS_fork    1
  ...
  #define SYS_trace  22 
  #define SYS_sysinfo 23 # 新增
  ```

- `kernel/syscall.c`, `kernel/sysproc.c`: kernel 端處理
  - `kernel/syscall.c`: 定義 `sysinfo()`讓 `syscall.c` 知道 `sys_sysinfo()` 是一個合法函數
    ```c
    extern uint64 sys_fork(void);
    ...
    extern uint64 sys_trace(void); 
    extern uint64 sys_sysinfo(void); # 新增
    ```
    剛剛在 `syscall.h` 定義 `#define SYS_sysinfo 23`，這個系統呼叫編號要能找到對應的 kernel 函數，當 user program 執行 `ecall` CPU 會到這裡，`syscall()`會透過 trapframe 讀取 a7，用 `syscalls[num]` 找對應函數，接著再執行該函數

    ```c
    static uint64 (*syscalls[])(void) = {
    [SYS_fork]    sys_fork,
    ...
    [SYS_trace]   sys_trace, 
    [SYS_sysinfo] sys_sysinfo, # 新增
    };

    void
    syscall(void)
    {
      int num;
      struct proc *p = myproc();

      num = p->trapframe->a7;
      if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
      }
      ...
    }
    ```
-  `kernel/sysinfo.h`, `kernel/sysproc.c`: 實作
    - `kernel/sysinfo.h` (在clone下來的檔案已定義好 `struct sysinfo` )
      ```h
      struct sysinfo {
        uint freemem;  // 剩餘記憶體量（bytes）
        int nproc;     // 執行中的 process 數量
      };
      ```
    - `kernel/sysproc.c`
      記得要包含剛剛定義的 `struct sysinfo` 檔案，並預定義會在 `kalloc.c` 出現的函數
      ```c
      #include "types.h"
      ...
      #include "sysinfo.h" # 新增

      extern uint64 freemem(void); # 新增
      extern int num_proc(void); # 新增
      ```

      用內建函式 `myproc()` 取得一個指向目前正在執行這段 system call 的 process 的指標  `p`，這個指標可以查 Page Table，找到 user space 傳入的虛擬位址 `dst` (destination) 對應的實體記憶體，也就是要把資料寫回去的位址
      -  資料流: kernel 的 info → 利用 `copyout(p->pagetable, dst, ...)` → 拷貝到 user 的記憶體位置
      
      `argaddr(0, &dst)` 從 syscall 的第 0 個參數 (`a0`) 拿出來的值，存到 `dst` 裡，接著呼叫預定義的函數 (`freemem()` 計算目前有多少空的記憶體、`num_proc()` 計算目前的 process 數)，`copyout()` 將 kernel 的  `&info` 複製到 user space address `dst` (kernel 空間不能直接存取 user space 的虛擬記憶體，必須透過 `copyout()` 做安全的轉換)

      ```c
      # 新增 sys_sysinfo() 
      uint64
      sys_sysinfo(void) 
      {
        struct sysinfo info;
        struct proc *p = myproc();
        uint64 dst;

        argaddr(0, &dst); // 從 user space 取得參數

        info.freemem = freemem();
        info.nproc = num_proc();

        // 複製給 user space
        if(copyout(p->pagetable, dst, (char *)&info, sizeof(info)) < 0)
          return -1;

        return 0;
      }
      ```
- `kernel/kalloc.c`

  實作 `freemem()` 回傳目前未分配的實體記憶體
  
  `kmem.freelist` 是全域共用的變數，為確保資料一致性，使用引用的 `spinlock.h` 中的 `acquire()`, `release()`，`acquire()` 會上鎖讓其他核心或執行緒被擋下來，不能同時操作共享資源，避免 `freelist` 不被更動，接著在 `release()` 解鎖讓其他核心可以操作

  在上鎖期間，用 for 迴圈計算還有多少沒被使用的記憶體頁面，每個大小是 PGSIZE（4KB）

  ```c
  uint64
  freemem(void) # 新增此函式
  {
    struct run *r;
    uint64 free_bytes = 0;

    acquire(&kmem.lock);
    for (r = kmem.freelist; r; r = r->next)
      free_bytes += PGSIZE;
    release(&kmem.lock);

    return free_bytes;
  }
  ```

- `kernel/proc.c`

  用 for 走訪整個 proc[] 陣列 (process table，裡面每個代表一 process 的資訊)，`NPROC` 是系統允許的最多 process 數，判斷時 p 對應的 process 要加鎖因為讀它的 state 時，state 可能會被其他核心改，所以要鎖住保證一致性，如果這個 process 的狀態不是 UNUSED，就把有效的 process 數量 `count` +1，判斷完後就解鎖

  ```c
  int
  num_proc(void) # 新增此函式
  {
    struct proc *p;
    int count = 0;

    for (p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state != UNUSED)
        count++;
      release(&p->lock);
    }

    return count;
  }
  ```
- `kernel/defs.h`

  在 `defs.h` 中宣告函數

  ```h
  ... 
  // kalloc.c
  void*           kalloc(void);
  ...
  uint64          freemem(void); # 新增
  
  // proc.c
  int             cpuid(void);
  ...
  int             num_proc(void); # 新增
  ```


## Test report
### 1. trace

- `public 測試`:
  -  執行 `trace 32 grep hello README` 確認指令正常執行`syscall_read`
  - 執行 `grep hello README` 確認不會有任何`syscall output`
  - 執行`trace 2 usertests forkforkfork` 壓力測試下確認大量的`fork`指令會正確繼承`tracemask`只能有`syscall fork`的 output 資訊
  ```
  ./grade-mp1-public
  make: 'kernel/kernel' is up to date.
  == Test trace 32 grep == trace 32 grep: OK (1.4s) 
  == Test trace nothing == trace nothing: OK (0.9s) 
  == Test trace children == trace children: OK (11.8s) 
  ```
-  `bonus 測試`： `./grade-mp1-bonus`
 1. `Test設計`：測試 `syscall exit` 的 output
    - 執行`trace 14 usertests fork`確認在fork情況下 syscall 也有正確輸出`fork \ exit \ wait` 三種 syscall output。 
    >因為在測試trace的過程找不到 exit 的 output，發現因為在一開始設計 syscall.c 只單純做了一個單一指令處理全部syscall，而在`printf`的呼叫時，syscall exit 已經呼叫並離開檔案，所以不會執行到後續指令
    ```c
    p->trapframe->a0 = syscalls[num](); //執行 exit 直接離開

    if (p->tracemask & (1 << num)){
      printf("%d: syscall %s -> %d\n", p->pid, syscallnames[num], p->trapframe->a0);
    }
    ```
    > 因此在後續修改成兩種情況 exit 或 non exit 處理 syscall 輸出
    ```c
    if (num == SYS_exit && (p->tracemask & (1U << num))) {
      printf("%d: syscall %s -> %d\n", p->pid, syscallnames[num], p->trapframe->a0);
    }

    p->trapframe->a0 = syscalls[num]();
    if (num != SYS_exit && (p->tracemask & (1U << num))){ //省略
    }
    ```
    ```
    == Test trace fork-exit-wait == trace fork-exit-wait: OK (10.2s) 
    ```
    > 修改後也能追蹤到 `4: syscall exit -> 0` 的輸出

  2. `Test設計`：確保非法號碼會進入 else branch
      - 為了能在 @test 裡執行指令，所以在 user/usertests.c 裡加入一個新的函數。執行 `usertests test_unknown` 時，程式碼會進入 else branch 並輸出 `unknown sys call` 
      ```c
      void
      test_unknown(void)
      {
        // 嘗試一個不存在的 syscall number
       asm volatile("li a7, 999; ecall");
      }
      ```
      ```
      == Test unknown syscall == unknown syscall: OK (0.6s) 
      ```

      > 能正常進入else branch print 出 `3 usertests: unknown sys call 999`

### 2. sysinfo

  使用 `user/sysinfotest.c` 測試:
  - `testcall()` 測試系統呼叫與參數驗證
  - `testmem()` 測試 `freemem` 是否反映實體記憶體使用情況
  - `testproc()` 測試 `nproc` 是否反應系統中的 process 數量
  - `testbad()` 測試是否可處理不合法的參數
  輸出 OK 表所有測試皆通過，`sysinfo()` 能正確取得並回傳系統狀態

  ```bash
  $ sysinfotest
  sysinfotest: start
  sysinfotest: OK
  ```

  接著用自己設計的 `user/sysinfotestbonus.c` 測試:
  - `testforkburst()`: 建立大量子行程來測試 `nproc` 增加數一不一致
  
    主要概念是呼叫 `sysinfo()`，取得目前系統有多少行程並將此設成基準值 `base_nproc`，接著建立子行程 (用`sleep(100)` 讓建立的子行程睡一下，讓 `nproc` 還會算上這個行程)，一直到 `fork()` 失敗，最後再次呼叫 `sysinfo()`，檢查 `nproc` 是否如預期至少等於 base_nproc + i（i 是成功 `fork()` 出來的數量）

    ```c
    void
    testforkburst() {
      struct sysinfo info;
      int maxchildren = 128;
      int pids[maxchildren];
      int i;

      sysinfo(&info);
      int baseNproc = info.nproc;

      for (i = 0; i < maxchildren; i++) {
        int pid = fork();
        if (pid < 0) {
          // End if fork fails (resources exhausted)
          printf("testforkburst: fork failed at %d\n", i);
          break;
        } else if (pid == 0) {
          sleep(100);  // Child process lives for a while
          exit(0);
        } else {
          pids[i] = pid;
        }
      }

      sysinfo(&info);

      if (info.nproc < baseNproc + i) {
        printf("testforkburst: FAIL: expected nproc >= %d, got %d\n", baseNproc + i, info.nproc);
        exit(1);
      }
      for (int j = 0; j < i; j++) {
        wait(0);
      }

      printf("testforkburst: OK (forked %d children)\n", i);
    }
    ```
  - `testforktreerecursive()`: 呼叫 `testforktreerecursive()`  用遞迴建立深度為 depth 的 process 樹、紀錄 nproc 前後的變化

    首先會記錄未 `fork()` 之前的行程數，接著用 `testforktreerecursive()` 的 `fork()` 遞迴產生新子行程，直到 depth 遞減到 0 (若過程中 `fork()` 失敗，例如記憶體不夠，就會印出錯誤訊息)，再檢查目前有多少行程，這時父行程會用 `wait()` 等釋放子行程資源，所以行程數量 (`nproc`) 應該要回到原本的值 (`base`)

    ```c
    void
    testforktreerecursive(int depth) {
      if (depth <= 0)
        return;

      int pid = fork();
      if (pid < 0) {
        printf("testforktree: fork failed at depth %d\n", depth);
        exit(1);
      } else if (pid == 0) {
        sleep(50);  
        testforktreerecursive(depth - 1);
        exit(0);
      } else {
        wait(0);
      }
    }

    void
    testforktree() {
      struct sysinfo info;
      sysinfo(&info);
      int base = info.nproc;

      testforktreerecursive(5); // 建立深度 5 的樹
 
      sysinfo(&info);
      if (info.nproc != base) {
        printf("testforktree: FAIL: expected nproc = %d, got %d\n", base, info.nproc);
        exit(1);
      }

      printf("testforktree: OK\n");
    }
    ```

    來測試！第 61 次 `fork()` 失敗 (因為系統資源有限所以這是可能的) 
    > testforkburst: OK (forked 61 children)	
    
    測試通過，成功 fork 出 61 個子行程，`sysinfotestbonus: OK` 表 `sysinfo()` 返回正確的 `nproc` 數值
  
    ```bash
    $ sysinfotestbonus
    sysinfotestbonus: start
    testforkburst: fork failed at 61
    testforkburst: OK (forked 61 children)
    testforktree: OK
    sysinfotestbonus: OK
    ```



## Contributions
| 工作項目  | 陳俞靜 | 蔡宛秦 |
|-----------------|----------------------|----------------------|
| Trace Code 1~3      | V |  |
| Trace Code 4~6         |  | V |
| `trace()` 實作        | V |   |
| `syinfo()` 實作         |  | V  |
| 撰寫報告 |  Trace Code 1~3、`trace()` 實作與測試報告 |  Trace Code 4~6、`syinfo()` 實作與測試報告 |
| Bonus: `trace()` 測資              | V  |  |
| Bonus: `syinfo()` 測資             |    | V  |