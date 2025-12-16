#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "shm.h"

// External declarations
extern struct proc proc[NPROC];

/**
 * Wrapper functions for reading cycle, time, and instret CSRs
 */

uint64 sys_rdcycle(void)
{
  return r_cycle();
}

uint64 sys_rdtime(void)
{
  return r_time();
}

uint64 sys_rdinstret(void)
{
  return r_instret();
}

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if (t == SBRK_EAGER || n < 0)
  {
    if (growproc(n) < 0)
    {
      return -1;
    }
  }
  else
  {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if (addr + n < addr)
      return -1;
    if (addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()))
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_shmget(void)
{
  char name[SHM_NAME_LEN];
  uint64 size;
  int flags;

  if (argstr(0, name, SHM_NAME_LEN) < 0)
    return -1;

  argaddr(1, &size);
  argint(2, &flags);

  return shm_get(name, size, flags);
}

uint64
sys_shmat(void)
{
  int shmid;
  uint64 uaddr;
  int flags;

  argint(0, &shmid);
  argaddr(1, &uaddr);
  argint(2, &flags);

  return (uint64)shm_attach(shmid, (void *)uaddr, flags);
}

uint64
sys_shmdt(void)
{
  uint64 shmaddr;

  argaddr(0, &shmaddr);

  return shm_detach((void *)shmaddr);
}

uint64
sys_shmctl(void)
{
  int shmid;
  int cmd;
  uint64 buf;

  argint(0, &shmid);
  argint(1, &cmd);
  argaddr(2, &buf);

  return shmctl(shmid, cmd, (void *)buf);
}

uint64
sys_getramused(void)
{
  return get_used_ram();
}

/**
 * @brief Set the priority of a process.
 * @param pid Process ID (0 means current process)
 * @param priority New priority value (lower = higher priority)
 * @return 0 on success, -1 on error
 *
 * @details
 * Only works with PRIORITY_SCHED or MLFQ_SCHED schedulers.
 * For MLFQ, sets the initial queue level based on priority.
 */
uint64
sys_setpriority(void)
{
  int pid, priority;
  struct proc *p;

  argint(0, &pid);
  argint(1, &priority);

  // Validate priority range (0-31, lower = higher priority)
  if (priority < 0 || priority > 31)
    return -1;

  // If pid is 0, set priority for current process
  if (pid == 0)
  {
    p = myproc();
#if defined(PRIORITY_SCHED) || defined(MLFQ_SCHED)
    p->priority = priority;
#ifdef MLFQ_SCHED
    // For MLFQ, also reset to appropriate queue based on priority
    if (priority <= 10)
      p->queue_level = 0; // High priority -> highest queue
    else if (priority <= 20)
      p->queue_level = 1; // Medium priority -> middle queue
    else
      p->queue_level = 2; // Low priority -> lowest queue
    p->time_slice = 1;    // Reset time slice
#endif
#endif
    return 0;
  }

  // Find process by PID and set its priority
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->pid == pid)
    {
#if defined(PRIORITY_SCHED) || defined(MLFQ_SCHED)
      p->priority = priority;
#ifdef MLFQ_SCHED
      if (priority <= 10)
        p->queue_level = 0;
      else if (priority <= 20)
        p->queue_level = 1;
      else
        p->queue_level = 2;
      p->time_slice = 1;
#endif
#endif
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }

  return -1; // Process not found
}
