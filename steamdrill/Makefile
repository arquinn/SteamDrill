### things that you can change
export OBJDIR = obj

##### external dependencies
   ## paths to include directories
export SDEV = $(realpath ../test)
export XEDINC = $(realpath ../xed-kit/include)
   ## paths to specific libraries
export LIBXED = $(realpath ../xed-kit/lib/libxed.a)
export LIBUTIL = $(SDEV)/libutil.a $(SDEV)/libmkparts.a

##### library deps in this repo.
   ## paths to the inlucd paths
export LIBSINC = $(realpath lib)
export CAPINC = $(realpath lib/capstone/include)
export SPDINC  = $(realpath lib/spdlog/include)
export ELFINC = $(realpath lib/elfutils/install/include)
export SQLITE = $(realpath lib/sqlite-amalgamation-3340100)
export ARGSINC = $(realpath lib/args)
   ## links to the build/library paths.
LIBELF_LIB = $(realpath lib/elfutils/install/lib)
CAPSTONE_LIB = $(realpath lib/capstone/build/)


##### things that we produce
   ## libraries:
export SDUTIL = $(realpath utils)/utils.a
export LIBS = $(realpath query_generator)/query_generator.a\
	      $(realpath instrument)/instrument.a\
	      $(realpath protos)/steamdrill.pb.o\
	      $(realpath binary_parser)/binary_parser.a\
	      $(SDUTIL)\
	      $(realpath utils)/$(OBJDIR)/tracer_configuration.o

   ##include paths
export SDUTILINC=$(realpath utils)
export CONFIGINC=$(realpath .)

export CXX = g++
export CC = gcc
export CFLAGS = -I$(SQLITE) -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_THREADSAFE=0 -O4 #-g -gdwarf-2 #-fsanitize=address

export CXXFLAGS =-std=c++17 -I$(SDEV) -I$(XEDINC) -I$(CAPINC) -I$(SPDINC) -I$(ELFINC) -I$(ARGSINC)\
		 -I$(CONFIGINC) -I$(SQLITE) -DSQLITE_OMNT_LOAD_EXTENSION -DSQLITE_THREADSAFE=0 -O4 #-g -gdwarf-2 # -fsanitize=address
#export LDFLAGS = -fsanitize=address

export LDLIBS=$(LIB_ELF) $(LIB_DW) $(LIB_CAPSTONE)
export PARSER_SHARED_LIBS=-L$(LIBELF_LIB) -L$(CAPSTONE_LIB) -lelf -ldw  -lz -lcapstone -lboost_serialization `pkg-config --libs protobuf`

LIBDIRS = lib/OmniTable-Musl
SUBDIRS = protos query_generator binary_parser utils instrument
DIRS = server static_tables sledgehammer
ALLDIRS = $(SUBDIRS) $(DIRS)



.PHONY:all
all: $(DIRS)

$(LIBDIRS):
	$(MAKE) -C $@
	$(MAKE) -C $@ install

$(SUBDIRS): $(LIBDIRS)
	$(MAKE) -C $@

$(DIRS): $(SUBDIRS)
	$(MAKE) -C $@

clean:
	for dir in $(ALLDIRS); do $(MAKE) -C $$dir $@; done

nuke:
	for dir in $(ALLDIRS); do $(MAKE) -C $$dir $@; done

.PHONY: all $(SUBDIRS) clean


