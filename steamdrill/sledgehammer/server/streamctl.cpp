#define __USE_LARGEFILE64

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <dirent.h>

#include <atomic>
#include <vector>


#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>


using namespace std;

#include "streamserver.h"
#include "parseklib.h"
#include "config.hpp"
#include "../../lib/args/args.hxx"

#define MAX_HOSTS 64

// For debugging (obviously)
#define DEBUG(x) if (verbose) std::cout << x
bool verbose = false;

// Globals
vector<struct replay_path> logFiles;
vector<struct cache_info> cacheFiles;
char outputFilename[128];

// Stats
int inputBytes = 0, outputBytes = 0;;
struct timeval tvStart, tvGAStart, tvFlattenEnd, tvEnd;

bool test_and_set(vector<struct cache_info> &cfiles, struct cache_info &cinfo)
{
    bool found = false;
    for(struct cache_info c : cfiles) {
      if (c.dev == cinfo.dev && c.ino == cinfo.ino &&
	    c.mtime.tv_sec == cinfo.mtime.tv_sec &&
	    c.mtime.tv_nsec == cinfo.mtime.tv_nsec)
	    found = true;
    }
    if (!found) cfiles.push_back(cinfo);
    return found;
}

int getNeededFiles(const char *dirname, vector<struct replay_path> &log_files, vector<struct cache_info> &cache_files)
{
    // First build up a list of files that are needed for this replay
    struct dirent* de;
    DIR* dir = opendir(dirname);
    if (dir == NULL) {
	fprintf (stderr, "Cannot open replay dir %s\n", dirname);
	return -1;
    }

    while ((de = readdir(dir)) != NULL) {
	if ((!strncmp(de->d_name, "ckpt",4) && strncmp(de->d_name, "ckpt.",5)) || 
	    !strcmp(de->d_name, "mlog") || !strncmp(de->d_name, "ulog", 4) || 
	    !strncmp(de->d_name, "time_log", 8)|| !strncmp(de->d_name, "utimelog", 8)) {

	    struct replay_path pathname;
	    if (snprintf (pathname.path, 256, "%s/%s", dirname, de->d_name) < 0) {
              fprintf(stderr, "snprintf failed! %d", errno);
              return -1;
            }
	    log_files.push_back(pathname);
	} else if (!strncmp(de->d_name, "klog", 4)) {
	    struct klogfile *log;
	    struct klog_result *res;
	    struct replay_path pathname;
	    struct cache_info cinfo;

	    if (snprintf (pathname.path, 256, "%s/%s", dirname, de->d_name) < 0) {
              fprintf(stderr, "snprintf failed! %d", errno);
              return -1;
            }
	    log_files.push_back(pathname);
	    // Parse to look for more cache files
	    log = parseklog_open(pathname.path);
	    if (!log) {
		fprintf(stderr, "%s doesn't appear to be a valid klog file!\n", pathname.path);
		return -1;
	    }
	    while ((res = parseklog_get_next_psr(log)) != NULL) {
		if (res->psr.sysnum == 5) {
		    struct open_retvals* pretvals = (struct open_retvals *) res->retparams;
		    if (pretvals) {
			cinfo.dev = pretvals->dev;
			cinfo.ino = pretvals->ino;
			cinfo.mtime = pretvals->mtime;
			test_and_set(cache_files, cinfo);
#if 0
			printf ("open cache file dev %x ino %x mtime %lx.%lx log %s\n", 
				cinfo.dev, cinfo.ino, cinfo.mtime.tv_sec, cinfo.mtime.tv_nsec, de->d_name);
#endif
		    }
		} else if (res->psr.sysnum == 11) {
		    struct execve_retvals* pretvals = (struct execve_retvals *) res->retparams;
		    if (pretvals && !pretvals->is_new_group) {
			cinfo.dev = pretvals->data.same_group.dev;
			cinfo.ino = pretvals->data.same_group.ino;
			cinfo.mtime = pretvals->data.same_group.mtime;
			test_and_set(cache_files, cinfo);
#if 0
			printf ("exec cache file dev %x ino %x mtime %lx.%lx log %s\n", 
				cinfo.dev, cinfo.ino, cinfo.mtime.tv_sec, cinfo.mtime.tv_nsec, de->d_name);
#endif
		    }
		} else if (res->psr.sysnum == 86 || res->psr.sysnum == 192) {
		    struct mmap_pgoff_retvals* pretvals = (struct mmap_pgoff_retvals *) res->retparams;
		    if (pretvals) {
			cinfo.dev = pretvals->dev;
			cinfo.ino = pretvals->ino;
			cinfo.mtime = pretvals->mtime;
			test_and_set(cache_files, cinfo);
#if 0
			printf ("mmap cache file dev %x ino %x mtime %lx.%lx log %s\n", 
				cinfo.dev, cinfo.ino, cinfo.mtime.tv_sec, cinfo.mtime.tv_nsec, de->d_name);
#endif
		    }
		}
	    }
	    parseklog_close(log);
	} 
    }
    closedir(dir);
    return 0;
}
int parseEpochFile(Config::Host *hosts,
		   int hostCnt,
		   const char *epochFile)
{
    int rc, epochCount = 0;
    int currHost = 0;
    
    FILE *file = fopen(epochFile, "r");

    if (file == NULL) {
	fprintf (stderr, "Unable to open epoch description file %s, errno=%d\n", epochFile, errno);
	return -1;
    }


    while (!feof(file)) {
	char line[256];
	if (fgets (line, 255, file)) {
	    Config::Epoch e;
	    epochCount += 1;
	    char unusedFirst, unusedSecond;
	    rc = sscanf (line,
			 "%d %c %u %c %u %u\n", 
			 &e.startPid, 
			 &unusedFirst,
			 &e.startClock,
			 &unusedSecond, 
			 &e.stopClock, 
			 &e.ckptClock);

	    if (rc != 6) {
		fprintf (stderr, "Unable to parse line of epoch descrtion file: %s\n", line);
		return -1;
	    }

	    if (currHost >= hostCnt)
	    {
		std::cerr << "problem reading epochFile, we're out of hosts?" << std::endl;
		return -1;		    
	    }
	    hosts[currHost].addEpoch(e);
	    
	    if (!hosts[currHost].epochSlotsLeft())
	    {
		++currHost;
	    }
	}
    }
    fclose(file);

    return epochCount;
}


int parseTreeFile(Config::Host *hosts,
		  int hostCnt,
		  const char *treeFile)
{
    int rc, outputCount = 0;
    int currHost = 0;
    
    FILE *file = fopen(treeFile, "r");

    if (file == NULL) {
	fprintf (stderr, "Unable to open epoch description file %s, errno=%d\n", treeFile, errno);
	return -1;
    }


    while (!feof(file)) {
	char line[256];
	if (fgets (line, 255, file)) {
	    char outputName[128];
	    outputCount += 1;
	    char unusedFirst, unusedSecond;
	    rc = sscanf (line, "%s\n", outputName);

	    if (rc != 1) {
		fprintf (stderr, "Unable to parse line of tree descrtion file: %s\n", line);
		return -1;
	    }
	    hosts[currHost].setTreeOutput(outputName);
	    currHost++;
	}
    }
    fclose(file);

    return outputCount;
}



int parseEpochFileRounds(Config::Host *hosts,
			 int &numRounds,
			 int hostCnt,
			 const char *epochFile)
{
    int rc, epochCount = 0;
    int currHost = 0, roundCount = 0, currRound = 0;
    std::vector<Config::Epoch> epochs;
    FILE *file = fopen(epochFile, "r");
    if (file == NULL) {
	fprintf (stderr, "Unable to open epoch description file %s, errno=%d\n", epochFile, errno);
	return -1;
    }

    while (!feof(file)) {
	char line[256];
	if (fgets (line, 255, file)) {
	    Config::Epoch e;
	    epochCount += 1;
	    char unusedFirst, unusedSecond;
	    rc = sscanf (line,
			 "%d %c %u %c %u %u\n", 
			 &e.startPid, 
			 &unusedFirst,
			 &e.startClock,
			 &unusedSecond, 
			 &e.stopClock, 
			 &e.ckptClock);

	    if (rc != 6) {
		fprintf (stderr, "Unable to parse line of epoch descrtion file: %s\n", line);
		return -1;
	    }
	    epochs.push_back(e);
	}	    
    }
    fclose(file);

    for (int i = 0; i < hostCnt; ++i)
    {
	roundCount += hosts[i].capacity();
    }

    numRounds = epochs.size() / roundCount; 
    std::cerr << numRounds << " rounds for " << epochs.size() << " in " << roundCount << std::endl;

    for (int outer = 0; outer < numRounds; ++outer)
    {	
	for (int i = 0; i < hostCnt; ++i)
	{
	    hosts[outer + i] = hosts[i];
	}
    }


    for (auto &e : epochs)
    {
	hosts[currRound + currHost].addEpoch(e);
	if (!hosts[currRound + currHost].epochSlotsLeft())
	{
	    if (currHost < (hostCnt - 1))
	    {
		currHost++;
	    }
	    else 
	    {
		currRound ++;
		currHost = 0;
	    }
	}
    }

    return epochCount;
}



int parseHostFile(Config::Host *hosts,
		  const char *filename,
		  const char *replayName,
		  const char *soName,
		  const char *analyzerName,
		  const char *lAnalyzer,
		  const char *sAnalyzer,
		  const char *libcName,
		  const char *configName,
		  const char *outputDir,
		  const int logFd,
		  const u_long stackHint,
		  const u_long exitClock,
		  const bool wait,
		  const bool sync,
		  const bool builtInLogger,
		  const bool fork,
		  const bool memProtector,
		  const bool altrace,
		  const bool ckpt,
		  const bool tree,
		  const bool ckptNOT)
{
    uint32_t flags = Config::ServerConfig::buildFlag(ckpt,
						     tree,
						     wait,
						     sync,
						     builtInLogger,
						     fork,
						     memProtector,
						     altrace,
						     ckptNOT);
    int rc;
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
	fprintf (stderr, "Unable to open host configuration file %s, errno=%d\n", filename, errno);
	return -1;
    }
    
    // Now parse the configuration file
    u_int host_cnt = 0;
    
    while (!feof(file)) {

	char line[256];
	if (fgets (line, 255, file)) {
	    int num_epochs;
	    char name[FILENAME_LEN];

	    rc = sscanf (line, "%d %s\n", &num_epochs, name);
	    if (rc != 2) {
		fprintf (stderr, "Unable to parse line of server config file: %s\n", line);
		return -1;
	    }
	    Config::Host host(name,
			      outputDir,
			      num_epochs,
			      STREAMSERVER_PORT,
			      replayName,
			      soName,
			      analyzerName,
			      lAnalyzer,
			      sAnalyzer,
			      libcName,
			      configName,
			      logFd,
                              'i',
			      stackHint,
			      exitClock,
			      flags);

	    hosts[host_cnt++] = host;
	    hosts[host_cnt-1].addNextHost(name);


	    //hosts.push_back(host);
	    //host.dump();
	}
    }
    fclose(file);
    return host_cnt;
}

static int mapFile(const char *filename, char **output, size_t *len)
{
    struct stat stats;
    int fd = open(filename, O_RDONLY);   

    fstat(fd, &stats);
    *len = stats.st_size;
    if (stats.st_size > 0)
    {
	*output = (char *)mmap(NULL, *len, PROT_READ, MAP_PRIVATE, fd, 0);

	if (*output == MAP_FAILED)
	{
	    std::cerr << "couldn't map " << filename << " " << errno << std::endl;
	    return -1;
	}
    }

    close(fd);
    return 0;
}

static int unmapFile(char *output, size_t len)
{
    munmap(output, len);
    return 0;
}

#define FLATTEN_SIZE 4194304
static int flatten(int offset, int count, FILE *outFd)
{
    int i, rc;
    size_t bytesWritten, cBytesWritten, totalRead = 0;;
    char *buffer;
    size_t len;

    for (i = offset; i < count; ++i)
    {
	char filename[128];
	sprintf(filename, STREAMCTL_TMP_PREFIX, i);
	
	rc = mapFile(filename, &buffer, &len);
	assert (!rc);
	
	totalRead += len;
	DEBUG("flatten read " << len << std::endl);
 
	bytesWritten = 0;	    
	while (bytesWritten < len)
	{
	    cBytesWritten = fwrite(buffer + bytesWritten, 1, len - bytesWritten, outFd);
	    DEBUG("flatten wrote " << cBytesWritten << std::endl);
	    
	    assert (cBytesWritten >= 0);
	    bytesWritten += cBytesWritten;
	}

	unmapFile(buffer, len);
    }
    return totalRead;
}

static int
global_analyzer(const char *gAnalyzer, const char *analyzer,  const char *output_dir, 
		int offset, int count, struct timeval *flattenEnd)
{
    char tempFilename[128];
    int outFd, rc = 0;
    FILE *flattenFILE;

    sprintf(outputFilename, "%s/output", output_dir);
    outFd = open(outputFilename, O_WRONLY | O_CREAT | O_TRUNC, 0777);   
    assert (outFd > 0);
    if (strlen(gAnalyzer) > 0)
    {
	sprintf(tempFilename, "%s/streamctl.output", STREAMCTL_TMP);
	flattenFILE = fopen(tempFilename, "w+");
	assert (flattenFILE != NULL);
    }
    else 
    {
	flattenFILE = fdopen(outFd, "w");
	assert (flattenFILE != NULL);
    }
    
    inputBytes = flatten(offset, count, flattenFILE);
    fclose(flattenFILE);
    gettimeofday(flattenEnd, NULL);   

    if (strlen(gAnalyzer) > 0)
    {
	int flattenFd;
	char *dlhandle;   
	int (*analyzerFoo)(int, int, int*, int*);

	flattenFd = open(tempFilename, O_RDONLY);
	assert(flattenFd > 0);

	dlhandle = (char *) dlopen(analyzer, RTLD_NOW);    
	if (dlhandle == NULL)
	{
	    std::cerr << "dlopen of " << analyzer << " failed" << std::endl;
	    std::cerr << dlerror();
	    return -1; 
	}
	dlerror();
	analyzerFoo = (int (*)(int, int, int*, int*)) dlsym(dlhandle, gAnalyzer); 
	if (analyzerFoo == NULL)
	{
	    std::cerr << "dlsym of " << gAnalyzer << " failed" << std::endl;
	    std::cerr << dlerror();
	}

	std::cerr << std::hex << "analyzerFoo " << (u_long)analyzerFoo << std::endl;
	rc = (*analyzerFoo)(flattenFd, outFd, &inputBytes, &outputBytes);    
	close (flattenFd);
    }
    else 
    {
	outputBytes = inputBytes;
    }

    close (outFd);
    return 0;
}

static void*
process_host (void* threadData) 
{
    Config::ServerThread *self = (Config::ServerThread *)threadData;
    Config::Host *h = self->h; 
    int rc; 
    
    gettimeofday (&h->tvStart, NULL);
    rc = h->connectToHost();
    if (rc < 0)
    {
	std::cerr << "couldn't connect to " << *h << std::endl;
	return (void*)rc;
    }

    rc = h->sendServerConfig();	
    if (rc < 0)
    {
	std::cerr << "couldn't send server config to " << *h << std::endl;
	return (void*)rc;
    }
    
    rc = h->sendReplayFiles(logFiles, cacheFiles);
    if (rc < 0) 
    {
	std::cerr << "couldn't send replay files to " << *h << std::endl;
	return (void*)rc;
    }
    gettimeofday (&h->tvSyncStop, NULL);
    if (wait) {
	int rc = h->waitForResponse();
	if (rc < 0) 
	{
	    std::cerr << "couldn't wait for response " << std::endl;
	    return (void *)-3;
	}
	gettimeofday (&h->tvWaitStop, NULL);
    }

    rc = h->getOutput(self->offset);
    if (rc < 0) 
    {
	std::cerr << "couldn't get output" << std::endl;
	return (void *)-3;
    }

    gettimeofday (&h->tvEnd, NULL);    
    return NULL;
}


int getStats(Config::ServerThread *self)
{
    int rc;
    Config::Host *h = self->h;

    rc = h->getStats(self->offset);
    if (rc < 0) 
    {
	std::cerr << "couldn't get stats" << std::endl;
	return -3;
    }

    rc = h->getAltrace(self->offset);
    if (rc < 0) 
    {
	std::cerr << "couldn't get stats" << std::endl;
	return -3;
    }
    return 0;
}





int main (int argc, char* argv[])
{
    int rc, totalEpochs, remapFd = -1, stackHint = 0;
    u_long exitClock;
    char dirname[80];
    char globalOutput[256], serverFormat[256];
    struct stat outputStat;

    Config::Host hosts[MAX_HOSTS];
    int hostCnt;

    args::ArgumentParser parser("Manages a Ptrace parallel session with streamservers.", "A.R.Q.");        
    args::Group group(parser, "Required arguments:",args::Group::Validators::All);
    args::Flag wait(parser, "wait", "Enables waiting for all streamservers to finish.", {'w', "wait"});
    args::Flag sync(parser, "sync", "Enables syncing replay to all streamservers.", {'s', "sync"});
    args::Flag verbose(parser, "verbose", "Enables verbose outputa at all streamservers.", {'v', "verbose"});
    args::Flag builtInLogger(parser, "builtInLogger", "Tells the tool to use the Tracerlogger.", {'b',"builtIn"});
    args::Flag fork(parser, "fork", "Run with fork isolation.", {'f',"fork"});
    args::Flag memProtector(parser, "memProtector", "Run using mem based tracer calls.", {'m',"memProtector"});
    args::Flag altrace(parser, "altrace", "Trace the accesses that run functions.", {'a',"altrace"});
    args::Flag ckpt(parser, "ckpt", "Tell the servers to take ckpts instead of trace.", {'c',"checkpoint"});
    args::Flag ckptNOT(parser, "ckptNOT", "Tell the servers to take ckpts NOT instead of trace.", {'n',"ckptNOT"});
    args::Flag tree(parser, "tree", "Tell the servers to do tree merge.", {'t',"tree"});

    args::ValueFlag<u_long> exit_clock(parser, "Clock to exit on (if  early)",
				       "Clock value", {'e',"exit_clock"});

    args::ValueFlag<uint32_t> remap(parser, "File descriptor to remap for output", "An fd integer", {'r',"remap"});

    args::ValueFlag<std::string> stack(parser, "Stack hint (loc to place stackt", "A hex number", {"stack"},"0");
    args::ValueFlag<std::string> analyzer(parser, "analyer", "Path to analyzer so ", { "analyzer"},"");
    args::ValueFlag<std::string> sAnalyzer(parser, "stream analyer", "stream analyzer function", { "stream"},"");
    args::ValueFlag<std::string> lAnalyzer(parser, "local analyer", "local analyzer function", { "local"},"");
    args::ValueFlag<std::string> gAnalyzer(parser, "global analyer", "global analyzer function", { "global"},"");


    args::Positional<std::string> replayName(group, "replay dir", "The replay director");
    args::Positional<std::string> soName(group, "shared object path", "The path to the shared object");
    args::Positional<std::string> libcName(group, "libc file path", "The path to the libc configuration");
    args::Positional<std::string> configName(group, "config file path", "The path to the ptrace configuration");
    args::Positional<std::string> epochFile(group, "Epoch file path", "The path to the epoch file");
    args::Positional<std::string> hostFile(group, "Host file path", "The path to the host file");
    args::Positional<std::string> outputDir(group, "Output directory", "Directory to output stats and log files");


    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
    try
    {
	parser.ParseCLI(argc, argv);
    }
    catch (args::Help)
    {
	std::cout << parser; 
	return 0;
    }
    catch (args::ParseError e)
    {
	std::cerr << e.what() << std::endl;
	std::cerr << parser; 
	return -1;
    }
    catch (args::ValidationError e)
    {
	std::cerr << e.what() << std::endl;
	std::cerr << parser; 
	return -2;
    }

    if (args::get(remap))
	remapFd = args::get(remap);

    if (args::get(exit_clock))
	exitClock = args::get(exit_clock);


    stackHint = strtoul(args::get(stack).c_str(), NULL, 0);


    if (verbose)
    {
	std::cout << "arguments: " << std::endl
		  << "hostfile: " << args::get(hostFile).c_str() << std::endl
		  << "replayName: " << args::get(replayName).c_str() << std::endl
		  << "soName: " << args::get(soName).c_str() << std::endl
		  << "analyzer: " << args::get(analyzer).c_str() << std::endl
		  << "lanalyzer: " << args::get(lAnalyzer).c_str() << std::endl
		  << "sanalyzer: " << args::get(sAnalyzer).c_str() << std::endl
		  << "ganalyzer: " << args::get(gAnalyzer).c_str() << std::endl
		  << "libcName: " << args::get(libcName).c_str() << std::endl
		  << "configName: " << args::get(configName).c_str() << std::endl
		  << "epochFile: " << args::get(epochFile).c_str() << std::endl
		  << "hostFile: " << args::get(hostFile).c_str() << std::endl
		  << "outputDir: " << args::get(outputDir).c_str() << std::endl
		  << "fork: " << args::get(fork) << std::endl
		  << "remapfd: " << remapFd << std::endl 
		  << std::hex << "stackHint: " << stackHint << std::endl
		  << std::dec << "exitClock: " << exitClock << std::endl
		  << std::dec <<  "memProtector: " << args::get(memProtector) << std::endl
		  << std::dec <<  "altrace: " << args::get(altrace) << std::endl;   	
    }


    // Read in the host configuration file
    hostCnt= parseHostFile(hosts,
			   args::get(hostFile).c_str(),
			   args::get(replayName).c_str(),
			   args::get(soName).c_str(),
			   args::get(analyzer).c_str(),
			   args::get(lAnalyzer).c_str(),
			   args::get(sAnalyzer).c_str(),
			   args::get(libcName).c_str(),
			   args::get(configName).c_str(),
			   args::get(outputDir).c_str(),
			   remapFd,
			   stackHint,
			   exitClock,
			   args::get(wait),
			   args::get(sync),
			   args::get(builtInLogger),
			   args::get(fork),
			   args::get(memProtector),
			   args::get(altrace),
			   args::get(ckpt),
			   args::get(tree),
			   args::get(ckptNOT));

    
    if (hostCnt < 0)
    {
	std::cerr << "couldn't read in host file " << std::endl;
	return -1;
    }

    if (sync) 
    {
	rc = getNeededFiles(args::get(replayName).c_str(), logFiles, cacheFiles); 
	if (rc != 0)
	{
	    std::cerr << "couldn't get needed files " << errno <<std::endl;
	    return -3;
	}
    }

    std::cerr << "output: " << args::get(outputDir).c_str() << std::endl;
    rc = mkdir (args::get(outputDir).c_str(), 0755);
    if (rc < 0 && errno != EEXIST) 
    { 
	std::cerr << "couldn't make " << outputDir << " rc " 
		  << rc << ",errno " << errno << std::endl;
    }
    
    if (args::get(tree))
    {
	// Assign Epochs to Hosts: 
	totalEpochs = parseTreeFile(hosts, hostCnt, args::get(epochFile).c_str());
	
	if (totalEpochs < 0)
	{
	    std::cerr << "couldn't read in epcoh file " << std::endl;
	    return -2;
	}
    }
    else 
    {
	// Assign Epochs to Hosts: 
	totalEpochs = parseEpochFile(hosts, hostCnt, args::get(epochFile).c_str());
	
	if (totalEpochs < 0)
	{
	    std::cerr << "couldn't read in epcoh file " << std::endl;
	    return -2;
	}
    }
    gettimeofday (&tvStart, NULL);

    // create a thread to handle each host:
    Config::ServerThread *threads = new Config::ServerThread[hostCnt];
    int count = 0;
    int offset = 0;
    // Connect to each of the hosts:
    for (int i = 0; i < hostCnt; ++i )
    {       	
	threads[i].h = hosts + i;
	threads[i].offset = offset;
	threads[i].tid = 0;

	if (threads[i].h->_serverConfig.numEpochs() > 0)
	{
	    offset += threads[i].h->_serverConfig.numEpochs();
	    long rc = pthread_create (&(threads[i].tid), NULL, process_host, 
				      threads + i);
	    if (rc < 0) {
		std::cerr <<  "Cannot create server thread, rc=" << rc << std::endl;
		assert (0);
	    }
	}
	if (threads[i].h->_serverConfig.isTreeMerge() &&
	    threads[i].h->_serverConfig.hasTreeOutput())
	{
	    offset += 1;
	    long rc = pthread_create (&(threads[i].tid), NULL, process_host, 
				      threads + i);
	    if (rc < 0) {
		std::cerr <<  "Cannot create server thread, rc=" << rc << std::endl;
		assert (0);
	    }
	}
    }    

    // wait for all pthreads: 
    for (int i = 0; i < hostCnt; i++) 
    {
	if (threads[i].tid > 0)
	{
	    long rc = pthread_join(threads[i].tid, NULL);
	    if (rc < 0) fprintf (stderr, "Cannot join address thread, rc=%ld\n", rc); 
	}
    }

    // now do global analyzer... not sure what this needs just yet.       
    gettimeofday (&tvGAStart, NULL);
    if (!args::get(ckpt))
    {
	global_analyzer(args::get(gAnalyzer).c_str(),
			args::get(analyzer).c_str(), 
			args::get(outputDir).c_str(), 
			0,
			totalEpochs,
			&tvFlattenEnd);
    }

    gettimeofday (&tvEnd, NULL);

    if (args::get(tree))
    {
	sprintf(globalOutput, "%s/tree", args::get(outputDir).c_str());
	strcpy(serverFormat, "%s/subtree.%d");

	std::ofstream lsStream(globalOutput, std::ofstream::out);
	
	lsStream << "tree flatten time: " 
		 << Config::ms_diff(tvFlattenEnd,tvGAStart) << " ms\n";    
	lsStream << "tree global time: "
		 << Config::ms_diff(tvEnd, tvFlattenEnd) << " ms\n";
	
	lsStream << "tree input bytes: " << inputBytes << "\n";
	lsStream << "tree output bytes: " << outputBytes << "\n";
	
	lsStream.close();	

	count = 0;
	for (int i = 0; i < hostCnt; ++i)
	{
	    Config::Host &h = hosts[i];
	    if (h._serverConfig.hasTreeOutput())
	    {	    
		char ctrlOutput[256];
		sprintf(ctrlOutput, serverFormat , args::get(outputDir).c_str(), i);
		std::ofstream lsStream(ctrlOutput, std::ofstream::out);
		
		lsStream << "subtree total time: "
			 << Config::ms_diff(h.tvEnd,h.tvStart) << " ms\n";
		lsStream << "subtree  wait time: "
			 << Config::ms_diff(h.tvWaitStop, h.tvStart) << " ms\n";
		lsStream << "subtree output time: "
			 << Config::ms_diff(h.tvEnd, h.tvWaitStop) << " ms\n";
		lsStream << "subtree output stall time: "
			 << Config::ms_diff(tvGAStart, h.tvEnd) << " ms\n";
		
		lsStream.close();	
	    }
	}
    }
    else 
    {
	//get stats on each host
	for (int i = 0; i < hostCnt; i++)
	{
	    Config::ServerThread *st = threads + i;
	    int rc = getStats(st);
	    if (rc < 0)
		fprintf(stderr, "Couldn't get stats on %d\n", i);	
	}

	sprintf(globalOutput, "%s/global", args::get(outputDir).c_str());
	strcpy(serverFormat, "%s/streamctl.%d");
	std::ofstream lsStream(globalOutput, std::ofstream::out);
	
	lsStream << "log time: " 
		 << Config::ms_diff(tvGAStart, tvStart) << " ms\n";
	lsStream << "flatten time: " 
		 << Config::ms_diff(tvFlattenEnd,tvGAStart) << " ms\n";    
	lsStream << "global time: "
		 << Config::ms_diff(tvEnd, tvFlattenEnd) << " ms\n";
	
	lsStream << "global input bytes: " << inputBytes << "\n";
	lsStream << "global output bytes: " << outputBytes << "\n";
	
	lsStream.close();	


	count = 0;
	for (int i = 0; i < hostCnt; ++i)
	{
	    Config::Host &h = hosts[i];
	    for (int i = count; i < count + h.numEpochs(); ++i)
	    {		

		char ctrlOutput[256];
		sprintf(ctrlOutput, serverFormat , args::get(outputDir).c_str(), i);
		std::ofstream lsStream(ctrlOutput, std::ofstream::out);
		
		lsStream << "start: " 
			 << h.tvStart.tv_sec << "." << h.tvStart.tv_usec << std::endl;
		lsStream << "end: " 
			 << h.tvEnd.tv_sec << "." << h.tvEnd.tv_usec << std::endl;
		lsStream << "streamctl total time: "
			 << Config::ms_diff(h.tvEnd,h.tvStart) << " ms\n";
		lsStream << "streamctl wait time: "
			 << Config::ms_diff(h.tvWaitStop, h.tvStart) << " ms\n";
		lsStream << "streamctl output time: "
			 << Config::ms_diff(h.tvEnd, h.tvWaitStop) << " ms\n";
		lsStream << "streamctl output stall time: "
			 << Config::ms_diff(tvGAStart, h.tvEnd) << " ms\n";
		
		lsStream.close();	
	    }
	    count += h.numEpochs();
	}
    }


    return 0;
}
