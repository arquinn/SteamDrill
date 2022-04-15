TARGETS=parallel_flowback parallel_dift streamserver streamctl parse_flowback get_subtree_ips cmp_flowback get_cf
LIBUTIL:= ../../test/libutil.a
LIBH:= ../../test/util.h
PARSEKLIB = ../../test/parseklib.o
CXXFLAGS= -std=c++0x -Wall -Werror -g -gdwarf-2
DIFTFLAGS= -DDIFT -DUSE_SHMEM


GLIB_INCLUDES=-I/usr/include/glib-2.0 -I/usr/lib/i386-linux-gnu/glib-2.0/include

all: $(TARGETS)

parallel_dift: parallel_dift.cpp
	$(CXX) $(CXXFLAGS) $(DIFTFLAGS) -o $@ $< ../obj-ia32/maputil.o streamnw.cpp -lpthread -lrt

parallel_flowback: parallel_flowback.cpp streamnw.cpp merge_mapper.o
	$(CXX) $(CXXFLAGS) -o $@ $^ ../obj-ia32/maputil.o -lpthread -lrt -g

condensed_flowback.o: condensed_flowback.cpp condensed_flowback.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

cmp_flowback: cmp_flowback.cpp streamnw.cpp merge_mapper.o condensed_flowback.o
	$(CXX) $(CXXFLAGS) -o $@ $^ ../obj-ia32/maputil.o -lpthread -lrt 

streamserver: streamserver.cpp streamnw.cpp streamnw.h streamserver.h $(LIBH)
	$(CXX) $(CXXFLAGS) -o $@ $< streamnw.cpp $(LIBUTIL) -lrt -lpthread

streamctl: streamctl.cpp streamnw.cpp streamnw.h
	$(CXX) $(CXXFLAGS) -o streamctl streamctl.cpp streamnw.cpp $(PARSEKLIB) -lrt

merge_mapper.o: merge_mapper.cpp merge_mapper.h
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

slice_cmp: slice_cmp.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< ../obj-ia32/slice.o ../obj-ia32/input_set.o

parse_flowback: parse_flowback.cpp merge_mapper.o
	$(CXX) $(CXXFLAGS) -g -gdwarf-2 -o $@ $< merge_mapper.o ../obj-ia32/maputil.o  -lrt

get_cf: get_cf.cpp merge_mapper.o
	$(CXX) $(CXXFLAGS) -g -gdwarf-2 -o $@ $< merge_mapper.o ../obj-ia32/maputil.o  -lrt

get_subtree_ips: get_subtree_ips.cpp merge_mapper.o
	$(CXX) $(CXXFLAGS) -g -gdwarf-2 -o $@ $< merge_mapper.o ../obj-ia32/maputil.o  -lrt

clean:
	rm -f *.a *.o $(TARGETS)
