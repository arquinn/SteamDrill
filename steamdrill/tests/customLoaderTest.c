#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>

/* In part, this code is based upon glibc, which uses the following copywrite:
   Copyright (C) 1996-2007, 2009, 2010, 2011 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

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



#define MAP_FORMAT "/proc/%d/maps"
void dump_maps() {
  FILE *maps;
  char *line = NULL;
  size_t len = 0, read = 0;
  char map_file[256];

  sprintf(map_file, MAP_FORMAT, getpid());
  maps = fopen(map_file, "r");
  while ((read = getline(&line, &len, maps)) != (size_t)-1 ) {
    printf("%s", line);
  }
  fclose(maps);
}


#define PAGE_SIZE 0x1000
struct filebuf {
  size_t len;
  char buf[512] __attribute__ ((aligned (__alignof (Elf32_Ehdr))));
};


void * map_that_file(int fd, struct filebuf *fbp) {
  const Elf32_Ehdr *header;
  const Elf32_Phdr *phdr;
  const Elf32_Phdr *ph;
  size_t maplength;
  char *errstring;

  // a bunch of things:
  Elf32_Dyn *ld, *dynIter;
  Elf32_Half phnum, ldnum;
  Elf32_Addr map_start, map_end, addr;

  /* This is the ELF header.  We read it in `open_verify'.  */
  header = (Elf32_Ehdr*) fbp->buf;

  /* Extract the remaining details we need from the ELF header
     and then read in the program header table.  */
  if (header->e_type != ET_DYN) {
    errstring = "not a dynamic elf?";
    write(1, errstring, strnlen(errstring, 128));
    return NULL;
  }

  //entry = header->e_entry;
  phnum = header->e_phnum;

  maplength = header->e_phnum * sizeof (Elf32_Phdr);
  if (header->e_phoff + maplength <= (size_t) fbp->len)
    phdr = (Elf32_Phdr*) (fbp->buf + header->e_phoff);
  else {
    phdr = (Elf32_Phdr*) alloca (maplength);
    lseek (fd, header->e_phoff, SEEK_SET);
    if ((size_t) read (fd, (void *) phdr, maplength) != maplength) {
      errstring = "cannot read file data";
      write(1, errstring, strnlen(errstring, 128));
      return NULL;
    }
  }

  /* Scan the program header table, collecting its load commands.  */
  struct loadcmd {
    Elf32_Addr mapstart, mapend, dataend, allocend;
    off_t mapoff;
    int prot;
  } loadcmds[phnum], *c;
  size_t nloadcmds = 0;

  for (ph = phdr; ph < &phdr[phnum]; ++ph) {
    switch (ph->p_type) {
      /* These entries tell us where to find things once the file's
         segments are mapped in.  We record the addresses it says
         verbatim, and later correct for the run-time load address.  */
      case PT_DYNAMIC:
        ld = (Elf32_Dyn*) ph->p_vaddr;
        ldnum = ph->p_memsz / sizeof (Elf32_Dyn);
        break;

      case PT_LOAD:
        /* A load command tells us to map in part of the file.
           We record the load commands and process them all later.  */
        c = &loadcmds[nloadcmds++];
        c->mapstart = ph->p_vaddr & ~(PAGE_SIZE - 1);
        c->mapend = ((ph->p_vaddr + ph->p_filesz + PAGE_SIZE - 1)
                     & ~(PAGE_SIZE - 1));
        c->dataend = ph->p_vaddr + ph->p_filesz;
        c->allocend = ph->p_vaddr + ph->p_memsz;
        c->mapoff = ph->p_offset & ~(PAGE_SIZE - 1);

        c->prot = 0;
        if (ph->p_flags & PF_R)
          c->prot |= PROT_READ;
        if (ph->p_flags & PF_W)
          c->prot |= PROT_WRITE;
        if (ph->p_flags & PF_X)
          c->prot |= PROT_EXEC;
        break;

      case PT_TLS:
        errstring = "OH NO! we don't have any support for thread local storage!";
        write(1, errstring, strnlen(errstring, 256));
        return NULL;
      default:
        fprintf(stderr, "unhandled program_header %d\n", ph->p_type);
    }
  }

  assert (nloadcmds);
  /* Now process the load commands and map segments into memory.  */
  c = loadcmds;

  /* Length of the sections to be loaded.  */
  maplength = loadcmds[nloadcmds - 1].allocend - c->mapstart;

  /* We can let the kernel map it anywhere it likes, but we must have
     space for all the segments in their specified positions relative
     to the first.  So we map the first segment without MAP_FIXED, but
     with its extent increased to cover all the segments.  Then we
     remove access from excess portion, and there is known sufficient
     space there to remap from the later segments.
  */
  map_start = (Elf32_Addr) mmap (NULL, maplength, PROT_NONE, MAP_PRIVATE, fd, c->mapoff);
  if ((void *) map_start == MAP_FAILED) {
    errstring = "failed to map segment from shared object";
    return NULL;
  }
  map_end = map_start + maplength;
  addr = map_start - c->mapstart;


  while (c < &loadcmds[nloadcmds]) {
    if (c->mapend > c->mapstart
        /* Map the segment contents from the file.  */
        && (mmap ((void *) (addr + c->mapstart),
                  c->mapend - c->mapstart, PROT_READ|PROT_WRITE,
                  MAP_FIXED|MAP_PRIVATE,
                  fd, c->mapoff)
            == MAP_FAILED))
      return NULL;

    if (c->allocend > c->dataend) {
      /* Extra zero pages should appear at the end of this segment,
         after the data mapped from the file.   */
      Elf32_Addr zero, zeroend, zeropage;

      zero = addr + c->dataend;
      zeroend = addr + c->allocend;
      zeropage = ((zero + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

      if (zeroend < zeropage)
        /* All the extra data is in the last page of the segment.
           We can just zero it.  */
        zeropage = zeroend;

      if (zeropage > zero) {
        /* Zero the final part of the last page of the segment.  */
        memset ((void *) zero, '\0', zeropage - zero);
      }

      if (zeroend > zeropage) {
        /* Map the remaining zero pages in from the zero fill FD.  */
        void* mapat = mmap ((void*) zeropage, zeroend - zeropage,
                            PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE|MAP_FIXED,
                            -1, 0);
        if (mapat == MAP_FAILED) {
          errstring = "cannot map zero-fill pages";
          return NULL;
        }
      }
    }
    ++c;
  }

  /* We are done mapping in the file.  We no longer need the descriptor.  */
  if (close (fd) != 0) {
    errstring = "cannot close file descriptor";
    return NULL;
  }
  /* Signal that we closed the file.  */
  fd = -1;

  dynIter = (Elf32_Dyn*) ((Elf32_Addr) ld + addr);

  Elf32_Sym *symtab = NULL;
  Elf32_Rel *start = NULL, *end = NULL;
  uintptr_t size = 0;
  while (dynIter->d_tag != DT_NULL) {
    switch (dynIter->d_tag) {
      case DT_REL:
        start = (Elf32_Rel*)(dynIter->d_un.d_ptr + addr);
        break;
      case DT_RELSZ:
        size = dynIter->d_un.d_val;
        break;
      case DT_SYMTAB:
        symtab = (Elf32_Sym *)(dynIter->d_un.d_ptr + addr);
        break;
      default:
        fprintf(stderr, "unhandled tag %d -> %x (%x)\n",
                dynIter->d_tag, dynIter->d_un.d_val, dynIter->d_un.d_ptr);
    }
    // get the values we care about from dyn.
    ++dynIter;
  }
  //TODO: consider (DT_RELCOUNT) as well!
  end = (Elf32_Rel*)((char*)start + size);

  //TODO: handle dependencies! [Do I... HAVE to?]
  for (; start < end; ++start) {
    Elf32_Addr* dest_addr = (Elf32_Addr*) (addr + start->r_offset);
    Elf32_Sym *symbol = symtab + ELF32_R_SYM(start->r_info);

    switch (ELF32_R_TYPE(start->r_info)) {
      case R_386_RELATIVE:
        *dest_addr += addr;
        break;
      case R_386_PC32:
        // we assume that we only have self-contained symbols! (not *really* a good idea)
        *dest_addr += addr + symbol->st_value - (Elf32_Addr)dest_addr;
        break;
      case R_386_32:
        *dest_addr += addr + symbol->st_value;
        break;
      default:
        fprintf(stderr, "offset: %x, info: %x, type: %x\n", start->r_offset, start->r_info, ELF32_R_TYPE(start->r_info));
        break;
    }
  }

  // dis is some hacky bullshit:
  c = loadcmds;
  while (c < &loadcmds[nloadcmds]) {
    if (c->mapend > c->mapstart) {
      mprotect((void*)(addr + c->mapstart), c->mapend - c->mapstart, c->prot);
    }
    ++c;
  }

  // not sure what is?
  return (void*)addr;
}


void * my_dlopen(const char*file) {
  int fd;
  struct filebuf fb;

  // probably this needs to directly call a systemcall to remove dependencies
  fd = open (file, O_RDONLY | O_CLOEXEC);
  fb.len = read (fd, fb.buf, sizeof (fb.buf));

  // probably why does this exist? void *stack_end = __libc_stack_end;
  return map_that_file(fd, &fb);
}


void print_usage() {
  fprintf(stderr, "./DlopenTest <base> <dlopen_offset>\n");
}


typedef  void* (*myFunc)();
int main(int argc, char **argv)
{
  char *filename = NULL;
  uintptr_t offset;
  if (argc < 2) {
    fprintf(stderr, "I need an argument!\n");
    print_usage();
    return -EINVAL;
  }

  filename = argv[1];
  offset = strtol(argv[2], NULL, 0);
  printf("calling %x from %s\n", offset, filename);

  void* loadAddr = my_dlopen(filename);
  // now try calling the function (does wark?)

  //myFunc dlopen;
  void (*fxn)(void);
  *(void **)(&fxn) = (loadAddr + offset);

  (*fxn)();
  return 0;
}
