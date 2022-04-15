#include "input_set.h"
#include <iostream>
#include <sstream>
#include <cstring>

#include <vector>
#include <algorithm>



IS::Location::Location(int16_t type, int16_t record_pid, u_long token_num,
	  u_long ip, u_long fileno) 
 {
     _type = type;
     _record_pid = record_pid;
     _token_num = token_num;
     _ip = ip;
     _fileno = fileno; 
}

IS::Location::Location(u_long token_num)
 {     
     _token_num = token_num;
     _type = 0;
     _record_pid = 0;
     _ip = 0;
     _fileno = 0;
}

bool IS::Location::operator==(const IS::Location &o) const
{
    return _token_num == o._token_num; 
}


IS::SliceCriteria::SliceCriteria(u_long ip, 
			     u_long sliceLoc)
{
    _ip = ip;
    _sliceLoc = sliceLoc;
}

bool IS::SliceCriteria::operator==(const IS::SliceCriteria &o) const 
{
    return _sliceLoc == o._sliceLoc && _ip == o._ip;
}


void IS::InputSet::addSliceCriteria (IS::SliceCriteria &node)
{ 
    _scNodes.insert(node);
}

void IS::InputSet::addLocation (struct IS::Location &l) 
{    
    _locations.insert(l);
}


void IS::InputSet::deserialize(std::istream &is) 
{
    char nodeid[128];
//    char ip[128];
    char *end;

    while (!is.eof()) { 
	is.getline(nodeid, 1024);
	if (strlen(nodeid) > 0) { 

	    u_long nid = strtoul(nodeid, &end, 16);
	    Location l(nid);
	    addLocation(l);
	}
    }
}

void IS::InputSet::deserialize_addresses(std::istream &is) 
{
    char ip[128];
    char *end;

    while (!is.eof()) { 
	is.getline(ip, 1024);
	u_long cip = strtoul(ip, &end, 16);
	if (cip != 0) {		
	    Location l(0,0,0,cip,0);
	    addLocation(l);

	if (cip == 0x848e1e7) { 
	    std::cerr << std::hex << "deserialize_addr, found " << cip
		      << " ip " << l._ip << std::endl;
	    
	}

	}
    }
}

void IS::InputSet::merge(IS::InputSet &other)
{
    for (auto &l : other._locations) 
	_locations.insert(l);
    
    for (auto &sc : other._scNodes) 
	_scNodes.insert(sc);
}

IS::InputSet IS::InputSet::diff(IS::InputSet &other)
{
    IS::InputSet n; 
    for (auto l : _locations) 
    {
	//if it isn't found. 
	if (other._locations.find(l) == 
	    other._locations.end())
	{
	    n.addLocation(l);
	}
    }
    return n;
}


void IS::InputSet::dump_addresses(std::ostream &os)
{
    os << "criteria:\n";
    os << "ips:\n";
    for (auto l : _locations)
    {
	os << std::hex << l._ip << std::endl;
    }
}
std::ostream& IS::operator<< (std::ostream &os, const IS::SliceCriteria &sc) 
{
    os << std::hex << "0x" << sc._sliceLoc << ":0x" << sc._ip;
    return os;
}

std::ostream& IS::operator<< (std::ostream &os, const struct IS::Location &l)
{
    os << std::hex << l._token_num << ":" << l._ip;   
    return os;
}

std::ostream& IS::operator<<(std::ostream &os, const IS::InputSet &s) 
{
    std::vector<uint32_t> nodes;       

    for (auto l : s._locations) { 
//	os << l << "\n";
	nodes.push_back(l._token_num);
    }
    std::sort(nodes.begin(), nodes.end());

    for (auto l : nodes){
	os << std::hex << l << "\n";
//	nodes.push_back(l._token_num);
    }    

    return os;
}


