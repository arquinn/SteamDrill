##### Makefile for the streamserver stuff

##
## Global variables used throughout the file
##
TARGETS = validate
all: $(TARGETS)

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


validate: $(OBJDIR)/validate.o $(PARSER_LIBS)
	$(CXX)  $^ -o $@ $(PARSER_SHARED_LIBS)

##
## Standard clean rules
##

.PHONY: clean nuke

clean:
	rm -r $(TARGETS) $(OBJDIR)/*.o

nuke:
	rm -rf .deps .dep_dirs_made
