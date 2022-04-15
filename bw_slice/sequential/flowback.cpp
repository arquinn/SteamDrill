#include "flowback.h"

#include <cstring>
#include <cassert>
#include <iostream> 
#include <sstream>


using namespace std;

Location::Location(u_long ip, u_long dynamic_id) {
    _ip = ip;
    _dynamic_id = dynamic_id;
}

bool Location::operator==(const Location &other) const 
{
    return _dynamic_id == other._dynamic_id; 
}

bool Location::isNull(void) const
{
    return _dynamic_id == 0;
}

SliceCriteria::SliceCriteria(u_long ip, 
			     u_long dynamic_id,
			     u_long sliceLoc, 
			     u_long lastModifiedIp) 
{
    _ip = ip;
    _dynamic_id = dynamic_id;
    _sliceLoc = sliceLoc;
    _lastModifiedIp = lastModifiedIp; 
}

bool SliceCriteria::operator==(const SliceCriteria &o) const 
{
    return _sliceLoc == o._sliceLoc 
	&& _lastModifiedIp == o._lastModifiedIp;
}


Location Flowback::addLocation(Location &node) { 
    if (_nodes.find(node) == _nodes.end())
    {
	_nodes.insert(node);
	return node;
    }
    return *(_nodes.find(node));
}

void Flowback::addLink(Location &parent, Location &child) { 
    _links.insert(std::make_pair(parent, child));
}

SliceCriteria Flowback::addSliceCriteria(SliceCriteria &node) { 
    if (_scNodes.find(node) == _scNodes.end())
    {
	_scNodes.insert(node);
	return node;
    }
    return *(_scNodes.find(node));
}

std::ostream& operator<< (std::ostream &os, const Location &l) 
{
    os << std::hex << "0x" << l._dynamic_id << ":0x" << l._ip;
    return os;    
}

std::ostream& operator<< (std::ostream &os, const SliceCriteria &sc) 
{
    os << std::hex << "0x" << sc._dynamic_id << ":0x" << sc._sliceLoc 
       << ":0x" << sc._ip << ":0x"<< sc._lastModifiedIp;
    return os;

}

std::ostream& operator<<(std::ostream &os, const Flowback &s) 
{
    os << "criteria:\n";
    for (auto &l : s._scNodes) { 
	os << l << "\n";
    }
    os << "ips:\n";
    for (auto &l : s._nodes) { 
	os << l << "\n";
    }
    os << "links:\n";
    for (auto p : s._links) { 
	os << p.first << "," << p.second << endl;
    }
    return os;
}
