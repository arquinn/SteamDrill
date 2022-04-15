#!/usr/bin/python

import os
import sys


def print_usage():
    print "./validate_output.py <correct> <paralllel_dir>"


def cmp_files(f1_name, f2_name):
    if f1_name.startswith("output."):
        if f2_name.startswith("output."):
            f1_size = int(f1_name[len("output."):])
            f2_size = int(f2_name[len("output."):]) 
            return f1_size - f2_size

        else:
            return 1

    else:
        if f2_name.startswith("output."):
            return -1
        return 1


def get_next_output(output):
    while True:
        try:
            ovalue = output.next()
        except StopIteration:
            return None
        
        if ovalue.startswith("output."):
            return ovalue        

def open_next_file(output_iter, parallel_dir):
    output_filename = get_next_output(output_iter)
    if output_filename == None:
        return None
    
    print "opening", os.path.join(parallel_dir,output_filename)
    output_file = open(os.path.join(parallel_dir,output_filename), "r")
    output_file_iter = iter(output_file)
    return output_file_iter
    
def main():
    if len(sys.argv) < 2:
        print_usage()
        return -1

    correct = sys.argv[1]
    parallel_dir = sys.argv[2]
    print correct,parallel_dir,"...",
    outputs = os.listdir(parallel_dir)
    outputs.sort(cmp_files)

    output = iter(outputs)
    output_file_iter = open_next_file(output, parallel_dir)
    if output_file_iter is None:
        print parallel_dir,"has no output?"
        return 1

    oline = output_file_iter.next()

    index = 0
    with open(correct, "r") as c_file:        
        for cline in c_file:
            if cline != oline:
                print "problem", index, "\n",cline, oline
                return 1
            index += 1
            try:
                oline = output_file_iter.next()
            except StopIteration:
                output_file_iter = open_next_file(output, parallel_dir)
                if output_file_iter == None:
                    "better be the end??"
                    oline = ""
                else:
                    oline = output_file_iter.next()
            
            
    print "SAME"
    return 0



main()



