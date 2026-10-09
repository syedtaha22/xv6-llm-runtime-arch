/* PID 1 for the Linux baseline guest.
 *
 * Reads /bench.txt (one run per line: "<test> <threads> <rep> <steps> <prompt>"),
 * runs /run for each line with OMP_NUM_THREADS=<threads>, and powers the guest
 * off when done.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
  mount("proc", "/proc", "proc", 0, NULL);
  FILE *f = fopen("/bench.txt", "r");
  if (!f) {
    printf("INIT: /bench.txt missing\n");
    sync();
    reboot(RB_POWER_OFF);
    return 1;
  }
  printf("INIT: ready\n");
  char line[1024];
  while (fgets(line, sizeof line, f)) {
    char test[16], steps[16], threads[16], rep[16];
    char *p = line;
    int n = 0;
    if (sscanf(p, "%15s %15s %15s %15s %n", test, threads, rep, steps, &n) < 4)
      continue;
    char *prompt = p + n;
    prompt[strcspn(prompt, "\r\n")] = 0;
    printf("RUN test=%s threads=%s rep=%s steps=%s\n", test, threads, rep, steps);
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
      setenv("OMP_NUM_THREADS", threads, 1);
      char *argv[] = {"/run", "/stories15M.bin",
                      "-z",   "/tokenizer.bin",
                      "-t",   "0.0",
                      "-s",   "123",
                      "-n",   steps,
                      "-i",   prompt,
                      NULL};
      execv("/run", argv);
      _exit(127);
    }
    int st;
    waitpid(pid, &st, 0);
    printf("END test=%s threads=%s rep=%s status=%d\n", test, threads, rep, st);
    fflush(stdout);
  }
  printf("BENCH_DONE\n");
  fflush(stdout);
  sync();
  reboot(RB_POWER_OFF);
  return 0;
}
