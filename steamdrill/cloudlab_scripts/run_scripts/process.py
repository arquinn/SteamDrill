import subprocess
import os
import errno

def call(binary, args, directory):
    a = [binary]
    a.extend(args)
    subprocess.call(a, cwd=directory)

def echo(binary, args, directory):
    a = [binary]
    a.extend(args)
    print a, "in",directory


def mkdir(directory):
    try: 
        os.makedirs(directory)

    except OSError as error:
        if error.errno != errno.EEXIST or not os.path.isdir(directory):
            raise
            
            
