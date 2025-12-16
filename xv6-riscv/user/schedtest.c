/**
 * @file schedtest.c
 * @brief Test program to compare different CPU scheduling algorithms in xv6.
 * @author Hamna Sajid
 * @date December 16, 2025
 *
 * @details
 * This program creates multiple child processes with different workload characteristics
 * to test and compare the behavior of different CPU schedulers (Round Robin, Priority, MLFQ).
 *
 * Test Types:
 * 1. CPU-bound test: Processes that perform intensive computation
 * 2. Mixed workload: Combination of CPU and I/O operations
 * 3. Priority test: (Priority scheduler only) Tests priority ordering
 *
 * Usage:
 * - Compile and run with default Round Robin: make qemu, then run 'schedtest'
 * - Compile and run with Priority scheduler: make qemu-priority, then run 'schedtest'
 * - Compile and run with MLFQ scheduler: make qemu-mlfq, then run 'schedtest'
 *
 * Expected Behaviors:
 * - Round Robin: All processes get equal time slices, fair distribution
 * - Priority: Lower priority values run first, potential starvation
 * - MLFQ: Interactive (I/O-bound) processes stay in high-priority queues,
 *         CPU-bound processes gradually move to low-priority queues
 */

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NUM_CHILDREN 5                  ///< Number of child processes to create
#define CPU_WORK_ITERATIONS 5000000000L ///< Iterations for CPU-bound work
#define IO_SLEEP_TICKS 10               ///< Sleep duration for I/O simulation

/**
 * @brief Perform CPU-intensive work (computation).
 * @param iterations Number of loop iterations to perform.
 *
 * @details
 * Simulates a CPU-bound process by performing busy-loop computation.
 * This type of workload should:
 * - In RR: Get equal time slices with other processes
 * - In Priority: Run based on priority value
 * - In MLFQ: Gradually move to lower-priority queues
 */
void cpu_intensive_work(long iterations)
{
    volatile long sum = 0;
    for (long i = 0; i < iterations; i++)
    {
        sum += i;
        // Occasionally yield to show cooperation
        if (i % 100000 == 0)
        {
            // yield();  // Optional: can uncomment to test yielding behavior
        }
    }
}

/**
 * @brief Perform I/O-bound work (sleep simulation).
 * @param sleep_ticks Number of ticks to sleep.
 *
 * @details
 * Simulates an I/O-bound process by periodically sleeping (blocking).
 * This type of workload should:
 * - In RR: Yield CPU while sleeping, resume with same priority
 * - In Priority: Maintain priority while blocked
 * - In MLFQ: Stay in high-priority queues (rewarded for I/O behavior)
 */
void io_intensive_work(int sleep_ticks)
{
    for (int i = 0; i < 10; i++)
    {
        printf("IO process: iteration %d\n", i);
        // Simulate I/O by doing a blocking operation (write to fd 1)
        // Then busy-wait to simulate I/O delay
        int start = uptime();
        while (uptime() - start < sleep_ticks)
        {
            // Busy wait
        }
    }
}

/**
 * @brief Test Case 1: CPU-bound processes.
 *
 * @details
 * Creates multiple CPU-bound processes that perform intensive computation.
 * Useful for observing:
 * - Time slice distribution in Round Robin
 * - Priority enforcement in Priority scheduler
 * - Queue demotion in MLFQ
 */
void test_cpu_bound()
{
    printf("\n=== Test 1: CPU-Bound Processes ===\n");
    printf("Assigning priorities: Child 0=0 (highest), Child 1=10 (high), Child 2=20 (med), Child 3=30 (low), Child 4=31 (lowest)\n");
    printf("Starting all processes...\n\n");

    int priorities[] = {0, 10, 20, 30, 31}; // Wider spread for better differentiation
    int pipes[NUM_CHILDREN][2];             // One pipe per child

    // Create pipes for each child
    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        if (pipe(pipes[i]) < 0)
        {
            printf("Pipe creation failed\n");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        int pid = fork();

        if (pid < 0)
        {
            printf("Fork failed\n");
            exit(1);
        }

        if (pid == 0)
        {
            // Child process: Close read ends of all pipes
            for (int j = 0; j < NUM_CHILDREN; j++)
            {
                close(pipes[j][0]);
                if (j != i)
                    close(pipes[j][1]); // Close write ends of other pipes
            }

            // Set priority based on child number
            int priority = priorities[i];
            setpriority(0, priority); // 0 = current process

            uint start_time = uptime();
            cpu_intensive_work(CPU_WORK_ITERATIONS);
            uint end_time = uptime();

            // Send result to parent via pipe
            int result[4]; // child_id, pid, priority, ticks
            result[0] = i;
            result[1] = getpid();
            result[2] = priority;
            result[3] = end_time - start_time;

            write(pipes[i][1], result, sizeof(result));
            close(pipes[i][1]);
            exit(0);
        }
    }

    // Parent: Close write ends and read results
    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        close(pipes[i][1]);
    }

    printf("Child    PID      Priority     Time (ticks)\n");
    printf("----------------------------------------------------\n");

    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        int result[4];
        read(pipes[i][0], result, sizeof(result));
        printf("%d        %d        %d            %d\n", result[0], result[1], result[2], result[3]);
        close(pipes[i][0]);
    }

    // Wait for all children to complete
    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        wait(0);
    }

    printf("\n=== Test 1 Complete ===\n");
}

/**
 * @brief Test Case 2: Mixed workload (CPU and I/O).
 *
 * @details
 * Creates a mix of CPU-bound and I/O-bound processes.
 * Useful for observing:
 * - How schedulers handle heterogeneous workloads
 * - Interactive process responsiveness in MLFQ
 * - Fairness across different process types
 */
void test_mixed_workload()
{
    printf("\n=== Test 2: Mixed Workload (CPU + I/O) ===\n");
    printf("Assigning priorities: I/O processes=5 (high), CPU processes=20 (low)\n");
    printf("Starting mixed workload...\n\n");

    int pipes[NUM_CHILDREN][2]; // One pipe per child

    // Create pipes for each child
    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        if (pipe(pipes[i]) < 0)
        {
            printf("Pipe creation failed\n");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        int pid = fork();

        if (pid < 0)
        {
            printf("Fork failed\n");
            exit(1);
        }

        if (pid == 0)
        {
            // Child process: Close read ends of all pipes
            for (int j = 0; j < NUM_CHILDREN; j++)
            {
                close(pipes[j][0]);
                if (j != i)
                    close(pipes[j][1]); // Close write ends of other pipes
            }

            int result[4]; // child_id, pid, priority, type (0=CPU, 1=I/O)
            result[0] = i;
            result[1] = getpid();

            if (i % 2 == 0)
            {
                // Even children: CPU-bound (lower priority)
                setpriority(0, 20); // Low priority for CPU-bound
                result[2] = 20;
                result[3] = 0; // 0 = CPU-bound
                cpu_intensive_work(CPU_WORK_ITERATIONS / 2);
            }
            else
            {
                // Odd children: I/O-bound (higher priority)
                setpriority(0, 5); // High priority for I/O-bound
                result[2] = 5;
                result[3] = 1; // 1 = I/O-bound
                // Simplified I/O work without printing
                for (int j = 0; j < 10; j++)
                {
                    int start = uptime();
                    while (uptime() - start < IO_SLEEP_TICKS)
                    {
                        // Busy wait
                    }
                }
            }

            // Send result to parent via pipe
            write(pipes[i][1], result, sizeof(result));
            close(pipes[i][1]);
            exit(0);
        }
    }

    // Parent: Close write ends and read results
    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        close(pipes[i][1]);
    }

    printf("Child    PID      Priority     Type\n");
    printf("----------------------------------------------------\n");

    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        int result[4];
        read(pipes[i][0], result, sizeof(result));
        char *type = (result[3] == 0) ? "CPU-bound" : "I/O-bound";
        printf("%d        %d        %d            %s\n", result[0], result[1], result[2], type);
        close(pipes[i][0]);
    }

    // Wait for all children to complete
    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        wait(0);
    }

    printf("\n=== Test 2 Complete ===\n");
}

/**
 * @brief Test Case 3: Stress test with many processes.
 *
 * @details
 * Creates many processes to stress-test the scheduler.
 * Useful for observing:
 * - Scheduler overhead and scalability
 * - Context switch frequency
 * - System stability under load
 */
void test_stress()
{
    printf("\n=== Test 3: Stress Test (Many Processes) ===\n");
    printf("Creating 10 processes with light CPU workload...\n\n");

    int num_stress = 10;
    int child_info[10][3]; // Store [child_id, pid, read_fd]

    uint test_start = uptime();

    for (int i = 0; i < num_stress; i++)
    {
        int p[2];

        // Create pipe just before fork to minimize open fds
        if (pipe(p) < 0)
        {
            printf("Pipe creation failed for child %d\n", i);
            exit(1);
        }

        int pid = fork();

        if (pid < 0)
        {
            printf("Fork failed\n");
            exit(1);
        }

        if (pid == 0)
        {
            // Child process: close read end, keep write end
            close(p[0]);

            // Light CPU work (10% of full workload)
            uint start_time = uptime();
            cpu_intensive_work(CPU_WORK_ITERATIONS / 10);
            uint end_time = uptime();

            // Send result to parent via pipe
            int result[3]; // child_id, pid, ticks
            result[0] = i;
            result[1] = getpid();
            result[2] = end_time - start_time;

            write(p[1], result, sizeof(result));
            close(p[1]);
            exit(0);
        }
        else
        {
            // Parent: close write end, store read end
            close(p[1]);
            child_info[i][0] = i;
            child_info[i][1] = pid;
            child_info[i][2] = p[0]; // Store read fd
        }
    }

    printf("Child    PID      Time (ticks)\n");
    printf("------------------------------------\n");

    int total_ticks = 0;
    for (int i = 0; i < num_stress; i++)
    {
        int result[3];
        read(child_info[i][2], result, sizeof(result));
        printf("%d        %d        %d\n", result[0], result[1], result[2]);
        total_ticks += result[2];
        close(child_info[i][2]);
    }

    uint test_end = uptime();
    uint total_time = test_end - test_start;
    int avg_ticks = total_ticks / num_stress;

    printf("------------------------------------\n");
    printf("Total wall-clock time: %d ticks\n", total_time);
    printf("Average completion time: %d ticks\n", avg_ticks);
    printf("Total CPU time used: %d ticks\n", total_ticks);

    printf("\n=== Test 3 Complete ===\n");
}

/**
 * @brief Main function - runs all scheduler tests.
 *
 * @details
 * Executes a suite of tests to evaluate the current scheduler's behavior.
 * Results can be compared across different scheduler implementations by
 * rebuilding with different SCHED_FLAG values.
 *
 * @return Exit code 0 on success.
 */
int main(int argc, char *argv[])
{
    printf("\n");
    printf("==========================================\n");
    printf("   CPU Scheduler Test Suite for xv6\n");
    printf("==========================================\n");

    // Run test suite
    test_cpu_bound();
    test_mixed_workload();
    test_stress();

    printf("\n==========================================\n");
    printf("   All Tests Complete!\n");
    printf("==========================================\n\n");

    exit(0);
}
