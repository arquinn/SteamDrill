#!/usr/bin/python
"""Module to run a merge """

import argparse
import os
import benchmark
import server
import process

def get_hosts(host_filename):
    """Get the hosts from a host file"""
    hosts = []
    with open(host_filename, "r") as rfile:
        for line in rfile:
            words = line.split()
            serv = server.Server(words[1])
            hosts.append(serv)

    return hosts

def logKey(logname):

    if "." in logname:        
        val = logname.split(".")[-1]
        return int(val)
    return 0


def start_servers(hosts, stream_dir):
    for host in hosts:
        host.run_background_command("./streamserver", [], stream_dir)
        host.run_background_command("./pound_cpu", [], stream_dir)

def stop_servers(hosts):
    for host in hosts:
        host.kill_command("streamserver")
        host.kill_command("pound_cpu")


def get_output_files(d):
    files = []
    for path in sorted(os.listdir(d), logKey):
        if "output" in path:
            files.append(os.path.join(d, path))
    return files

def ship_output_files(files, hosts, host_file):

    with open(host_file, "w") as hfile: 
        for ofile, host in zip(files, hosts):
            shm_path = "/dev/shm/{}".format(os.path.basename(ofile))
            host.send_file(ofile, shm_path)
            hfile.write("{}\n".format(shm_path))


def main():
    """main function for running a test"""

    parser = argparse.ArgumentParser()
    parser.add_argument("--hosts", dest="host_file", required=True)
    parser.add_argument("--tree-merge", dest="tree_merge", required=True)

    parser.add_argument("--stream-dir", dest="stream_dir", required=True)
    parser.add_argument("--output-dir", dest="output_dir", required=True)

    args = parser.parse_args()
    hosts = get_hosts(args.host_file)
    files = get_output_files(args.tree_merge)
    ship_output_files(files, hosts, "tree_merge_outputs")


    start_servers(hosts, args.stream_dir)
    toutputs = os.path.abspath("tree_merge_outputs")

    test_args = [" ", " ", " ", " ", toutputs, os.path.abspath(args.host_file), os.path.abspath(args.output_dir), "-w", "-t"]
    print "calling streamctl"
    process.call("./streamctl", test_args, args.stream_dir)
    print "finished streamctl"
    stop_servers(hosts)


    

main()
