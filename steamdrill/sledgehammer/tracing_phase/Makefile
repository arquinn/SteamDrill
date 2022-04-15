LDLIBS = -lrt -lboost_regex -lboost_serialization

CFILES = $(filter-out undo_log.cpp gadget_regions.cpp memory_checkpoint.cpp paranoid.cpp mem_region.cpp, $(wildcard *.cpp))
OBJECTS = $(patsubst %.cpp, $(OBJDIR)/%.o, $(CFILES))
EXT_OBJECTS = $(SDUTILINC)/$(OBJDIR)/tracer_configuration.o $(SDUTILINC)/$(OBJDIR)/shared_state.o

DEPS   = $(patsubst %.cpp, .deps/%.d, $(CFILES))


TARGETS = ptrace_wrapper
all: $(TARGETS)

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

ptrace_wrapper: $(OBJECTS) $(EXT_OBJECTS) $(LIBXED) $(LIBUTIL) $(SDUTIL)
	$(CXX) $(CXXFLAGS) -o $@ $^  $(LDLIBS)

clean:
	rm -r $(OBJDIR)/*.o $(TARGETS)

nuke:
	rm -rf .deps .dep_dirs_made
