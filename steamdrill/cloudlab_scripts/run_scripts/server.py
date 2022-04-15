import experiment_utilities
import sys
import spur
import os
import shlex
import subprocess
 

class Server:
    # not sure why I need num_epochs
    def __init__(self, host):
        self._host = host

    def __eq__(self, other):
        return (self._host == other._host)

    def __ne__(self, other):
        return (self._host != other._host)

    def run_command(self, cmd, args, directory, ignore_problems=False):
        shell = experiment_utilities.open_ssh_session(self._host)
        remote_args = [cmd]
        for a in args:
            remote_args.append(a)

        print self._host, "running", ",".join(remote_args), "in", directory, "on"
        with shell:
            try:
                result = shell.run(remote_args, cwd=directory)
                print result.output
            except spur.results.RunProcessError as rpe:
                print cmd, "had problem"
                if not ignore_problems:
                    raise rpe


    def run_background_command(self, cmd, args, directory):
        shell = experiment_utilities.open_ssh_session(self._host)

        cmd_path="{}/{}".format(directory,"run_background_task.sh")
        self.send_file("run_background_task.sh", cmd_path)            

        remote_args = ["./run_background_task.sh",cmd]
        for a in args:
            remote_args.append(a)

        with shell:
            shell.run(["/bin/chmod","777","run_background_task.sh"],cwd=directory)
            shell.spawn(remote_args,cwd=directory)


    def kill_command(self, command):
        shell = experiment_utilities.open_ssh_session(self._host)
        with shell:
            shell.spawn(["pkill","-9",command])

    def send_file(self, local, remote):
        experiment_utilities.put_file(self._host, local, remote)


    def start_ctrl(self, user, password, flags):

        args = ["time","./streamctl","partitions.test","server.config","-w"]
        for item in flags:
            args.append(item)                

        shell = experiment_utilities.open_ssh_session(self.host, user, password)
        with shell:
            try:
                shell.run(["/bin/rm","-r",CLOUDLAB_STREAMCTL_DIR + "/tmp_results"], cwd="/")
            except spur.results.RunProcessError:
                print>>sys.stderr, "whoops, tmp_results didn't exist yet" 

            result = shell.run(args,cwd=CLOUDLAB_STREAMCTL_DIR)
            print>>sys.stderr, result.output


class Test_Configuration: 
    def __init__(self, pfilename, correct_dir, server_file, hosts):

        self.num_partitions = sum(1 for line in open(pfilename, "r")) -1 #off by one b/c line with directory
        self.pfilename = pfilename
        self.cdir = correct_dir
        self.sfile = server_file
        self.hosts = hosts
        self.ctrl_host = self.hosts[0]#choose the first host as ctrl


    def ship_replay_files(self,user,password): 
        self.ctrl_host.start_recv_files(user, password)

        replay_dir = open(self.pfilename, "r").readline().strip()
        args = ["./sync_files", replay_dir, self.ctrl_host.host]
                
        try:
            p = subprocess.check_output(args, cwd=STREAMCTL_DIR)
            print>>sys.stderr, p
        except subprocess.CalledProcessError as e:
            print>>sys.stderr, "streamctl returned non-zero rc"
            print>>sys.stderr, e.cmd
            print>>sys.stderr, e.returncode
            print>>sys.stderr, e.output


    def prep_ctrl(self, user, password):
        self.ctrl_host.prep_for_ctrl(user, password, self.pfilename, None, None)

    def prep_sync_ctrl(self, user, password):
        self.ctrl_host.prep_for_ctrl(user, password, self.pfilename, self.sfile, self.cdir)

    def start_ctrl(self, user, password, flags): 
        self.ctrl_host.start_ctrl(user, password, flags)

        print>>sys.stderr, "<finished with stream_ctl for "+str(self.num_partitions) + " host test>"

    def get_stats_files(self, user, password, out_files):         

        output_file_prefix = out_files + str(self.num_partitions)
        
        taint_files = ["tar","-zcf","/tmp/taint-stats.tgz"]
        stream_files = ["tar","-zcf","/tmp/stream-stats.tgz"]
        for i in range(self.num_partitions):
            taint_files.append("/tmp/taint-stats-" + str(i))
            stream_files.append("/tmp/stream-stats-" + str(i))

        shell = experiment_utilities.open_ssh_session(self.ctrl_host.host, user, password)
        with shell:
            shell.run(taint_files,cwd = "/")
            shell.run(stream_files,cwd = "/")    


        local_taint = output_file_prefix + ".taint-stats.tgz"
        local_stream = output_file_prefix + ".stream-stats.tgz"
        remote_taint = "/tmp/taint-stats.tgz"
        remote_stream = "/tmp/stream-stats.tgz"

        experiment_utilities.get_file(self.ctrl_host.host, user, password,local_taint, remote_taint)
        experiment_utilities.get_file(self.ctrl_host.host, user, password,local_stream, remote_stream)
