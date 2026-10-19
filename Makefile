# Build the PL/M-80 compiler and its runtime library.
#
#   make                 builds build/plm.rxbin and build/plmrt.rxbin
#   make test            compiles and runs every program in tests/ and compares the output
#   make c               builds the C89 version, c/plmc-c
#   make test-c          runs the programs in tests/ with the C89 version
#
# CREXX_BIN is the directory holding the cREXX tools (rxc, rxas, rxvm).
CREXX_BIN ?= $(dir $(shell which rxc))
RXC  = $(CREXX_BIN)/rxc
RXAS = $(CREXX_BIN)/rxas

all: build/plm.rxbin build/plmrt.rxbin

build/%.rxbin: %.crexx
	mkdir -p build
	cp $< build/$*.crexx
	cd build && $(RXC) -n $* > $*.log 2>&1 || (cat $*.log; exit 1)
	cd build && $(RXAS) $*

test: all
	./tests/run.sh

# the C89 version (see c/): one executable that compiles and runs
c:
	$(MAKE) -C c

test-c: c
	PLMC=$(CURDIR)/c/plmc-c ./tests/run.sh

clean:
	rm -rf build
	$(MAKE) -C c clean

.PHONY: all test clean c test-c
