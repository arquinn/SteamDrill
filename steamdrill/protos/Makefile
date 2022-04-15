##### Makefile for grpc elements
## Based on the example makefile from grpc

PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH = `which $(GRPC_CPP_PLUGIN)`


TARGETS = steamdrill.pb.o
#steamdrill.grpc.pb.o

all: $(TARGETS)

#.PRECIOUS: %.grpc.pb.cpp
#%.grpc.pb.cpp: %.proto
#	$(PROTOC) --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<
#	mv $*.grpc.pb.cc $@

.PRECIOUS: %.pb.cpp
%.pb.cpp: %.proto
	$(PROTOC) --cpp_out=. $<
	mv $*.pb.cc $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

##
## Standard clean rules
##

.PHONY: clean
clean:
	rm -r $(TARGETS)
