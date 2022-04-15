# Time-Transcendent Debugging the OmniTable Way

###### i.e., our prototype implementation of the OmniTable Query Model and the Steamdrill time-transcendent debugger

#### Overview

This repository contains the prototype implementation of the OmniTable
Query model and the SteamDrill time-transcendent debugger.  The OmniTable
query model enables time-transcendent debugging---developers can express
debugging code that is decoupled from the time sequence of the
original execution to better summarize, compare, and understand buggy
behavior in an execution.  We find that such power is especially
useful for diagnosing the root cause of our most pernicious bugs.

In particular, the OmniTable model is built on relational query logic.
It is built around a massive abstraction, called an OmniTable, that
encodes all of the user-level program state (i.e., register and memory
values) reached during each instruction in a program execution;
conceptually, an OmniTable is extracted online (i.e., during the
execution) and queried offline (i.e., after the execution).  The model
exposes a SQL query interface to enable succinct reasoning over huge
portions of a program's execution.  Critically, the combination of SQL
above an offline execution abstraction enables debugging logic that is
not inherently tied to the time sequence of the original program
execution; instead, developer scan write queries that simultaneously
observe, summarize, and compare events at many points in an execution.

However, the OmniTable abstraction itself is infeasible to
realize---its size is on the order of the number of instructions
executed times the size of the memory space of the execution.
Storing, or even calculating, such a massive abstraction constitutes
substantial overheads that are unreasonable for all but the shortest
executions.

To realize the model, SteamDrill, our prototype, proposes lazy
materialization.  Namely, rather than extract and store an OmniTable
directly during a program's execution, SteamDrill instead uses
deterministic record/replay to store a representation of the execution
(i.e., the log of nondeterministic events that defines the execution)
that the system could use to recreate OmniTable data offline.  When a
developer queries an OmniTable, SteamDrill creates dynamic
instrumentation that it injects into a replay execution to create the
data required for recomputing the OmniTable.

SteamDrill uses the time-transcendence of the OmniTable model as a
vector to drastically improve the speed of debugging queries.  The
system resolves debugging queries using many replay executions rather
than a single execution in two ways.  First, it employs a novel
multi-replay resolution strategy that decomposes a query into many
independent subqueries.  The strategy resolves each subquery in a new
replay execution, re-executing the same underlying execution logic many
times, in the order of their expected materialization costs.  The
approach allows SteamDrill to use the data obtained from
inexpensive-to-materialize subqueries to reduce the materialization
cost of expensive-to-materialize subqueries---often, we find that the
benefits of such information dwarfs the cost of a new replay
execution.

Second, Steamdrill uses time-transcendence as a mechanism for
automatically employing cluster-scale epoch parallelism (see
[JetStream](https://arquinn.github.io/assets/pdf/quinn16.pdf) and
[SledgeHammer](https://arquinn.github.io/assets/pdf/quinn18.pdf)).
The system splits each round of replay into many time-slices, called
epochs.  It instruments and executes each epoch on an independent core
in a large compute cluster to materialize the data for the current
subquery.

In the remainder of this README, we first describe how to download and
install SteamDrill and then discuss how to use the system.  We end
with an overview of the source-level layout of the project

## Download and Installing SteamDrill

SteamDrill is built on [Arnold](https://github.com/endplay/omniplay);
it is integrated into the original source of Arnold.  Below, we
describe the steps to download and install everything you need to get
started.

### Get the source

1) Download and install the 12.04.2 LTS Ubuntu Linux 32-bit
distribution.  You should install this distro before
proceeding---Arnold will not work if you try to run it on another
distribution.

2) Obtain the SteamDrill source code and all submodules:

```
$ git clone git@github.com:arquinn/SteamDrill.git
$ pushd steamdrill; git submodule update --recursive; popd
$ git clone git@github.com:arquinn/Spark-For-OmniTable.git
```

3) Install all of the dependencies for Arnold, SteamDrill, and Spark.

```
$ sudo apt-get install libboost-all-dev gawk texinfo autoconf gettext openjdk-8-jdk wget cmake llvm-dev
$ sudo mkdir -p /opt/gradle; sudo chmod 751 /opt/gradle;
$ pushd /opt/gradle;
$ wget https://services.gradle.org/distributions/gradle-7.4.2-bin.zip
$ unzip gradle-7.4.2-bin
$ rm gradle-7.4.2-bin.zip
```

You'll also want to add gradle to your path:

```
$ export PATH=$PATH:/opt/gradle/gradle-7.4.2/bin
```

Or, you might find it convenient to place this in your .bashrc:

```
$ echo "PATH=$PATH:/opt/gradle/gradle-7.4.2/bin" >> ~/.bashrc
```


### Installation

#### Arnold

1) Assuming that <SteamDrill> is the root of this source tree, run the
following.  It initializes some environment variables, including
$OMNIPLAY_DIR, which we use throughout the rest of this README.

```
$ cd <SteamDrill>/scripts
$ ./setup.sh
$ source $HOME/.omniplay_setup
```

2) Build the Omniplay kernel

```
$ cd $OMNIPLAY_DIR/linux-lts-quantal-3.5.0
$ make menuconfig
$ sudo make modules_install
$ sudo make install
$ sudo make headers_install INSTALL_HDR_PATH=$OMNIPLAY_DIR/test/replay_headers
$ sudo reboot
```

After rebooting, you should be running on Arnold; you can verify with
`uname -a`.

3) Build glibc

```
$ cd $OMNIPLAY_DIR/eglibc-2.15/
$ mkdir build
$ mkdir prefix
$ cd build
$ ../configure -prefix=$OMNIPLAY_DIR/eglibc-2.15/prefix --disable-profile --enable-add-on  --without-gd  --without-selinux  --without-cvs  --enable-kernel=3.2.0
$ make
$ mkdir /var/db (if not there already)
$ chown <user> /var/db
$ mkdir ../prefix/etc
$ touch ../prefix/etc/ld.so.conf
$ make install
$ cd $OMNIPLAY_DIR/eglibc-2.15/prefix
$ ln -s /usr/lib/locale
```

4) Build the tools for record and replay with Arnold

```
$ cd $OMNIPLAY_DIR/test/dev
$ make
$ cd ..
$ make
```

#### Spark

Go to where you downloaded spark-for-OmniTable and execute the following:

```
$ ./build/mvn -DskipTests -T<num cores> clean install
```

#### SteamDrill

1) Build and install required submodules:

```
$ pushd $OMNIPLAY_DIR/steamdrill/lib/
$ pushd capstone
$ mkdir build && pushd build
$ ../cmake.sh
$ popd
$ popd
$ pushd elfutils
$ autoconf -i -f && ./configure --prefix=`pwd`/install && make && make install
$ popd
$ popd
```

3) Build and install SteamDrill

```
$ pushd $OMNIPLAY_DIR/steamdrill
$ make -j <num_cores>
$ pushd spark_datasource;
$ gradle build
$ popd
$ popd
```


## Using SteamDrill and Arnold

You can begin recording and replaying executions now that you have
Arnold installed!  But first, there's one additional step that you
must perform on each reboot (it might be a good idea to but this in
your `.bashrc`):

```
$ $OMNIPLAY_DIR/scripts/insert_spec.sh
```

To record and replay an execution, you must first determine your
dynamic link path (hint: you can look in /etc/ld.so.conf.d/).  A
typical path looks like:
`/lib/i386-linux-gnu:/usr/lib/i386-linux-gnu:/usr/local/lib:/usr/lib:/lib`

#### Recording

Go to the tools directory, i.e., `$OMNIPLAY_DIR/test`. You can record
a program by specifying its fully-qualified pathname:

```
$ ./launcher --pthread <omniplay>/eglibc-2.15/prefix/lib:<libpath> <program_path> <args>
```

This will record the execution of that program, as well as any
children spawned by that program.  You will see new directories added
each recorded execution in the `/replay_logdb` directory.  Each entry
contains a log of non-deterministic execution for kernel operations,
glibc operations, and some checkpointing files.

Additionally, you will start to see new files added in the
`/replay_cache` directory.  Cache files are named by device and inode
number.  If a file changes over time, past versions are additionally
named by their respective modification times.

#### Replaying

Go to the tools directory, i.e. `$OMNIPLAY_DIR/test`, and execute the
following command, where </replay_logdir/rec_id> refers to the
directory entry for the recorded execution:

```
$ ./resume </replay_logdir/rec_id> -pthread <omniplay>/src/omniplay/eglibc-2.15/prefix/lib
```

A successful replay will print the message "Goodbye, cruel lamp! This
replay is over" in the kernel log (use dmesg to check).  An
unsuccessful replay may or may not print this message.  It will also
print out error messages in any event.

#### SteamDrill

To create a query over a specific execution, called <replay>, you
perform the following actions.  Note, we assume that you have access
to a cluster of machines, and we assume that the `/replay_logdb` folder
is stored in filesystem shared across the nodes (we used
[glusterfs](https://www.gluster.org/) in our experiments).

1) You must first prepare your cluster for your replay.  Note---these
steps only need to occur once per replay, repeated queries over the
same execution need not re-prepare the cluster.  The `prep_replay.sh` in
the `<steamdrill>/server` folder performs these steps for our particular
experimental cluster; below we outline them more generally:

1.a) First, you use our static disassembly tool on the executables
used by the execution. You must perform this step on each host in the cluster:

```
$ pushd <steamdrill>/server; ./block_cache_add.sh <replay>; popd
```

1.b) Then, you must create all of the checkpoints needed for your
replay.  Let <cores> be the number of cores in your cluster and execute:

```
$ pushd <steamdrill>/server
$ ckpts=`./check_partition <replay> <cores> | awk '$5 >0 {print $5} | uniq'
$ for ckpt in $ckpts; do
$ ./resume -pthread <omniplay>/src/omniplay/eglibc-2.15/prefix/lib --ckpt_at=$ckpt <replay>
$ done
```

2) Write your query.  The simplest approach is to create a query using
Spark's dataframe API as a new file, named <query.scala>, in the
`<steamdrill>/spark_datasource/src/main/scala/examples` directory.

3) Execute it! First, add the hostname of each of your hosts to
`<spark>/conf/slaves` (sorry for the name). Then, start the spark
cluster, assuming you installed `spark-for-omnitable` in the directory
<spark>, by calling:

```
$ <spark>/sbin/start-all.sh
```

After starting spark, you can execute the following command to submit
your query:

```
$ <steamdrill>/spark_datasource/submit.sh <query> <spark>
```
