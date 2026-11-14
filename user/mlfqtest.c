// mlfqtest: checks MLFQ demotion, preemption by new processes,
// time-not-forgiven, and the 64-tick boost. Run with CPUS=1.
#include "kernel/types.h"
#include "kernel/pinfo.h"
#include "user/user.h"

#define MAXP 64

static struct pinfo buf[MAXP];

static int
level_of(int pid)
{
  int n = getpinfo(buf, MAXP);
  for (int i = 0; i < n; i++)
    if (buf[i].pid == pid)
      return buf[i].level;
  return -1;
}

static void
fail(char *msg)
{
  printf("mlfqtest: FAIL %s\n", msg);
  exit(1);
}

static void
spin(int nticks)
{
  int t0 = uptime();
  while (uptime() - t0 < nticks)
    ;
}

// Sleep across the next boost. A process running at the boost tick is
// boosted and then charged that same tick, which uses up level 0's
// 1-tick slice; sleeping through it leaves us at level 0, used 0.
static void
wait_boost(void)
{
  int t = uptime();
  pause(64 - t % 64 + 1);
}

// A CPU hog drops to level 3 after 1+2+4 = 7 ticks of CPU.
static void
test_demote(void)
{
  int me = getpid();
  // Start right after a boost so no boost interferes.
  wait_boost();
  if (level_of(me) != 0)
    fail("not at level 0 after boost");
  spin(10);
  int lv = level_of(me);
  printf("mlfqtest: after 10 ticks of spinning, level %d\n", lv);
  if (lv != 3)
    fail("hog should be at level 3");
}

// A process that sleeps keeps its level and used time.
static void
test_not_forgiven(void)
{
  int me = getpid();
  wait_boost();
  // Use most of level 0..1 by alternating short spins and sleeps.
  for (int i = 0; i < 8; i++) {
    int t0 = uptime();
    while (uptime() == t0)
      ;
    pause(1);
  }
  int lv = level_of(me);
  printf("mlfqtest: sleeper after 8 ticks of CPU, level %d\n", lv);
  if (lv < 2)
    fail("sleeping should not reset used time");
}

// A new child starts at level 0 and preempts its level-3 parent.
static void
test_preempt(void)
{
  int fds[2];
  wait_boost();
  spin(10); // parent now at level 3
  pipe(fds);
  int start = uptime();
  int pid = fork();
  if (pid == 0) {
    int t = uptime();
    write(fds[1], &t, sizeof(t));
    exit(0);
  }
  spin(3); // keep the CPU busy; the child should still get in
  int t;
  read(fds[0], &t, sizeof(t));
  close(fds[0]);
  close(fds[1]);
  wait(0);
  printf("mlfqtest: child ran %d ticks after fork\n", t - start);
  if (t - start > 2)
    fail("new process was not scheduled ahead of a level-3 hog");
}

// The boost brings a hog back to level 0.
static void
test_boost(void)
{
  int me = getpid();
  spin(10);
  if (level_of(me) != 3)
    fail("hog should be at level 3 before boost");
  // Spinning through the boost tick, we land on level 0 and are then
  // charged that tick, so we should see level 1 right after the boost.
  int seen = -1;
  int t0 = uptime();
  while (uptime() - t0 < 70) {
    int lv = level_of(me);
    if (lv < 3) {
      seen = lv;
      break;
    }
  }
  printf("mlfqtest: boost observed: level 3 -> %d\n", seen);
  if (seen < 0)
    fail("no boost within 70 ticks");
}

int
main(void)
{
  test_demote();
  test_not_forgiven();
  test_preempt();
  test_boost();
  printf("mlfqtest: OK\n");
  exit(0);
}
