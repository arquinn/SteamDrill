TARGETS = instrument.a
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

$(OBJDIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

-include $(DEPS)

instrument.a: $(OBJDIR)/instrument.o
	ar rv $@ $^
	ranlib $@

clean:
	rm -rf $(TARGETS) $(OBJDIR)/*.o

nuke:
	rm -rf .dep_dirs_made
	rm -rf .deps
