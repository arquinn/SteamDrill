targets=query_generator.a

ALL:$(targets)


.PHONY: all
all: $(TARGETS)

##
## Automatic DEPS tracking
##



CFILES=$(wildcard *.cpp)
DEPS=$(patsubst %.cpp, .deps/%.d, $(CFILES))

.deps/%.d: %.cpp .dep_dirs_made
	mkdir -p $(dir $@);
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) -MM $< -MF $@.$$$$;\
	sed 's,$(notdir $*).o[ :]*,$(OBJDIR)/$(basename $*).o $@: ,g' < $@.$$$$ > $@;\
	rm -f $@.$$$$

.dep_dirs_made:
	@touch .dep_dirs_made
	@mkdir .deps

$(OBJDIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LLVM_CXXFLAGS) -c -o $@ $<

$(OBJDIR)/sqlite3.o: $(SQLITE)/sqlite3.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

-include $(DEPS)


query_generator.a: $(OBJDIR)/generate_tracers.o\
		   $(OBJDIR)/tracer_builder.o\
		   $(OBJDIR)/tracer_module.o\
	 	   $(OBJDIR)/generate_libc_config.o\
	 	   $(OBJDIR)/variable_type.o\
	 	   $(OBJDIR)/sqlite3.o
	ar rv $@ $^
	ranlib $@

clean:
	rm -r $(targets) $(OBJDIR)/*.o

nuke:
	rm -rf .dep_dirs_made .deps/
