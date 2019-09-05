DEBUGFLAGS = -g -ggdb -O2
ifeq ($(DEBUG), 1)
	DEBUGFLAGS = -g -ggdb -O0 -pedantic
endif

# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
CPPFLAGS =  -Wall -Wno-unused-function $(DEBUGFLAGS) -fPIC -std=gnu99 -D_GNU_SOURCE
# CC:=$(shell sh -c 'type $(CC) >/dev/null 2>/dev/null && echo $(CC) || echo gcc')

# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	CC=gcc
	LD=gcc
	SHOBJ_CFLAGS ?=  -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -shared -Wl,-Bsymbolic,-Bsymbolic-functions
else
	CC=clang
	CFLAGS += -mmacosx-version-min=10.6
	SHOBJ_CFLAGS ?= -dynamic -fno-common -g -ggdb
	SHOBJ_LDFLAGS ?= -dylib -exported_symbol _RedisModule_OnLoad -macosx_version_min 10.6
endif

ROOT=$(shell pwd)
# Flags for preprocessor
LDFLAGS = -lm -lc

CPPFLAGS += -I$(ROOT) -I$(ROOT)/contrib
SRCDIR := $(ROOT)/src
MODULE_OBJ = $(SRCDIR)/rebloom.o
MODULE_SO = $(ROOT)/redisbloom.so

DEPS = $(ROOT)/contrib/MurmurHash2.o \
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
LDFLAGS += -fprofile-arcs
endif

all: $(MODULE_SO)

$(MODULE_SO): $(MODULE_OBJ) $(DEPS)
	$(LD) $^ -o $@ $(SHOBJ_LDFLAGS) $(LDFLAGS)

build: all
	$(MAKE) -C tests build-test

test: $(MODULE_SO)
	$(MAKE) -C tests test

build-test: $(MODULE_SO)
	$(MAKE) -C tests build-test

perf:
	$(MAKE) -C tests perf


package: $(MODULE_SO)
	mkdir -p $(ROOT)/build
	ramp-packer -vvv -m ramp.yml -o "$(ROOT)/build/rebloom.{os}-{architecture}.latest.zip" "$(MODULE_SO)"

clean:
	$(RM) $(MODULE_OBJ) $(MODULE_SO) $(DEPS)
	$(RM) -f print_version
	$(RM) -rf build
	$(RM) -rf tmp
	find . -name '*.gcov' -delete
	find . -name '*.gcda' -delete
	find . -name '*.gcno' -delete
	$(MAKE) -C tests clean

distclean: clean

docker:
	docker build -t redislabs/rebloom .

docker_push: docker
	docker push redislabs/rebloom:latest

# Compile an executable that prints the current version
print_version:  $(SRCDIR)/version.h $(SRCDIR)/print_version.c
	@$(CC) -o $@ -DPRINT_VERSION_TARGET $(SRCDIR)/$@.c

#	$(MAKE) CFLAGS="-fprofile-arcs -ftest-coverage" LDFLAGS="-fprofile-arcs"

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
