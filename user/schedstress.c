// schedstress: many processes forking, sleeping, spinning and exiting,
// with concurrent getpinfo() calls. Meant for CPUS=3: it should finish
// without a panic, deadlock or lost process.
#include "kernel/types.h"
#include "kernel/pinfo.h"
#include "user/user.h"

#define ROUNDS  20
#define WORKERS 12

static struct pinfo buf[64];

static void
worker(int id)
{
  for (int i = 0; i < 6; i++) {
    int pid = fork();
    if (pid == 0) {
      // grandchild: mix of CPU and sleep
      if ((id + i) % 3 == 0) {
        int t0 = uptime();
        while (uptime() - t0 < 2)
          ;
      } else if ((id + i) % 3 == 1) {
        pause(1);
      } else {
        getpinfo(buf, 64);
      }
      exit(0);
    }
    if (pid < 0)
      break; // proc table full; fine under stress
    if (i % 2 == 0)
      getpinfo(buf, 64);
  }
  while (wait(0) >= 0)
    ;
  exit(0);
}

int
main(void)
{
  for (int r = 0; r < ROUNDS; r++) {
    int started = 0;
    for (int w = 0; w < WORKERS; w++) {
      int pid = fork();
      if (pid == 0)
        worker(w);
      if (pid > 0)
        started++;
    }
    int reaped = 0;
    while (wait(0) >= 0)
      reaped++;
    if (reaped != started) {
      printf("schedstress: FAIL round %d started %d reaped %d\n",
             r, started, reaped);
      exit(1);
    }
  }

  // Afterwards only init, sh and we should be left.
  int n = getpinfo(buf, 64);
  if (n != 3) {
    printf("schedstress: FAIL %d processes left over\n", n);
    for (int i = 0; i < n; i++)
      printf("  pid %d state %d %s\n", buf[i].pid, buf[i].state, buf[i].name);
    exit(1);
  }
  printf("schedstress: OK\n");
  exit(0);
}
