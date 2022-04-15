/* Copyright (C) 2011 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Chris Metcalf <cmetcalf@tilera.com>, 2011.

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

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <kernel_stat.h>

#include <sysdep.h>
#include <sys/syscall.h>
#include <kernel-features.h>

#include "overflow.h"

/* Get information about the file NAME in BUF.  */
int
__fxstatat (int vers, int fd, const char *file, struct stat *buf, int flag)
{
  if (vers == _STAT_VER_KERNEL)
    {
      int rc = INLINE_SYSCALL (fstatat64, 4, fd, file, buf, flag);
      return rc ?: stat_overflow (buf);
    }

  errno = EINVAL;
  return -1;
}
libc_hidden_def (__fxstatat)
