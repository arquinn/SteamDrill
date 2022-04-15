#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

extern "C"
{
#include <dwarf.h>
#include <elf.h>

//why are these at different spots..>? Ubuntu you confuse me!
#include <elfutils/libdw.h>
#include <libelf.h>
}


#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

// internal includes
//#include "../lib/json/single_include/nlohmann/json.hpp"

Dwarf_CFI *cfi;
//std::vector<nlohmann::json> functions;

static const char* indexToReg(int ind)
{
    switch (ind)
    {
    case 4:
	return "esp";
    default:
	return "???";
    }
}

static void get_ops(Dwarf_Op *ops, size_t nops, char *rtn)
{
    assert (nops == 1);
    for (size_t i = 0; i < nops; ++i)
    {
	switch (ops[i].atom)
	{
	case DW_OP_fbreg:
	    sprintf(rtn, "(base + %llu)", ops[i].number);
	    return;
	case DW_OP_bregx:
	    sprintf(rtn, "(%s + %llu)",
		    indexToReg(ops[i].number), ops[i].number2);
	    break;
	}
    }
}

int get_stack_frame(Dwarf_Addr pc, char *sframe)
{
    Dwarf_Frame *frame;
    Dwarf_Op *ops;
    size_t nops;
    dwarf_cfi_addrframe(cfi, pc, &frame);
    dwarf_frame_cfa(frame, &ops, &nops);
    get_ops(ops, nops, sframe);
    return 0;
}

int get_params(Dwarf_Die *parent, char *params)
{
    Dwarf_Die res, typeref;
    Dwarf_Attribute attr;
    Dwarf_Op *ops;
    size_t oplen;

    char param_loc[128], temp[128];

    if (dwarf_child(parent, &res))
    {
	return -1;
    }

    do {
	switch (dwarf_tag(&res))
	{
	case DW_TAG_formal_parameter:
	    dwarf_attr(&res, DW_AT_type, &attr);
	    dwarf_formref_die(&attr, &typeref);
	    dwarf_attr(&res, DW_AT_location, &attr);
	    dwarf_getlocation(&attr, &ops, &oplen);

	    get_ops(ops, oplen, param_loc);

	    sprintf(temp," (%s;%s;%d;%s )", dwarf_diename(&res),
		    dwarf_diename(&typeref),
		    dwarf_bytesize(&typeref), param_loc);

	    strcat(params, temp);

	    break;
	}
    } while (!dwarf_siblingof(&res, &res));
    return 0;
}

nlohmann::json handle_function(Dwarf_Die *res)
{
    int line;
    const char *source_name, *function_name;
    char stack_frame[128] = "", params[1024] = "", source[1024],
	breakp[16];

    Dwarf_Addr pc;
    nlohmann::json rtn;

    source_name = dwarf_decl_file(res);
    function_name = dwarf_diename(res);
    dwarf_decl_line(res, &line);
    dwarf_entrypc (res, &pc);

    //get_stack_frame(pc, stack_frame);
    //get_params(res, params);
    sprintf(source, "%s:%d", source_name, line);

    (rtn)["name"] = function_name;
    (rtn)["source"] = source;
    (rtn)["startIP"] = pc;
    return rtn;
}

static int add_function(Dwarf_Die * die, void *unused)
{
    (void) unused;
    nlohmann::json funcData = handle_function(die);

    functions.push_back(funcData);
    return 0;
}

static int dumpShit(Dwarf *dbg, Dwarf_Global *glb, void *unused)
{
  //how do I figure this stuffs out..?
  printf("%ld %ld %s", glb->cu_offset, glb->die_offset, glb->name);
}

void print_format()
{
    fprintf(stderr, "./funct_extraction <progrname>");
}

nlohmann::json parse_symbol(Elf *elf,
                            Elf32_Shdr *shdr,
                            Elf32_Sym *sym)
{
  nlohmann::json rtn;
  char *name = elf_strptr(elf, shdr->sh_link, sym->st_name);
  rtn["location"] = sym->st_value;
  rtn["name"] = name;
  return rtn;
}

void parse_elf(Elf *elf)
{
  Elf_Scn *escn;
  Elf32_Ehdr *ehdr;

  char *name;
  int rc;


  ehdr = elf32_getehdr(elf);
  assert(ehdr != NULL);

  //iterate over all elf sections
  escn = NULL;
  while ((escn = elf_nextscn(elf, escn)) != NULL)
  {
    Elf32_Shdr * shdr = NULL;
    shdr = elf32_getshdr(escn);
    assert(shdr != NULL);

    if (shdr->sh_type == SHT_SYMTAB)
    {
      Elf_Data *data = NULL;
      Elf32_Sym *sym = NULL;
      u_int entries = 0;
      entries = shdr->sh_size / shdr->sh_entsize;
      data = elf_getdata(escn, data);
      assert(data != NULL);

      for (u_int i = 0; i < entries; ++i)
      {
        sym = &((Elf32_Sym *)data->d_buf)[i];
        if (ELF32_ST_TYPE(sym->st_info) == STT_FUNC)
        {
          nlohmann::json funcData = parse_symbol(elf, shdr, sym);
          functions.push_back(funcData);
        }
      }
    }
  }
}

int main(int argc, char** argv)
{
    Dwarf *dbg;
    Elf   *elf;
    Dwarf_Off offset = 0, last_offset;
    size_t hdr_size;

    const char* progname;
    int fd = -1;

    if (argc < 2) {
	print_format();
        return -1;
    }

    progname = argv[1];

    if ((fd = open(progname, O_RDONLY)) < 0)
    {
        perror("open");
        return -2;
    }


    if (elf_version(EV_CURRENT) == EV_NONE)
    {
      printf("can't init elf version %s\n", elf_errmsg(-1));
      return -3;
    }

    elf = elf_begin(fd, ELF_C_READ, NULL);
    if (elf == NULL) {
      printf("ugh, can't elf_begin %s\n", elf_errmsg(-1));
      return -4;
    }
    parse_elf(elf);

    nlohmann::json arr(functions);
    std::cout << arr << std::endl;

    close(fd);
    return 0;
}
