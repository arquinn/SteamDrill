""" Represents a suites of tests to be run by our benchmarking script"""

import os
import shutil
import process


LOGGING_SO = "logging.so"
ANALYZER = "analyzer.so"
LIBC_CONFIG = "libc_config"
CONFIG = "config"

class Test:
    """ Represents a single test to be run, consists of:
        1. path to epoch file
        2. path to output directory """

    def __init__(self, epoch_path, output_dir):
        self._epoch_path = epoch_path
        self._output_dir = output_dir

    def get_epoch_path(self):
        """ returns the epoch_path for the test"""
        return self._epoch_path

    def get_output_dir(self):
        """ returns the output_dir for the test"""
        return self._output_dir

    def __str__(self):
        return "{} {}".format(self._epoch_path, self._output_dir)

class Benchmark:
    """ Represents a class of test to be run for a single application """
    def __init__(self, bm, stream_dir):
        self._stream_dir = stream_dir
        self._extra_args = []

        self._analyzer = ""
        self._global = ""
        self._local = ""
        self._stream = ""
        with open(bm, "r") as bmfile:
            for line in bmfile:
                [key, value] = line.split(":")
                value = value.strip()
                if key == "bm_dir":
                    bm_dir = os.path.expanduser(value)
                elif key == "tests_dir":
                    tests_dir = os.path.join(bm_dir, value)
                elif key == "shared_object":
                    self._shared_object = os.path.join(bm_dir, value)
                elif key == "libc_config":
                    self._libc_config = os.path.join(bm_dir, value)
                elif key == "config":
                    self._config = os.path.join(bm_dir, value)
                elif key == "replay_dir":
                    self._replay_dir = os.path.join(bm_dir, value)
                elif key == "correct_log":
                    self._correct_log = os.path.join(bm_dir, value)
                elif key == "analyzer":
                    self._analyzer = os.path.join(bm_dir, value)
                elif key == "global":
                    self._global = value
                elif key == "local":
                    self._local = value
                elif key == "stream":
                    self._stream = value
                elif key == "extra_args":
                    self._extra_args = value.split()
                else:
                    print "I didn't recogize", line

        #make sure output directory exists
        self._output_dir = os.path.join(bm_dir, "output")
        try:
            os.mkdir(self._output_dir)
        except OSError:
            print self._output_dir, "already existed"

        self._tests = []
        for test in os.listdir(tests_dir):
            output_dir = os.path.join(bm_dir, "output", test)
            test_dir = os.path.join(tests_dir, test)
            new_test = Test(test_dir, output_dir + "/")
            print "new test", new_test
            self._tests.append(new_test)


    def prep_global_host(self):
        """ prepares the cluster to run the test """
        #copy files into home directory
        home = os.path.expanduser("~")

        so_path = os.path.join(home, LOGGING_SO)
        libc_config_path = os.path.join(home, LIBC_CONFIG)
        config_path = os.path.join(home, CONFIG)
        analyzer_path = os.path.join(home, ANALYZER)

        shutil.copy2(self._shared_object, so_path)
        shutil.copy2(self._libc_config, libc_config_path)
        shutil.copy2(self._config, config_path)

        if self._analyzer != "":
            shutil.copy2(self._analyzer, analyzer_path)




    def prep_tests(self, hosts):
        """ prepares the cluster to run the test """
        #copy files into home directory
        home = os.path.expanduser("~")

        so_path = os.path.join(home, LOGGING_SO)
        libc_config_path = os.path.join(home, LIBC_CONFIG)
        config_path = os.path.join(home, CONFIG)
        analyzer_path = os.path.join(home, ANALYZER)

        shutil.copy2(self._shared_object, so_path)
        shutil.copy2(self._libc_config, libc_config_path)
        shutil.copy2(self._config, config_path)

        if self._analyzer != "":
            shutil.copy2(self._analyzer, analyzer_path)


        process.call("sudo",\
                     ["/bin/bash", "set_timing.sh"],\
                     os.getcwd())


        #assume that we're the first host
        for host in hosts[1:]:
            host.run_command("sudo",\
                          ["/bin/chmod", "777", "/replay_cache", "/replay_cache"],\
                          "/")
            host.run_command("sudo",\
                             ["/bin/bash", "set_timing.sh"],\
                             os.getcwd())

            host.send_file(so_path, so_path)
            host.send_file(libc_config_path, libc_config_path)
            host.send_file(config_path, config_path)
            if self._analyzer != "":
                host.send_file(analyzer_path, analyzer_path)


    def sync_replay(self, host_file_path):
        home = os.path.expanduser("~")
        so_path = os.path.join(home, LOGGING_SO)
        libc_path = os.path.join(home, LIBC_CONFIG)
        config_path = os.path.join(home, CONFIG)
        args = [self._replay_dir, so_path, libc_path, config_path] 
        #get largest test
        most_epochs = 0
        test = ""
        
        for c_test in self._tests:
            curr_epoch = sum(1 for l in open(c_test.get_epoch_path()))
            if curr_epoch > most_epochs:
                test = c_test
                most_epochs = curr_epoch
                
        test_args = list(args)
        test_args.append(test.get_epoch_path())
        test_args.append(host_file_path)
        test_args.append(test.get_output_dir())
        test_args.append("-w")
        test_args.append("-s")
        
        if len(self._extra_args) > 0:
            test_args.extend(self._extra_args)
        
        process.mkdir(test.get_output_dir())
        print "syncing files with", test
        process.call("./streamctl", test_args, self._stream_dir)
            
    def ckpt_test(self, host_file_path, test):
        home = os.path.expanduser("~")
        so_path = os.path.join(home, LOGGING_SO)
        libc_path = os.path.join(home, LIBC_CONFIG)
        config_path = os.path.join(home, CONFIG)
        args = [self._replay_dir, so_path, libc_path, config_path] 
        
        args.append(test.get_epoch_path())
        args.append(host_file_path)
        args.append(test.get_output_dir())
        args.append("-w")
        args.append("-c")
        args.append("-s")

        if len(self._extra_args) > 0:
            args.extend(self._extra_args)
        
        process.mkdir(test.get_output_dir())
        print "syncing files with", test
        process.call("./streamctl", args, self._stream_dir)


    def NOT_ckpt_tests(self, hosts, host_file):
        home = os.path.expanduser("~")
        so_path = os.path.join(home, LOGGING_SO)
        libc_path = os.path.join(home, LIBC_CONFIG)
        config_path = os.path.join(home, CONFIG)
        args = [self._replay_dir, so_path, libc_path, config_path] 
        
        args.append(test.get_epoch_path())
        args.append(host_file_path)
        args.append(test.get_output_dir())
        args.append("-w")
        args.append("--NOT-ckpt")
        args.append("-s")

        if len(self._extra_args) > 0:
            args.extend(self._extra_args)
        
        process.mkdir(test.get_output_dir())
        print "syncing files with", test
        process.call("./streamctl", args, self._stream_dir)



    def start_servers(self, hosts):
        for host in hosts:
            host.run_background_command("./streamserver", [], self._stream_dir)
            host.run_background_command("./pound_cpu", [], self._stream_dir)
            host.run_background_command("./read_meminfo.sh", [], self._stream_dir)

    
    def setup_checkpoint_dirs(self, hosts):
        for host in hosts:
            host.run_command("/bin/bash", ["setup_checkpoint_dir.sh", self._replay_dir], os.getcwd(), True)

    def link_checkpoints(self, hosts):
        for host in hosts:
            host.run_command("/bin/bash", ["link_checkpoints.sh", self._replay_dir], os.getcwd(), True)

    def stop_servers(self, hosts):
        for host in hosts:
            host.kill_command("streamserver")
            host.kill_command("pound_cpu")
            host.kill_command("read_meminfo")


    def run_ckpt_tests(self, hosts, host_file_path, rounds, ckpt=False, sync=True):
        self.start_servers(hosts)

        if sync:
            self.sync_replay(host_file_path)
            
        for test in self._tests:
            if ckpt:
                self.ckpt_test(host_file_path, test)
                for r in range(rounds):

                    if test.should_run():
                        test.run()




        self.stop_servers(hosts)


    def cleanup_tests(self, hosts):
        """ cleans up the cluster after a test """
        for host in hosts:
            host.run_command("/bin/bash", ["reset_host.sh"], os.getcwd(), True)

    def clear_ckpts(self, hosts):
        """ cleans up checkpoints after a full test """
        for host in hosts:
            host.run_command("/bin/bash", ["clear_checkpoints.sh", self._replay_dir], os.getcwd(), True)
    

    

    def ckpt_tests(self, hosts, host_file_path):

        self.start_servers(hosts)
        self.setup_checkpoint_dirs(hosts)

        home = os.path.expanduser("~")
        so_path = os.path.join(home, LOGGING_SO)
        libc_path = os.path.join(home, LIBC_CONFIG)
        config_path = os.path.join(home, CONFIG)
        args = [self._replay_dir, so_path, libc_path, config_path] 
        
        for test in self._tests:
            process.mkdir(test.get_output_dir())
            test_args = list(args)
            test_args.append(test.get_epoch_path())
            test_args.append(host_file_path)
            test_args.append(test.get_output_dir())
            test_args.append("-w")
            test_args.append("-c")
            test_args.append("-s")

            if len(self._extra_args) > 0:
                test_args.extend(self._extra_args)

            process.echo("./streamctl", test_args, self._stream_dir)
            process.call("./streamctl", test_args, self._stream_dir)
            print "<finished with ckpt", test, ">"

        self.stop_servers(hosts)
        self.link_checkpoints(hosts)

    def run_tests(self, hosts, host_file_path, sync=True):
        self.start_servers(hosts)

        """ runs all of the tests, also syncs files using the largest one """
        home = os.path.expanduser("~")
        so_path = os.path.join(home, LOGGING_SO)
        libc_path = os.path.join(home, LIBC_CONFIG)
        config_path = os.path.join(home, CONFIG)
        analyzer_path = os.path.join(home, ANALYZER)

        args = [self._replay_dir, so_path, libc_path, config_path]

        if sync:
            #get largest test
            most_epochs = 0
            test = ""

            for c_test in self._tests:
                curr_epoch = sum(1 for l in open(c_test.get_epoch_path()))
                if curr_epoch > most_epochs:
                    test = c_test
                    most_epochs = curr_epoch

            test_args = list(args)
            test_args.append(test.get_epoch_path())
            test_args.append(host_file_path)
            test_args.append(test.get_output_dir())
            test_args.append("-w")
            test_args.append("-s")

            if len(self._extra_args) > 0:
                test_args.extend(self._extra_args)
                process.mkdir(test.get_output_dir())
                print "syncing files with", test
                process.call("./streamctl", test_args, self._stream_dir)

        for test in self._tests:
            process.mkdir(test.get_output_dir())
            test_args = list(args)
            test_args.append(test.get_epoch_path())
            test_args.append(host_file_path)
            test_args.append(test.get_output_dir())
            test_args.append("-w")


            if self._analyzer != "":
                test_args.append("--analyzer={}".format(analyzer_path))
                added = False
                if self._global != "":
                    added = True
                    test_args.append("--global={}".format(self._global))

                if self._local != "":
                    added = True
                    test_args.append("--local={}".format(self._local))

                if self._stream != "":
                    added = True
                    test_args.append("--stream={}".format(self._stream))

                if not added:
                    print "uhh, you forgot to include global/local spec"
                    assert False

            if len(self._extra_args) > 0:
                test_args.extend(self._extra_args)

            process.echo("./streamctl", test_args, self._stream_dir)
            process.call("./streamctl", test_args, self._stream_dir)
            print "<finished with", test, ">"

        self.stop_servers(hosts)

    def run_tree_merge(self, hosts, host_file_path, merge_file, output_dir):
        self.start_servers(hosts)

        home = os.path.expanduser("~")
        so_path = os.path.join(home, LOGGING_SO)
        libc_path = os.path.join(home, LIBC_CONFIG)
        config_path = os.path.join(home, CONFIG)
        analyzer_path = os.path.join(home, ANALYZER)

        args = [self._replay_dir, so_path, libc_path, config_path]

        process.mkdir(os.path.abspath(output_dir))
        test_args = list(args)
        test_args.append(os.path.abspath(merge_file))
        test_args.append(os.path.abspath(host_file_path))
        test_args.append(os.path.abspath(output_dir))
        test_args.append("-w")
        test_args.append("-t")


        if self._analyzer != "":
            test_args.append("--analyzer={}".format(analyzer_path))
            added = False
            if self._global == "":
                print "uhh.. .this really only does global buddy"
                
            test_args.append("--global={}".format(self._global))

        process.echo("./streamctl", test_args, self._stream_dir)
        process.call("./streamctl", test_args, self._stream_dir)
        print "<finished with round>"
        self.stop_servers(hosts)




    def should_run(self, dest_dir, sub_dir):
        #move everything from the tmp_dir to the destination dir
        output_path = os.path.join(dest_dir, sub_dir)

        try:
            os.mkdir(output_path)
            return True
        except OSError:
            return False

    def move_output(self, dest_dir, sub_dir):
        output_path = os.path.join(dest_dir, sub_dir)

        for i in os.listdir(self._output_dir):
            data_path = os.path.join(self._output_dir, i)
            shutil.move(data_path, output_path)

    def check_output(self, dest_dir, sub_dir):
        output_path = os.path.join(dest_dir, sub_dir)
        process.call("./validate_output.sh", [output_path, self._correct_log], os.getcwd())



    def remove_output(self, dest_dir, sub_dir):
        output_path = os.path.join(dest_dir, sub_dir)
        process.call("./remove_output.sh", [output_path], os.getcwd())

    def __str__(self):
        r = "{} {} {}".format(self._replay_dir, self._shared_object, self._test_config)
        for t in self._tests:
            r = "{}\n{}".format(r, t)
        return r
