#!/usr/bin/python

import sys
import shutil
import os
import subprocess

def print_usage():
    print "./join_subtree.py subtreeprefix outdir" 


def logKey(logname):

    if "." in logname:        
        val = logname.split(".")[-1]
        return int(val)
    return 0


def main():
    if len(sys.argv) < 3:
        print_usage()
        return -1

    round_dir = sys.argv[1]
    outdir = sys.argv[2]


    subprocess.call(["/bin/mkdir", "-p", outdir])
    curr_offset = 0

    for path in sorted(os.listdir(round_dir), key=logKey):
        print path, "starts at", curr_offset 
        ccount = 0

        for epoch in os.listdir(os.path.join(round_dir, path)):
            fullpath = os.path.join(os.path.join(round_dir, path, epoch))
            newpath = ""
            if "output" in epoch:
                newpath = "{}/output.{}".format(outdir, curr_offset)
            elif "global" in epoch:
                newpath = "{}/global.{}".format(outdir, curr_offset)
            else:
                suboffset = int(epoch.split(".")[-1])
                subbase = ".".join(epoch.split(".")[:-1])
                newpath = "{}/{}.{}".format(outdir, subbase, suboffset + curr_offset)
                
                if suboffset > ccount:
                    ccount = suboffset

            print "copying", fullpath, "to",newpath
            shutil.copyfile(fullpath, newpath)
    

        curr_offset += ccount + 1

main()
