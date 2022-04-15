from __future__ import with_statement
import gdb
import re

#determine nodes w/ same line
line_to_node = {}

#used to label edges
nodeid_to_node = {}

#used to do deduplicating for line invocations
nodeid_to_nodeid = {}

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
        return self.memloc.strip() +":" + self.value.strip()

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

def build_dedup(line, nodes):

    start_nodes = []
    dynamicids_to_nodeid = {}

    #we need to figure out how many independent calls to this line we have here:
    for n in nodes:
        start_did = n.get_linestart_dynid()
        print n.nodeid, start_did
        assert(start_did > 0 and "line 145")

        nodeid = n.nodeid
        if start_did in dynamicids_to_nodeid:
            nodeid = dynamicids_to_nodeid[start_did]
        else:
            dynamicids_to_nodeid[start_did] = nodeid
            start_nodes.append(n)

    
        nodeid_to_nodeid[n.nodeid] = nodeid

    
    return start_nodes

def dump_nodes(outf):
#    for line in line_to_node:
#        print line,
#        for node in line_to_node[line]:
#            print node.nodeid,
#        print 
#    print
#    print

    for line in line_to_node:
        nodes = build_dedup(line, line_to_node[line])
        for n in nodes:

            identifier = n.nodeid
            label = n.nodeid + ": " + n.line +"\\n" + n.source
            print>>outf, "x" + identifier +"[label=\"" + label + "\", color=Green]"

        print

def add_links(outf):
    for line in line_to_node:
        for node in line_to_node[line]:
#            print n.nodeid,"to",node.p1,node.p2
            alias = nodeid_to_nodeid[node.nodeid] 

            for n in node.parents:

                if n in nodeid_to_nodeid:
                    nid = nodeid_to_nodeid[n]
                    n_node = nodeid_to_node[n]
                
                    print "adding link from",alias,"to",nid
                    print n_node.get_link_string()
                    
                    print>>outf, "x"+ alias +"-> x" + nid + "[label=\""\
                        + n_node.get_link_string() + "\" dir=back]"

                    

class ReadableSlice (gdb.Command):
    """Convert from the machine generated slice file
    into a readable file by ananlyzing the state of the program"""

    def __init__(self):
        super (ReadableSlice, self).\
            __init__("slice",\
                         gdb.COMMAND_SUPPORT,\
                         gdb.COMPLETE_FILENAME)

    def invoke (self, arg, from_tty):
        listsize_str = "set listsize 1" 
        gdb.execute(listsize_str)
        #[infile, outfile] = arg.split()
        infile = "{}/nodeids".format(arg)
        outfile = "{}/flowback".format(arg)

        print "in",infile,"out",outfile

        
        with open (infile, 'r') as f:
            for l in f:
                n = node_obj(l)
                if n.nodeid in nodeid_to_node:
                    nodeid_to_node[n.nodeid].add_node_parents(l)
                    continue
                else:
                    nodeid_to_node[n.nodeid] = n

                if n.line not in line_to_node:
                    line_to_node[n.line] = [n] 
                else:
                    line_to_node[n.line].append(n)

        with open(outfile, "w") as outf:
            print >> outf, "digraph g {\n"
            dump_nodes(outf)
            add_links(outf)
            print >> outf, "}"

                
ReadableSlice()
            
