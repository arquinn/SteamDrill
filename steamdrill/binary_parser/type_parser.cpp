

optional<VariableType> resolveType(Dwarf_Die &child)
{
  Dwarf_Die typeDie, curr = child;
  Dwarf_Attribute attr;
  int rc;
  uint64_t size = 0;
  bool resolved = false;
  deque<string> stack;

  do {
    rc = (int) dwarf_attr(&curr, DW_AT_type, &attr);
    if (rc == 0) break;
    rc = (int) dwarf_formref_die(&attr, &typeDie);
    if (rc == 0) break;

    rc = dwarf_peel_type(&typeDie, &curr);
    if (rc != 0) break;

    switch (dwarf_tag(&curr))
    {
      // treat arrays as pointers.
      case DW_TAG_pointer_type:
      case DW_TAG_array_type:
        stack.push_back("%s*");
        size = 4;
        break;
      case DW_TAG_union_type:
        //case DW_TAG_structure_type:
        return boost::none;
      case DW_TAG_const_type:
        // ignore const
        break;
      default:
        if (dwarf_hasattr(&curr, DW_AT_name))
        {
          stack.push_back(dwarf_diename(&curr));
        }
        else
        {
          // from what I've seen, this happens b/c of a struct that is
          // only referenced through a typedef. So, I just ignore these
          // for now.
          return nullptr;
        }
        if (!size)
        {
          rc = (int) dwarf_attr(&curr, DW_AT_byte_size, &attr);
          if (!rc)
            std::cerr << "\tcannot get size?";
          else
            dwarf_formudata(&attr, &size);
        }
        resolved = true;
        break;
    }
  } while(!resolved);

  // hacky way to get a pointer w/out a type to resolve to void*
  std::string name = "void";
  while (stack.size() > 0)
  {
    std::string fmt = stack.back();
    stack.pop_back();
    if (fmt.find('%') != string::npos)
      name = boost::str(format(fmt) %name);
    else
      name = fmt;
  }
  if (size > 8)
    return boost::none;
  return VariableType((int)size, name);
  // typeDie = dwfl_addrdie(_dwfl, addr, NULL);
}
