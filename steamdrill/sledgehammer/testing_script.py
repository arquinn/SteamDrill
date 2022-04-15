#!/usr/bin/python

##this is a testing script. There are not many like it, and this one is mine: 
import sys
import argparse

class mem_map: 
    def __init__(self, start, name): 
        self.start = start
        self.name = name

    def __lt__(self, other): 
        return self.start < other.start

    def __str__(self):
        return hex(self.start) + " " + str(self.name)
        
def build_mem_map(mem_file): 
    ## this is broken in that it only adds a single anon. It's usable
    ## for now though.

    maps = {}
    for line in mem_file:
        words = line.split()
        bounds = words[3].split("-")
        region = "anon"
        if len(words) == 9:
            region = words[8]
        
        if region == "anon" or region not in maps:
            maps[region] = mem_map(int(bounds[0], 16), region)

    return sorted(maps.values())

def main(): 
    parser = argparse.ArgumentParser(description="test some scripts and whatnot")
    parser.add_argument('mem_maps', metavar='LOG')
    parser.add_argument('log', metavar='LOG')

    args = parser.parse_args()

    mem_maps = None
    with open(args.mem_maps, "r") as mem_file:
        mem_maps = build_mem_map(mem_file)
    

    i = 1
    with open(args.log, "r") as log_file:
        #figure out where the line belongs:
        for line in log_file:
            entries = line.split()
            eip = entries[2]
            val = int(eip[len("[eip]:"):], 16)
            special = None
            for region in mem_maps:
                if region.start > val:
                    break
                special = region
            if special == None:
                print "what the fuck?", i, line
            print i,hex(val - special.start), special.name
            i += 1


main()
