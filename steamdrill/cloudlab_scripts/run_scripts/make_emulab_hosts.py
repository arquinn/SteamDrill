#!/usr/bin/python
import sys

def main():
    """The main function to generate the hosts"""

    if len(sys.argv) < 2:
        print "need more hosts"
        return -1

    num_hosts = sys.argv[1]
    epochs_per_host = sys.argv[2]

    for i in range(int(num_hosts)):
        print "{} node-{}".format(epochs_per_host, i)

    return 0

main()
