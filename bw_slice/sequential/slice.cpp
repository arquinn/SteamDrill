#include "slice.h"
#include <iostream>
#include <sstream>
#include <cstring>


S::Location::Location(u_long ip) 
{ 
    _ip = ip;
}
	
bool S::Location::operator==(const Location &other) const 
{
    return _ip == other._ip;
}



S::SliceCriteria::SliceCriteria(u_long ip, 
			     u_long sliceLoc, 
			     u_long lastModifiedIp) 
{
    _ip = ip;
    _sliceLoc = sliceLoc;
    _lastModifiedIp = lastModifiedIp; 
}

bool S::SliceCriteria::operator==(const SliceCriteria &o) const 
{
    return _sliceLoc == o._sliceLoc 
	&& _lastModifiedIp == o._lastModifiedIp;
}


S::Location S::Slice::addLocation(S::Location &node) { 
    if (_nodes.find(node) == _nodes.end())
    {
	_nodes.insert(node);
	return node;
    }
    return *(_nodes.find(node));
}

S::SliceCriteria S::Slice::addSliceCriteria(S::SliceCriteria &node) { 
    if (_scNodes.find(node) == _scNodes.end())
    {
	_scNodes.insert(node);
	return node;
    }
    return *(_scNodes.find(node));
}


S::Slice S::Slice::diff(Slice &other) { 
    Slice s;

    for (auto n: _nodes) { 
	bool found = false;
	for (auto m : other._nodes) { 
	    if (m == n) { 
		found = true;
		break;
	    }
	}
	if (!found)
	    s.addLocation(n);
    }    
    return s;
}

void S::Slice::deserialize(std::istream &is) { 
    char ip[128];
    char *end;
    bool ips = false; 

    while (!is.eof()) { 
	is.getline(ip, 1024);
	//ugh, not complicated enough! 

	if (!strnlen(ip, 1024) || 
	    !strncmp(ip, "criteria:",9))
	{
	    continue;
	}
	
	else if (!ips && !strncmp(ip, "ips:",4)) 
	{
	    ips = true;
	    continue;
	}
	else if (!ips) 
	{ 


	    std::stringstream s(ip);
	    char sliceLoc[128], lastModified[128];
	    s.getline(ip, 128, ':');
	    s.getline(sliceLoc, 128,':');
	    s.getline(lastModified, 128, ':');

	    S::SliceCriteria sc(strtoul(ip, &end, 16),
			     strtoul(sliceLoc, &end, 16),
			     strtoul(lastModified, &end, 16));
	    addSliceCriteria(sc);
			
	}
	else { 
	    S::Location l(strtol(ip, &end, 16));
	    addLocation(l);
	}
    }
}

void S::Slice::merge(S::Slice &other)
{
    for (auto &l : other._nodes)
	_nodes.insert(l);
    
    for (auto &sc : other._scNodes) 
	_scNodes.insert(sc);
}

void S::Slice::mergeIS(IS::InputSet &other)
{
    for (auto &l : other._locations) {
	S::Location cl(l._ip);
	if (cl._ip == 0x848e1e7) 
	    std::cerr << std::hex << "adding " << cl._ip << " to slice \n";
	_nodes.insert(cl);
    }
}


std::ostream& S::operator<< (std::ostream &os, const S::Location &l) 
{
    os << std::hex << "0x" << l._ip;
    return os;

}

std::ostream& S::operator<< (std::ostream &os, const S::SliceCriteria &sc) 
{
    os << std::hex << "0x" << sc._sliceLoc << ":0x" << sc._ip
       << ":0x"<< sc._lastModifiedIp;
    return os;

}

std::ostream& S::operator<<(std::ostream &os, const S::Slice &s) 
{
    os << std::hex << "criteria:\n";
    for (auto &l : s._scNodes) { 
	os << l << "\n";
    }
    os << "ips:\n";
    for (auto &l : s._nodes) { 
	os << l << "\n";
    }
    return os;
}
