export MUSL = $(realpath ../lib/OmniTable-Musl/install)
export CXXFLAGS += -I$(SDUTILINC)


### how hard would it be to add a reasonable makefile for isolation_pass?
SUBDIRS = server $(filter-out analysis_phase/ log/ tracer_examples/ server/ isolation_pass/, $(wildcard */))

all: $(SUBDIRS)

$(SUBDIRS):
#	@echo $@
	$(MAKE) -C $@

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@; done

nuke:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir $@; done

.PHONY: all $(SUBDIRS)
