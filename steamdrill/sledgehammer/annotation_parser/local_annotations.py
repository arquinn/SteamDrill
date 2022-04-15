#!/usr/bin/python

import sys
import subprocess

def print_usage():
    print "local_annotations.py " 

def get_loc(diff_header):
    #assume it goes <new_line>d<old_line>
    return diff_header.split('d')[1]

def find_tracepoints(string):
    prev = ""
    locs = {}
    for line in string.split("\n"):
        if "TRACEPOINT" in line:
            locs[get_loc(prev)] = line
        prev = line

    return locs


def tracepoint_format(tpline, old_file, tpmsg):
    req = old_file + ":" + tpline
    
    no_header = tpmsg.split("(")[1]
    no_header = no_header.split(")")[0]

    vals = no_header.split(",")
    req += " " + vals[0]

    for i in range(1, len(vals), 2):
        req += " " + vals[i] + "," + vals[i+1]
    
    return req



def main():
    if len(sys.argv) < 2:
        print_usage()
        return -1

    new_file = sys.argv[1]
    old_file = sys.argv[2]


    #first get all of the differences
    try:        
        diff = subprocess.check_output(["/usr/bin/diff", new_file, old_file])
    except subprocess.CalledProcessError as e:
        diff = e.output

    tracepoints = find_tracepoints(diff)



    for t in tracepoints:
        print tracepoint_format(t, old_file, tracepoints[t])


    return 0


main()
