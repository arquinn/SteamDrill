#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <assert.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>

#include "../maputil.h"
#include "../postprocess/input_set.h"
#include "../graph_interface/node.h"
#include "../graph_interface/input_output.h"

//#define DETAILS 1

using namespace std;

//#define DEBUG_ID(a) (a == 0x572dc3)


void cmpIS(IS::InputSet &baseline, IS::InputSet &testing) 
{
    IS::InputSet oneLessTwo = baseline.diff(testing); 
    cerr << "there are " << oneLessTwo._locations.size() 
	 << " nodes in baseline but not testing\n";

//    for (auto &l : oneLessTwo._locations)
//	cerr << l << endl;

    IS::InputSet twoLessOne = testing.diff(baseline); 
    cerr << "there are " << twoLessOne._locations.size() 
	 << " nodes in testing but not baseline \n";

}

int mergeIS(IS::InputSet &is, int index, int offset)
{
    IS::InputSet n, adjusted; 
    int input_count;
    char fname[256];

    sprintf(fname,"/tmp/%d/dift",index);
    ifstream ifs(fname);
    n.deserialize(ifs);  

    ifs.close();
    
    for (auto &l : n._locations) {
	IS::Location lnew(l._token_num + offset);
	adjusted._locations.insert(lnew);	

#ifdef DEBUG_ID
	if (DEBUG_ID(lnew._token_num))
	{
	    cerr << hex << "token_num " << lnew._token_num << " found in epoch " << index
		 << " (originally " << lnew._token_num - offset << ")" << endl;
	}
#endif
    }

    is.merge(adjusted);    

    sprintf(fname,"/tmp/%d/num_inputs",index);
    ifs.open(fname);
    ifs >> std::hex >> input_count;
    ifs.close();


    return input_count;
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    char *baseline = argv[1];
    int num_epochs = atoi(argv[2]); 
    char filename[256];

    IS::InputSet baselineIS, startingAS, testingAS; 
    sprintf(filename, "%s/dift", baseline);
    ifstream stream(filename);
    baselineIS.deserialize(stream);
    stream.close();


    IS::InputSet is; 
    int64_t offset = 0;
    for (int i = 0; i < num_epochs; ++i) 
    {	
	offset += mergeIS(is, i, offset);	
    }    


    cmpIS(baselineIS, is); 


    sprintf(filename, "%s/starting_as", baseline);
    stream.open(filename);
    startingAS.deserialize(stream);
    stream.close();

    sprintf(filename, "/tmp/0/starting_as");
    stream.open(filename);
    testingAS.deserialize(stream);
    stream.close();

    cmpIS(startingAS, testingAS);

    return -1;
}

