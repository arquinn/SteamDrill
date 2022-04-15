##### Makefile for the streamserver stuff

##
## Global variables used throughout the file
##
#../binary_parser/binary_parser.a


TARGETS = steamdrill_process sqlite_shell import_type add_partition check_partition get_blocks parse_blocks

all: $(TARGETS)

##
## For automatic dependency tracking
##

CFILES=$(filter-out tracing_function_base.cpp, $(wildcard *.cpp))
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

$(OBJDIR)/sqlite3.o: $(SQLITE)/sqlite3.c
	$(CC) $(CFLAGS) $^ -c -o $@
-include $(DEPS)

##
## For each target
##

steamdrill_process: $(OBJDIR)/steamdrill_process.o\
		   $(OBJDIR)/sh_session.o\
	   	   $(OBJDIR)/partition_manager.o\
	    	   $(OBJDIR)/timer.o\
		   $(LIBS)\
		   $(LIBUTIL)
	$(CXX) $^ -o  $@ $(LDLIBS) $(PARSER_SHARED_LIBS) $(LDFLAGS)

add_partition: $(OBJDIR)/add_partition.o\
		$(OBJDIR)/partition_manager.o\
		$(LIBUTIL)
	$(CXX) $^ -o $@ $(LDLIBS) $(LDFLAGS)


check_partition: $(OBJDIR)/check_partition.o\
		$(OBJDIR)/partition_manager.o\
		$(LIBUTIL)
	$(CXX) $^ -o $@ $(LDLIBS) $(LDFLAGS)


sqlite_shell: $(SQLITE)/shell.c $(SQLITE)/sqlite3.c
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

import_type: $(OBJDIR)/sqlite3.o $(OBJDIR)/import_type.o
	$(CXX) $(CFLAGS) $^ -o $@ -lpthread

get_blocks: $(OBJDIR)/get_blocks.o $(LIBS)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS) $(PARSER_SHARED_LIBS)

parse_blocks: $(OBJDIR)/parse_blocks.o $(LIBS)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS) $(PARSER_SHARED_LIBS)



##
## Standard clean rules
##

.PHONY: clean nuke

clean:
	rm -r $(TARGETS) $(OBJDIR)/*.o

nuke:
	rm -rf .deps .dep_dirs_made
