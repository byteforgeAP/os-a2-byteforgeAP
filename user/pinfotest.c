// pinfotest: checks the Part 1 counters and getpinfo().
// Run with CPUS=1 for meaningful runticks numbers.
#include "kernel/types.h"
#include "kernel/pinfo.h"
#include "user/user.h"

#define MAXP 64

static struct pinfo buf[MAXP];

static int
findself(struct pinfo *out)
{
  int n = getpinfo(buf, MAXP);
  int me = getpid();
  for (int i = 0; i < n; i++) {
    if (buf[i].pid == me) {
      *out = buf[i];
      return 0;
    }
  }
  return -1;
}

static void
fail(char *msg)
{
  printf("pinfotest: FAIL %s\n", msg);
  exit(1);
}

int
main(void)
{
  struct pinfo a, b;

  // getpinfo argument checks.
  if (getpinfo(buf, -1) != -1)
    fail("max < 0 should return -1");
  if (getpinfo(buf, 0) != 0)
    fail("max == 0 should return 0");
  if (getpinfo((struct pinfo *)0xffffffffff, 4) != -1)
    fail("bad pointer should return -1");
  int n = getpinfo(buf, MAXP);
  if (n < 3)
    fail("expected at least init, sh, pinfotest");
  if (getpinfo(buf, 1) != 1)
    fail("max == 1 should return 1");
  for (int i = 1; i < n; i++)
    if (buf[i].pid == 0 || buf[i].state == 0)
      fail("reported an UNUSED slot");

  // Spin for ~20 ticks; we should be charged roughly every tick.
  if (findself(&a) < 0)
    fail("self not found");
  int t0 = uptime();
  while (uptime() - t0 < 20)
    ;
  if (findself(&b) < 0)
    fail("self not found");
  int got = b.runticks - a.runticks;
  printf("pinfotest: spun 20 ticks, charged %d, nsched %d -> %d, level %d\n",
         got, a.nsched, b.nsched, b.level);
  if (got < 15)
    fail("runticks did not grow about once per tick");
  if (b.nsched < 1)
    fail("nsched should be positive");
  if (b.ctime > t0)
    fail("ctime is after we started spinning");

  // A child does not inherit counters.
  int pid = fork();
  if (pid < 0)
    fail("fork");
  if (pid == 0) {
    struct pinfo c;
    if (findself(&c) < 0)
      fail("child not found");
    if (c.runticks > 2 || c.nsched < 1 || c.nsched > 3 || c.level != 0 ||
        c.ctime < b.ctime) {
      printf("child: runticks %d nsched %d level %d ctime %d\n",
             c.runticks, c.nsched, c.level, c.ctime);
      fail("child counters did not start fresh");
    }
    exit(0);
  }
  int st;
  wait(&st);
  if (st != 0)
    exit(1);

  printf("pinfotest: OK\n");
  exit(0);
}
