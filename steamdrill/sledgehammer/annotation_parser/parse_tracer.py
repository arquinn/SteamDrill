from __future__ import with_statement
import gdb
import re

class Breakpoint: 
    def __init__(self, address, function):
        assert(int(address, 16))

        self._address = address
        self._function = function

    def __str__(self):
        return "{} {}".format(self._address, self._function)


class node_obj:
    def __init__(self,line):
        w = line.split(",")
        self.nodeid = w[0]
        self.dynamicid = w[1]
        self.ip = w[2]
        self.parents = [] #what this node derives from
        self.memloc = w[5]
        self.value = w[6]

        self.add_node_parents(line)

        self.line, self.source = self.source_line()

    def add_node_parents(self,line):
        w = line.split(",")
        if w[3] != "0":
            self.parents.append(w[3])
        if w[4] != "0":
            self.parents.append(w[4])


    def source_line(self):
        line_str = "list *0x" + self.ip
        try:
            s =  gdb.execute(line_str, False, True)    
            str_lines = s.split("\n")            
            meta_line = str_lines[0].split()[-1].strip("().")
            
            line = " ".join(str_lines[1].split()[1:])
            line = line.replace('"',r'\"');    
            line = line.replace('\n',r'\\n"');    
            
        except gdb.error:
            meta_line = self.ip
            line = "" #starting state" 
    
        return meta_line,line 

    def get_linestart_dynid(self):
        start = 0
        start_str = ""
        end = 0
        try:
            addr_str = "info line *0x" + self.ip
            s =  gdb.execute(addr_str, False, True)    
            m = re.search("address (0x\w+) <.+?> and ends at (0x\w+)",s)
            if m != None:
                start_str = m.group(1)
                start = int(start_str,16)
                end = int(m.group(2),16)

        except gdb.error:
            print "huh... couldn't get info line..?"
            print self.ip
            return self.dynamicid
#            assert(0)

        if start == 0:
            return self.dynamicid

        index = 0
        ip = int(self.ip,16)        
        try:
            assem_str = "x/" + str(end-start) + "i " + start_str
            s =  gdb.execute(assem_str, False, True)    

            for l in s.split("\n"):
                m = re.search("(0x\w+)[<: ]",l)
                if m != None:
                    addr = int(m.group(1),16)
                    if addr == end:
                        assert(0)
                    elif addr >= ip:
                        return int(self.dynamicid,16) - index
                    else:
                        index += 1
                else:
                    print "no addrs...? ", addr_str, assem_str, l
                    print index
                    assert(0)
                    
            
        except gdb.error:
            print "huh... couldnt' get assembly?"    

            
    def get_link_string(self):
        return self.memloc +":" + self.value

def get_line_addrs(line):

    #step one: get all start and end addrs
    endpoints = []
    addr_str = "info line " + line
    try:
        s =  gdb.execute(addr_str, False, True)    

        for l in s.split("\n"):
            if len(l) > 0:
                m = re.search("address (0x\w+) <.+?> and ends at (0x\w+)",l)
                found = False
                for e in endpoints:
                    if e[0] == m.group(1):
                        found = True
                        break

                if not found:
                    endpoints.append((m.group(1), m.group(2)))

    except gdb.error:
        print "huh... couldn't get info line..?"
                    
    #next, convert start-end addrs to list of all addrs for line
    addrs = []    
    for e in endpoints:
        start = int(e[0],16)
        end = int(e[1],16)
        assem_str = "x/" + str(end-start) + "i " + e[0]
        try:
            s =  gdb.execute(assem_str, False, True)    
            a = []
            for l in s.split("\n"):
                m = re.search("(0x\w+) <",l)
                addr = int(m.group(1),16)
                if addr != end:
                    a.append(addr)
#                    a.append(m.group(1))
                else:
                    break
            addrs.append(a)
                
        except gdb.error:
            print "huh... couldnt' get assembly?"    


    return addrs;



                    
def get_starting_addr(function_symbol):
    addr_str = "info line " + function_symbol
    try:
        string = gdb.execute(addr_str, False, True)
        match = re.search(r"starts at address (0x\w+)", string)
        if match != None:
            addr = match.group(1)
            return addr

    except gdb.error:
        return None

class ParseTracer (gdb.Command):
    """Convert the provided user-level config into an Abott config"""

    def __init__(self):
        super(ParseTracer, self).__init__("parse_config",
                                          gdb.COMMAND_SUPPORT,\
                                          gdb.COMPLETE_FILENAME)

    def invoke (self, arg, from_tty):
        infile_path, output_path = arg.split()
        print "in", infile_path, "output",output_path

        breakpoints = []
        with open(infile_path, 'r') as infile:
            curr_function = ""

            for line in infile:
                stripped_line = line.strip()
                if stripped_line.startswith("Function"):
                    curr_function = stripped_line.split()[1]

                else:
                    if curr_function == "":
                        print "must have a function defined!", line
                        return -1

                    addr = get_starting_addr(line.strip())
                    if addr != None:
                        break_point = Breakpoint(addr, curr_function)
                        breakpoints.append(break_point)

        with open(output_path, "w") as ofile:
            for break_point in breakpoints:
                print>>ofile, break_point
ParseTracer()
