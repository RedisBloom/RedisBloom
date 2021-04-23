CC?=gcc
INFER?=./deps/infer
INFER_DOCKER?=redisbench/infer-linux64:1.0.0

DEBUGFLAGS = -g -ggdb -O2
ifeq ($(DEBUG), 1)
	DEBUGFLAGS = -g -ggdb -O0 -pedantic
endif

# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
username := $(shell sh -c 'id -u')
usergroup := $(shell sh -c 'id -g')
CPPFLAGS =  -Wall -Wno-unused-function $(DEBUGFLAGS) -fPIC -std=gnu99 -D_GNU_SOURCE
# CC:=$(shell sh -c 'type $(CC) >/dev/null 2>/dev/null && echo $(CC) || echo gcc')

# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	LD=$(CC)
	SHOBJ_CFLAGS ?=  -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -shared -Wl,-Bsymbolic,-Bsymbolic-functions
else
	# version 10.15 changed SDK dir
	# https://stackoverflow.com/questions/58278260/cant-compile-a-c-program-on-a-mac-after-upgrading-to-catalina-10-15
	MACOS_VERSION = $(shell sh -c 'sw_vers -productVersion 2>/dev/null | grep -oE '[0-9]+\.[0-9]+' ' )
	ifeq ($(shell expr $(MACOS_VERSION) \>= 10.15), 1)
		SHOBJ_LDFLAGS ?= -syslibroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
	endif
	CC=clang
	CFLAGS ?= -dynamic -fcommon -g -ggdb
	SHOBJ_LDFLAGS += -dylib -exported_symbol _RedisModule_OnLoad
endif

ROOT=$(shell pwd)

LDFLAGS = -lm -lc
CPPFLAGS += -I$(ROOT)/contrib -I$(ROOT) -I$(ROOT)/src -I$(ROOT)/deps/t-digest-c/src
SRCDIR := $(ROOT)/src
MODULE_OBJ = $(SRCDIR)/rebloom.o
MODULE_SO = $(ROOT)/redisbloom.so
LIBTDIGEST_LIBDIR = $(ROOT)/deps/t-digest-c/build/src
LIBTDIGEST = $(LIBTDIGEST_LIBDIR)/libtdigest_static.a

# Flags for preprocessor
MODULE_LDFLAGS = -lm -lc -L$(LIBTDIGEST_LIBDIR) -ltdigest_static

DEPS = $(ROOT)/contrib/MurmurHash2.o \
	   $(ROOT)/rmutil/util.o \
	   $(SRCDIR)/sb.o \
	   $(SRCDIR)/cf.o \
	   $(SRCDIR)/rm_topk.o \
	   $(SRCDIR)/rm_tdigest.o \
	   $(SRCDIR)/topk.o \
	   $(SRCDIR)/rm_cms.o \
	   $(SRCDIR)/cms.o 

DEPS_TEST = $(ROOT)/contrib/MurmurHash2.o \
	   $(ROOT)/rmutil/util.o \
	   $(SRCDIR)/sb.o \
	   $(SRCDIR)/cf.o \
	   $(SRCDIR)/rm_topk.o \
	   $(SRCDIR)/topk.o \
	   $(SRCDIR)/rm_cms.o \
	   $(SRCDIR)/cms.o

export 

ifeq ($(COV),1)
CFLAGS += -fprofile-arcs -ftest-coverage
MODULE_LDFLAGS += -fprofile-arcs
endif

all: $(MODULE_SO)

$(MODULE_SO): $(MODULE_OBJ) $(DEPS) $(LIBTDIGEST)
	$(LD) $^ -o $@ $(SHOBJ_LDFLAGS) $(MODULE_LDFLAGS)

$(LIBTDIGEST):
	$(MAKE) -C deps/t-digest-c library_static

libtdigest: $(LIBTDIGEST)


build: all
	$(MAKE) -C tests


TEST_REPORT_DIR ?= $(PWD)

ifeq ($(SIMPLE),1)
export GEN=1
export SLAVES=0
export AOF=0
export CLUSTER=0
else
export GEN ?= 1
export SLAVES ?= 1
export AOF ?= 1
export CLUSTER ?= 0
endif

test: $(MODULE_SO)
	$(MAKE) -C tests/unit test
	MODULE=$(realpath $(MODULE_SO)) \
	CLUSTER=$(CLUSTER) \
	GEN=$(GEN) AOF=$(AOF) SLAVES=$(SLAVES) \
	VALGRIND=$(VALGRIND) \
	$(ROOT)/tests/flow/tests.sh

perf:
	$(MAKE) -C tests perf

lint:
	clang-format -style=file -Werror -n $(SRCDIR)/*

setup:
	@echo Setting up system...
	./opt/build/get-fbinfer.sh
	./deps/readies/bin/getpy3
	python3 -m pip install -r ./deps/readies/paella/requirements.txt

static-analysis-docker:
	$(MAKE) clean
	$(MAKE) libtdigest
	docker run -v $(ROOT)/:/RedisBloom/ --user "$(username):$(usergroup)" $(INFER_DOCKER) bash -c "cd RedisBloom && CC=clang infer run --fail-on-issue --biabduction --skip-analysis-in-path ".*rmutil.*"  -- make"

static-analysis:
	$(MAKE) clean
	$(MAKE) libtdigest
	 CC=clang $(INFER) run --fail-on-issue --biabduction --skip-analysis-in-path ".*rmutil.*" -- $(MAKE)
	
format:
	clang-format -style=file -i $(SRCDIR)/*

package: $(MODULE_SO)
	mkdir -p $(ROOT)/build
	ramp-packer -vvv -m ramp.yml -o "$(ROOT)/build/rebloom.{os}-{architecture}.latest.zip" "$(MODULE_SO)"

clean:
	$(RM) $(MODULE_OBJ) $(MODULE_SO) $(DEPS)
	$(RM) -f print_version
	$(RM) -rf build
	$(RM) -rf infer-out
	$(RM) -rf tmp
	find . -name '*.gcov' -delete
	find . -name '*.gcda' -delete
	find . -name '*.gcno' -delete
	$(MAKE) -C deps/t-digest-c clean
	$(MAKE) -C tests clean

distclean: clean

docker:
	docker build -t redislabs/rebloom .

docker_push: docker
	docker push redislabs/rebloom:latest

# Compile an executable that prints the current version
print_version:  $(SRCDIR)/version.h $(SRCDIR)/print_version.c
	@$(CC) -o $@ -DPRINT_VERSION_TARGET $(SRCDIR)/$@.c

#   $(MAKE) CFLAGS="-fprofile-arcs -ftest-coverage" LDFLAGS="-fprofile-arcs"

COV_DIR=tmp/lcov

cov coverage:
	@$(MAKE) clean
	@$(MAKE) test COV=1
	mkdir -p $(COV_DIR)
	gcov -c -b $(SRCDIR)/* > /dev/null 2>&1
	lcov -d . -c -o $(COV_DIR)/gcov.info --no-external > /dev/null 2>&1
	lcov -r $(COV_DIR)/gcov.info "*test*" "*contrib*" "*redismodule.h" "*util.c*" -o $(COV_DIR)/gcov.info > /dev/null 2>&1
	lcov -l $(COV_DIR)/gcov.info
	genhtml --legend -o $(COV_DIR)/report $(COV_DIR)/gcov.info > /dev/null 2>&1
