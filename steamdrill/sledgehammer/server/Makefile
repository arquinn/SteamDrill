##### Makefile for the streamserver stuff

##
## Global variables used throughout the file
##
TARGETS = streamserver streamctl pound_cpu sequential test_configuration
all: $(TARGETS)

EXT_OBJS = $(SDUTILINC)/$(OBJDIR)/tracer_configuration.o

LDLIBS=-ldl -lpthread -ldw -lelf -lboost_serialization
#CXXFLAGS = -std=c++0x
##
## For automatic dependency tracking
##

CFILES=$(wildcard *.cpp)
DEPS=$(patsubst %.cpp, .deps/%.d, $(CFILES))

.deps/%.d: %.cpp .dep_dirs_made\
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


streamserver: $(OBJDIR)/streamserver.o $(OBJDIR)/config.o $(OBJDIR)/streamnw.o $(LIBUTIL)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

streamctl: $(OBJDIR)/streamctl.o $(OBJDIR)/config.o $(OBJDIR)/streamnw.o $(LIBUTIL)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

sequential: $(OBJDIR)/sequential.o $(OBJDIR)/config.o $(OBJDIR)/streamnw.o $(LIBUTIL)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)

test_configuration: $(OBJDIR)/test_configuration.o $(EXT_OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@  $(LDLIBS) -lboost_serialization -lboost_regex


pound_cpu: $(OBJDIR)/pound_cpu.o $(LIBUTIL)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LDLIBS)


##
## Standard clean rules
##

.PHONY: clean nuke

clean:
	rm -r $(TARGETS) $(OBJDIR)/*.o

nuke:
	rm -rf .deps .dep_dirs_made
