#!/usr/bin/python

def find_start_addrs(inp):
    addrs = {}
    for line in inp:
        addrs[line] = 0
        if "(" in line:
            return addrs

def find_end_addrs(inp):
    found_count = 0
    addrs = {}
    for line in inp:
        if "addrs" in line:
            found_count += 1
        elif found_count == 2:
            addrs[line] = 0 

    return addrs

def find_diff(start, end):
    for s in start:
        if s not in end:
            print s,"not in end"
    for e in end:
        if e not in start:
            print e,"not in start"

    

def main():
    
    base = sys.argv[1]
    count = sys.argv[2]
    #starts with "addrs:"

    prev_file = "{}/log.{}.altrace".format(base,0)
    end_addrs = find_end_addrs(prev_file)

    return 0

    for i in range(1, count):
        this_file = "{}/log.{}.altrace".format(base,0)
        start_addrs = find_start_addrs(this_file)
        
        find_diff(start_addrs, end_addrs)
        
        end_addrs = find_end_addrs(this_file)

    


#ends with "addrs:"


main



