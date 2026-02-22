#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"
#include "proc.h"

/* Page fault handler */
int handle_pgfault(uint64 va) // ADD TODO 3 int handle_pgfault()
{
    /* Find the address that caused the fault */
    /* uint64 va = r_stval(); */

    /* mp3 TODO */
    // ADD TODO 3
    struct proc *p = myproc();
    va = PGROUNDDOWN(va);  // 對齊頁邊界

    // 檢查 fault 位址是否在合法範圍
    if (va >= p->sz || va < 0)
        return -1;

    pte_t *pte = walk(p->pagetable, va, 0);
    if(pte == 0 || (*pte & (PTE_V | PTE_S)) == 0){  // case 1：完全沒有 PTE 或 flags 都是 0 → 當作「從沒分配過」→ lazy alloc
        char *mem = kalloc();
        if (mem == 0)
            return -1;

        memset(mem, 0, PGSIZE);
        *pte = PA2PTE((uint64)mem) | PTE_U | PTE_R | PTE_W | PTE_X | PTE_V;
        return 0;

    }else if((*pte & PTE_S) && !(*pte & PTE_V)){    // NEW case 2：這一頁在 disk 上（swapped），PTE_S = 1, PTE_V = 0

        if(swapin_page(p->pagetable, va, pte) < 0){
            return -1;
        }
        
        return 0;
    }
    return -1;

    // panic("not implemented yet\n");
}
