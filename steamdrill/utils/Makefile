TARGETS = utils.a $(OBJDIR)/tracer_configuration.o $(OBJDIR)/shared_state.o
.PHONY: all
all: $(TARGETS)

##
## Automatic DEPS tracking
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

utils.a: $(OBJDIR)/utils.o
	ar rv $@ $^
	ranlib $@


$(OBJDIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/shared_state.o: shared_state.c shared_state.h
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<


shared_test: $(OBJDIR)/shared_state.o $(OBJDIR)/shared_test.o
	$(CXX) $(CFLAGS) -o $@ $^

-include $(DEPS)


clean:
	rm -rf $(TARGETS) $(OBJDIR)/*.o
