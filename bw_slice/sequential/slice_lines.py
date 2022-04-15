from __future__ import with_statement
import gdb

all_lines = {}

def source_line(nodestring, do_caching):

    line_str = "list *" + nodestring
    try:
        s =  gdb.execute(line_str, False, True)    
        str_lines = s.split("\n")            
        
        if " is in " in s:
            meta_line = str_lines[0].split("(")[1].rstrip(").")
        else:
            meta_line = str_lines[0].split("is at")[1].rstrip(".")
        
        inst =  "(0x" + nodestring + ")"
        line = " ".join(str_lines[1].split()[1:])
        
    except gdb.error as e:
        meta_line = str(e)
        inst = ""
        line = ""

    if meta_line in all_lines:
        return ""
    
    if do_caching:
        all_lines[meta_line] = True
    return meta_line + inst + ": " + line 


class Slice (gdb.Command):
    """Convert from the machine generated slice file
    into a readable file by ananlyzing the state of the program"""

    def __init__(self):
        super (Slice, self).\
            __init__("slice",\
                         gdb.COMMAND_SUPPORT,\
                         gdb.COMPLETE_NONE)

    def invoke (self, arg, from_tty):
        listsize_str = "set listsize 1" 
        gdb.execute(listsize_str)
        infile, outfile = arg.split()

        print "in",infile, "out",outfile

        with open (infile, 'r') as f:
            with open(outfile, 'w+') as f2:
                ips = False
                
                for l in f:
#                    print l.strip()

                    if len(l.strip()) == 0:
                        continue
                    elif l.strip() == "criteria:":
                        continue
                    elif not ips and l.strip() == "ips:":
                        ips = True
                        print "lines:"
                    elif not ips:
                        w = l.split(":")
                        print>>f2, "Criteria:",w[0], "@",w[1],\
                            "last modified: (",source_line(w[2].strip(),False), ")"
                    else:
                        s = source_line(l.strip(), True)
                        if len(s) > 0:
                            print>>f2, s


                
Slice()
            
