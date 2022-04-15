#ifndef SLICE_H_
#define SLICE_H_

#include <unordered_set>
#include <unordered_map>
#include <iostream>

//using namespace std;

class Location {    
public:
    u_long _ip;
    u_long _dynamic_id; 

    Location(u_long ip, u_long dynamic_id);

    bool operator==(const Location &) const;
    bool isNull(void) const;
    friend std::ostream& operator<< (std::ostream &os, const Location &l);
};

class SliceCriteria { 
public:
    u_long _ip; 
    u_long _dynamic_id; 
    u_long _sliceLoc; 
    u_long _lastModifiedIp; // might be reasonable to differentiate this

    SliceCriteria(u_long ip, u_long dynamic_id, u_long sliceLoc, u_long lastModifiedIp); 

    bool operator==(const SliceCriteria &) const;
    friend std::ostream& operator<< (std::ostream &os, 
				     const SliceCriteria &l);
};

struct LocationHasher { 
    size_t operator() (const Location &l ) const { 
	return std::hash<u_long>() (l._dynamic_id) 
	    ^ (std::hash<u_long>() (l._ip) << 1);
    }
};

struct SliceCriteriaHasher { 
    size_t operator() (const SliceCriteria &sc ) const { 
	return std::hash<u_long>() (sc._ip) 
	    ^ std::hash<u_long>() (sc._lastModifiedIp) << 1; 
    }
};

class Flowback { 

public:    
    std::unordered_set<Location, LocationHasher> _nodes;
    std::unordered_multimap<Location, Location, LocationHasher> _links;
    std::unordered_set<SliceCriteria, SliceCriteriaHasher> _scNodes;

    Location addLocation(Location &node);
    void addLink(Location &parent, Location &child);
    SliceCriteria addSliceCriteria(SliceCriteria &sc); 

    void deserialize(std::istream &is);
    Flowback difference(Flowback &other); 

    friend std::ostream& operator<<(std::ostream &os, const Flowback &s);
};


#endif /* SLICE_H_ */
