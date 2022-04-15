import paramiko
import os
import spur


def open_ssh_session(host, user, password):
    return spur.SshShell(hostname=host, username=user, password=password, missing_host_key=spur.ssh.MissingHostKey.accept)


def open_paramiko_ssh_session(host, user, password): 
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(host, username = user, password = password)
    return ssh

def put_file(host, user, password, local_file, remote_file):
    ssh = open_paramiko_ssh_session(host, user, password)
    sftp = ssh.open_sftp()
    sftp.put(local_file, remote_file)
    sftp.close()


def put_files(host, user, password, local_dir, remote_dir):

    print "remote_dir",remote_dir
    print "local_dir",local_dir
    ssh = open_paramiko_ssh_session(host, user, password)
    sftp = ssh.open_sftp()
    try:
        sftp.mkdir(remote_dir)
    except:
        print "seqtt.reults must have already existed" 
    
    for fname in os.listdir(local_dir): 
        sftp.put(local_dir + "/" + fname, remote_dir + "/" + fname)    

    sftp.close()



def get_file(host, user, password, local_file, remote_file):
    ssh = open_paramiko_ssh_session(host, user, password)
    sftp = ssh.open_sftp()
    sftp.get(remote_file, local_file)
    sftp.close()
