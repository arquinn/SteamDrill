#!/usr/bin/python
"""Module to run a logging test"""

import argparse
import os
import benchmark
import server

def logKey(logname):

    if "." in logname:        
        val = logname.split(".")[-1]
        return int(val)
    return 0

def get_output_files(d):
    files = []
    for path in sorted(os.listdir(d), key=logKey):
        if "output" in path:
            files.append(os.path.join(d, path))
    return files

def ship_output_files(files, hosts, host_file):

    with open(host_file, "w") as hfile: 
        for ofile, host in zip(files, hosts):
            shm_path = "/dev/shm/{}".format(os.path.basename(ofile))
            host.send_file(ofile, shm_path)
            hfile.write("{}\n".format(shm_path))

def get_hosts(host_filename):
    """Get the hosts from a host file"""
    hosts = []
    with open(host_filename, "r") as rfile:
        for line in rfile:
            words = line.split()
            serv = server.Server(words[1])
            hosts.append(serv)

    return hosts

def main():
    """main function for running a test"""

    parser = argparse.ArgumentParser()
    parser.add_argument("-t", "--test_name", dest="test", required=True)
    parser.add_argument("--hosts", dest="host_file", required=True)
    parser.add_argument("-s", "--stream_dir", dest="stream_dir", required=True)
    parser.add_argument("-o", "--output_dir", dest="output_dir", required=True)
    parser.add_argument("-r", "--rounds", dest="rounds", required=True)
    parser.add_argument("--tree-merge", dest="tree_merge", default="")
    parser.add_argument("--skip-sync", dest="skip_sync", action="store_true", default=False)
    parser.add_argument("--skip-check", dest="skip_check", action="store_true", default=False)
    parser.add_argument("--save-output", dest="save_output", action="store_true", default=False)
    parser.add_argument("--ckpt", dest="ckpt", action="store_true", default=False)
    parser.add_argument("--ckpt_test", dest="ckpt_test", action="store_true", default=False)
    parser.add_argument("--NOT-ckpt_test", dest="NOT_ckpt_test", action="store_true", default=False)
    parser.add_argument("--clear-ckpts", dest="clear_ckpts", action="store_true", default=False)


    cmd_args = parser.parse_args()
    host_file = os.path.abspath(cmd_args.host_file)
    hosts = get_hosts(host_file)
    bench = benchmark.Benchmark(cmd_args.test, cmd_args.stream_dir)

    sync = True
    if cmd_args.skip_sync:
        sync = False

    keep_output = False
    if cmd_args.save_output:
        keep_output = True

    try:
        os.mkdir(cmd_args.output_dir)
    except OSError:
        print cmd_args.output_dir, "already existed...moving on"


    print "preparing the servers..."
    bench.prep_tests(hosts)

    if cmd_args.tree_merge:
        files = get_output_files(cmd_args.tree_merge)
        ship_output_files(files, hosts, "tree_merge_outputs")
        for i in range(int(cmd_args.rounds)):
            if bench.should_run(cmd_args.output_dir, str(i)):
                print "running tests, round", str(i)
                bench.run_tree_merge(hosts, host_file,\
                                     "tree_merge_outputs",\
                                     "{}/{}".format(cmd_args.output_dir,str(i)))
                print "checking outputs...",
                bench.check_output(cmd_args.output_dir, str(i))
                print "done"

        return

    if cmd_args.ckpt_test:
        for i in range(int(cmd_args.rounds)):
            if bench.should_run(cmd_args.output_dir, str(i)):
                print "running ckpt tests, round", str(i)
                bench.ckpt_tests(hosts, host_file)
                print "checking outputs...",
                bench.check_output(cmd_args.output_dir, str(i))
                print "done"

        return

    if cmd_args.NOT_ckpt_test:
        for i in range(int(cmd_args.rounds)):
            if bench.should_run(cmd_args.output_dir, str(i)):
                print "running NOT ckpt tests, round", str(i)
                bench.NOT_ckpt_tests(hosts, host_file)
                print "checking outputs...",
                bench.check_output(cmd_args.output_dir, str(i))
                print "done"

        return        

    if cmd_args.ckpt:
        bench.ckpt_tests(hosts, host_file)

    for i in range(int(cmd_args.rounds)):
        if bench.should_run(cmd_args.output_dir, str(i)):
            print "running tests, round", str(i)
            bench.run_tests(hosts, host_file, sync)
            sync = False

            print "copying results to", cmd_args.output_dir
            bench.move_output(cmd_args.output_dir, str(i))

            if not cmd_args.skip_check:
                print "checking outputs...",
                bench.check_output(cmd_args.output_dir, str(i))
                print "done"

            if not keep_output:
                print "removing outputs...",
                bench.remove_output(cmd_args.output_dir, str(i))
                print "done"

            bench.cleanup_tests(hosts)


        else:
            print "skipping round", str(i)

    if cmd_args.clear_ckpts:
        bench.clear_ckpts(hosts)

main()
