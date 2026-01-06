/**
 * @file sched_priority.c
 * @brief Priority-based CPU scheduler implementation for xv6.
 * @author Hamna Sajid
 * @date December 16, 2025
 *
 * @details
 * This file implements a priority-based scheduling algorithm for xv6.
 * Processes are assigned a static priority value (lower number = higher priority).
 * The scheduler always selects the runnable process with the highest priority
 * (lowest priority value) to run next.
 *
 * Key features:
 * - Static priority assignment (default: 10)
 * - Preemptive scheduling via timer interrupts
 * - Simple priority-based selection (no aging mechanism)
 *
 * Limitations:
 * - Susceptible to priority inversion
 * - Can lead to starvation of low-priority processes
 * - No dynamic priority adjustment
 *
 * Compilation:
 * This scheduler is enabled when PRIORITY_SCHED is defined at compile time.
 * Use: make SCHED_FLAG=PRIORITY qemu
 */

#ifdef PRIORITY_SCHED

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// External declarations
extern struct proc proc[NPROC];

/**
 * @brief Priority-based CPU scheduler main loop.
 *
 * @details
 * This is the main scheduler function that runs in a loop on each CPU core.
 * It continuously searches for the highest priority (lowest priority value)
 * runnable process and context switches to it.
 *
 * Algorithm:
 * 1. Enable interrupts to allow device I/O
 * 2. Scan the process table to find the highest priority RUNNABLE process
 * 3. Context switch to the selected process
 * 4. Upon return (after process yields/blocks), repeat
 *
 * Synchronization:
 * - Acquires process locks during iteration to safely check state
 * - Holds only one process lock at a time to avoid deadlock
 * - Releases locks of non-selected processes immediately
 *
 * @note This function never returns - it loops indefinitely.
 * @note If no runnable process exists, the CPU idles with interrupts enabled.
 */
void scheduler(void)
{
    struct proc *p;
    struct cpu *c = mycpu();

    c->proc = 0;

    for (;;)
    {
        // Enable interrupts on this processor to allow device I/O
        intr_on();

        // Variables to track the highest priority process found
        struct proc *highest_priority_proc = 0;
        int lowest_priority_val = __INT_MAX__;

        // Sca[]n0 ]rocess table for highest priority RUNNABLE process
        for (p = proc; p < &proc[NPROC]; p++)
        {
            acquire(&p->lock);

            if (p->state == RUNNABLE)
            {
                // Check if this process has higher priority (lower value) than current best
                if (p->priority < lowest_priority_val)
                {
                    // Release lock of previous candidate if any
                    if (highest_priority_proc != 0)
                    {
                        release(&highest_priority_proc->lock);
                    }

                    // Update to new highest priority process
                    lowest_priority_val = p->priority;
                    highest_priority_proc = p;
                }
                else
                {
                    // Not the highest priority, release lock
                    release(&p->lock);
                }
            }
            else
            {
                // Process not runnable, release lock
                release(&p->lock);
            }
        }

        // If we found a runnable process, switch to it
        if (highest_priority_proc != 0)
        {
            p = highest_priority_proc;

            // Mark process as running
            p->state = RUNNING;
            c->proc = p;

            // Context switch to the selected process
            // When swtch returns, the process has yielded/blocked
            swtch(&c->context, &p->context);

            // Process is done running for now
            c->proc = 0;

            // Release the process lock
            release(&p->lock);
        }
    }
}

#endif // PRIORITY_SCHED
