/**
 * @file sched_mlfq.c
 * @brief Multi-Level Feedback Queue (MLFQ) scheduler implementation for xv6.
 * @author Hamna Sajid
 * @date December 16, 2025
 *
 * @details
 * This file implements a Multi-Level Feedback Queue scheduling algorithm for xv6,
 * based on the classic MLFQ design by Corbato et al. (1962).
 *
 * MLFQ Design Principles:
 * - Multiple priority queues with different time slices
 * - Processes start at the highest priority queue
 * - If a process uses its entire time slice, it moves to a lower priority queue
 * - Shorter time slices for higher priority queues (for interactive processes)
 * - Longer time slices for lower priority queues (for CPU-bound processes)
 * - Periodic priority boost to prevent starvation
 *
 * Queue Configuration:
 * - Queue 0 (Highest): Time slice = 1 tick  (for I/O-bound/interactive)
 * - Queue 1 (Middle):  Time slice = 2 ticks (for mixed workloads)
 * - Queue 2 (Lowest):  Time slice = 4 ticks (for CPU-bound/batch)
 *
 * Priority Boost:
 * Every BOOST_INTERVAL ticks, all processes are moved back to the highest
 * priority queue to prevent starvation and maintain system responsiveness.
 *
 * Compilation:
 * This scheduler is enabled when MLFQ_SCHED is defined at compile time.
 * Use: make SCHED_FLAG=MLFQ qemu
 */

#ifdef MLFQ_SCHED

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];

// MLFQ Configuration Constants
#define NQUEUE 3           ///< Number of priority queues
#define TIME_SLICE_0 1     ///< Time slice for highest priority queue (ticks)
#define TIME_SLICE_1 2     ///< Time slice for middle priority queue (ticks)
#define TIME_SLICE_2 4     ///< Time slice for lowest priority queue (ticks)
#define BOOST_INTERVAL 100 ///< Ticks between priority boosts to prevent starvation

/// Last time a priority boost was performed (in ticks)
static uint64 last_boost_time = 0;

/**
 * @brief Get the time slice duration for a specific queue level.
 *
 * @param queue_level The queue level (0 = highest priority, NQUEUE-1 = lowest)
 * @return Time slice in ticks for the specified queue level
 *
 * @details
 * Returns the appropriate time quantum for each queue:
 * - Higher priority queues get smaller time slices (better for interactive processes)
 * - Lower priority queues get larger time slices (better for CPU-bound processes)
 */
static int
get_time_slice(int queue_level)
{
    switch (queue_level)
    {
    case 0:
        return TIME_SLICE_0;
    case 1:
        return TIME_SLICE_1;
    case 2:
        return TIME_SLICE_2;
    default:
        return TIME_SLICE_2; // Default to lowest queue time slice
    }
}

/**
 * @brief Perform priority boost - move all processes to highest priority queue.
 *
 * @details
 * This function implements the priority boost mechanism to prevent starvation.
 * It resets all processes to the highest priority queue (level 0) and assigns
 * them the corresponding time slice. This ensures that:
 * 1. Low-priority processes don't starve indefinitely
 * 2. Process priorities are periodically reevaluated
 * 3. Interactive processes that were demoted can regain high priority
 *
 * Called periodically every BOOST_INTERVAL ticks.
 */
static void
priority_boost(void)
{
    struct proc *p;

    for (p = proc; p < &proc[NPROC]; p++)
    {
        acquire(&p->lock);

        // Reset to highest priority queue if process exists
        if (p->state != UNUSED)
        {
            p->queue_level = 0;
            p->time_slice = get_time_slice(0);
        }

        release(&p->lock);
    }
}

/**
 * @brief Multi-Level Feedback Queue (MLFQ) scheduler main loop.
 *
 * @details
 * This is the main MLFQ scheduler function that runs on each CPU core.
 * It implements a sophisticated scheduling algorithm that balances:
 * - Interactive process responsiveness (via high-priority queues)
 * - CPU-bound process throughput (via low-priority queues)
 * - Fairness (via priority boost mechanism)
 *
 * Algorithm Flow:
 * 1. Check if priority boost is needed (periodic starvation prevention)
 * 2. Scan queues from highest to lowest priority
 * 3. Select first RUNNABLE process from highest non-empty queue
 * 4. Context switch to selected process
 * 5. Upon return, manage time slice and queue demotion:
 *    - If time slice expired and still runnable: demote to lower queue
 *    - If process blocked for I/O: keep in same queue (reward I/O-bound)
 *
 * Time Slice Management:
 * - Decremented on each timer interrupt while process is running
 * - When exhausted, process is demoted to next lower queue
 * - Replenished when moving to a new queue
 *
 * Queue Selection Priority:
 * Processes in higher-numbered queues only run when all lower-numbered
 * (higher-priority) queues are empty.
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
        // Enable interrupts to allow device I/O and timer interrupts
        intr_on();

        uint64 current_time = ticks;

        // Priority boost: periodically move all processes to highest queue
        // This prevents starvation and maintains system responsiveness
        if (current_time - last_boost_time > BOOST_INTERVAL)
        {
            priority_boost();
            last_boost_time = current_time;
        }

        // Find first runnable process from highest priority queue
        // Scan queues in priority order (0 = highest, NQUEUE-1 = lowest)
        struct proc *selected = 0;

        for (int queue = 0; queue < NQUEUE && selected == 0; queue++)
        {
            for (p = proc; p < &proc[NPROC]; p++)
            {
                acquire(&p->lock);

                // Check if process is runnable and in current queue level
                if (p->state == RUNNABLE && p->queue_level == queue)
                {
                    selected = p;
                    // Don't release lock - we'll use this process
                    break;
                }

                release(&p->lock);
            }
        }

        // If we found a runnable process, switch to it
        if (selected != 0)
        {
            p = selected;

            // Mark process as running
            p->state = RUNNING;
            c->proc = p;

            // Context switch to the selected process
            // When swtch returns, the process has either:
            // 1. Voluntarily yielded (e.g., blocked on I/O)
            // 2. Been preempted by timer interrupt
            swtch(&c->context, &p->context);

            // After context switch back to scheduler

            // Decrement remaining time slice for this queue level
            if (p->time_slice > 0)
            {
                p->time_slice--;
            }

            // Time slice management and queue demotion
            // If time slice expired AND process is still runnable (didn't block):
            if (p->time_slice == 0 && p->state == RUNNABLE)
            {
                // Demote to lower priority queue (if not already at lowest)
                if (p->queue_level < NQUEUE - 1)
                {
                    p->queue_level++;
                }

                // Assign new time slice for the new queue level
                p->time_slice = get_time_slice(p->queue_level);
            }
            // If process blocked for I/O (state != RUNNABLE):
            // Keep it in the same queue - reward I/O-bound behavior
            // Refresh time slice for fairness when it becomes runnable again
            else if (p->state != RUNNABLE && p->state != UNUSED)
            {
                p->time_slice = get_time_slice(p->queue_level);
            }

            // Process is done running for now
            c->proc = 0;

            // Release the process lock
            release(&p->lock);
        }
    }
}

#endif // MLFQ_SCHED
