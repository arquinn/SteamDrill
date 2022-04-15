/* Copyright (C) 2011 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by CodeSourcery.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <execinfo.h>
#include <search.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

static int do_test (void);
#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"

/* Set to a non-zero value if the test fails.  */
volatile int ret;

/* Accesses to X are used to prevent optimization.  */
volatile int x;

/* Called if the test fails.  */
#define FAIL() \
  do { printf ("Failure on line %d\n", __LINE__); ret = 1; } while (0)

/* The backtrace should include at least handle_signal, a signal
   trampoline, noreturn_func, 3 * fn, and do_test.  */
#define NUM_FUNCTIONS 7

/* Use this attribute to prevent inlining, so that all expected frames
   are present.  */
#define NO_INLINE __attribute__((noinline))

jmp_buf b;

void
handle_signal (int signum)
{
  void *addresses[NUM_FUNCTIONS];
  char **symbols;
  int n;
  int i;

  /* Get the backtrace addresses.  */
  n = backtrace (addresses, sizeof (addresses) / sizeof (addresses[0]));
  printf ("Obtained backtrace with %d functions\n", n);
  /*  Check that there are at least seven functions.  */
  if (n < NUM_FUNCTIONS)
    {
      FAIL ();
      longjmp (b, 1);
    }
  /* Convert them to symbols.  */
  symbols = backtrace_symbols (addresses, n);
  /* Check that symbols were obtained.  */
  if (symbols == NULL)
    {
      FAIL ();
      longjmp (b, 1);
    }
  for (i = 0; i < n; ++i)
    printf ("Function %d: %s\n", i, symbols[i]);
  /* Check that the function names obtained are accurate.  */
  if (strstr (symbols[0], "handle_signal") == NULL)
    {
      FAIL ();
      longjmp (b, 1);
    }
  /* Do not check name for signal trampoline.  */
  if (strstr (symbols[2], "noreturn_func") == NULL)
    {
      FAIL ();
      longjmp (b, 1);
    }
  for (i = 3; i < n - 1; i++)
    if (strstr (symbols[i], "fn") == NULL)
      {
	FAIL ();
	longjmp (b, 1);
      }
  /* Symbol names are not available for static functions, so we do not
     check do_test.  */

  longjmp (b, 1);
}

NO_INLINE __attribute__((noreturn)) void
noreturn_func (void)
{
  while (1)
    ;
}

NO_INLINE int
fn (int c)
{
  pid_t parent_pid, child_pid;
  struct sigaction act;

  if (c > 0)
    {
      fn (c - 1);
      return x;
    }

  memset (&act, 0, sizeof (act));
  act.sa_handler = handle_signal;
  sigemptyset (&act.sa_mask);
  sigaction (SIGUSR1, &act, NULL);
  parent_pid = getpid ();

  child_pid = fork ();
  if (child_pid == (pid_t) -1)
    abort ();
  else if (child_pid == 0)
    {
      sleep (1);
      kill (parent_pid, SIGUSR1);
      _exit (0);
    }

  /* In the parent.  */
  if (setjmp (b))
    return 0;
  else
    noreturn_func ();
}

NO_INLINE static int
do_test (void)
{
  fn (2);
  return ret;
}
