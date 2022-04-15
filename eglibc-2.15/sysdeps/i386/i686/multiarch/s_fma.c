/* Multiple versions of fma.
   Copyright (C) 2010 Free Software Foundation, Inc.
   Contributed by Intel Corporation.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA. */

#include <config.h>

#ifdef HAVE_AVX_SUPPORT
#include <math.h>
#include <init-arch.h>

extern double __fma_ia32 (double x, double y, double z) attribute_hidden;
extern double __fma_fma (double x, double y, double z) attribute_hidden;

libm_ifunc (__fma, HAS_FMA ? __fma_fma : __fma_ia32);
weak_alias (__fma, fma)

# define __fma __fma_ia32
#endif

#include <sysdeps/ieee754/ldbl-96/s_fma.c>
