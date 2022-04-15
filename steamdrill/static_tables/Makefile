##### Makefile for the streamserver stuff

##
## Global variables used throughout the file
##
TARGETS = staticFunctions globals variables assemOps mem_maps funcParams

LIBS=../binary_parser/binary_parser.a

all: $(TARGETS)

## types doesn't work. because F*** it!

##
## For automatic dependency tracking
##

CFILES=$(wildcard *.cpp)
DEPS=$(patsubst %.cpp, .deps/%.d, $(CFILES))

.deps/%.d: %.cpp .dep_dirs_made
	mkdir -p $(dir $@);
	$(CXX) $(CXXFLAGS) -MM $< -MF $@.$$$$;\
	sed 's,$(notdir $*).o[ :]*,$(OBJDIR)/$(basename $*).o $@: ,g' < $@.$$$$ > $@;\
	rm -f $@.$$$$

.dep_dirs_made:
	@touch .dep_dirs_made
	@mkdir .deps

$(OBJDIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

-include $(DEPS)

##
## For each target
##

#	 	 $(LIB_CAPSTONE)
#		 $(LIB_ELF)
#		 $(LIB_DW)

assemOps: $(OBJDIR)/assemOps.o $(OBJDIR)/staticLibrary.o $(PARSER_LIBS) $(LIBUTIL) $(LIBS)
	$(CXX) $^ -o $@ $(LDLIBS) -lboost_regex $(PARSER_SHARED_LIBS) $(LIBUTIL) $(LDFLAGS)


staticFunctions: $(OBJDIR)/staticFunctions.o\
		 $(OBJDIR)/staticLibrary.o\
		 $(LIBUTIL)\
	         $(LIBS)
	$(CXX)  $^ -o $@  -lboost_regex $(PARSER_SHARED_LIBS) $(LDFLAGS)

funcParams: $(OBJDIR)/funcParams.o\
		 $(OBJDIR)/staticLibrary.o\
		 $(PARSER_LIBS)\
	         $(LIBS)\
		 $(LIBUTIL)
	$(CXX)  $^ -o $@ -lboost_regex $(PARSER_SHARED_LIBS) $(LIBUTIL) $(LDFLAGS)

globals: $(OBJDIR)/globals.o\
	 $(OBJDIR)/staticLibrary.o\
	 $(PARSER_LIBS)\
	 $(LIBS)\
	 $(LIBUTIL)
	$(CXX) $^ -o $@ -lboost_regex $(PARSER_SHARED_LIBS) $(LIBUTIL) $(LDFLAGS)

mem_maps: $(OBJDIR)/mem_maps.o\
	     $(OBJDIR)/staticLibrary.o\
	     $(PARSER_LIBS)\
	     $(SDUTIL)\
	     $(LIBS)\
	     $(LIBUTIL)
	$(CXX)  $^ -o $@ $(PARSER_SHARED_LIBS) $(LDFLAGS)


types: $(OBJDIR)/types.o $(OBJDIR)/staticLibrary.o $(PARSER_LIBS) $(LIBUTIL) $(LIBS)
	$(CXX) $^ -o $@ -lboost_regex $(PARSER_SHARED_LIBS) $(LIBUTIL) $(LDFLAGS)

variables: $(OBJDIR)/variables.o $(OBJDIR)/staticLibrary.o $(PARSER_LIBS) $(LIBUTIL) $(LIBS)
	$(CXX) $^ -o $@ $(LDLIBS) -lboost_regex $(PARSER_SHARED_LIBS) $(LIBUTIL) $(LDFLAGS)


##
## Standard clean rules
##

.PHONY: clean nuke

clean:
	rm -r $(TARGETS) $(OBJDIR)/*.o

nuke:
	rm -rf .deps .dep_dirs_made
