#!/usr/bin/python
import os
import sys


def print_usage():
    print "./compare_altrace_sets.py <dir> <num_epochs>"



def get_start(filename):
    lines = []
    with open(filename, "r") as infile:
        for line in infile:
            if line.startswith("("):
                break
            if line.strip != "":
                lines.append(line.strip())

    return lines

def get_end(filename):
    lines = []
    with open(filename, "r") as infile:
        for line in reversed(infile):
            if line.startswith("("):
                break
            if line.strip != "":
                lines.append(line.strip())

    return [l for l in reversed(lines)]


def compare_files(end, start):
    print "comparing",end,"and",start
    elines = get_end(end)
    slines = get_start(start)

    for el, sl in zip(elines, slines):
        if el != sl:
            print "oh no!",el, sl
    

def main():
    if len(sys.argv) < 2:
        print_usage()
        return -1

    parallel_dir = sys.argv[1]
    num_epochs = int(sys.argv[2])

    for i in range(num_epochs - 1):
        compare_files(os.path.join(parallel_dir, "altrace.{}".format(i)),\
                      os.path.join(parallel_dir, "altrace.{}".format(i+1)))

    print "SAME"
    return 0



main()



