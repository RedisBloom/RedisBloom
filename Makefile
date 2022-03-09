
ifneq ($(filter coverage show-cov upload-cov,$(MAKECMDGOALS)),)
COV=1
endif

ROOT=.
MK.pyver:=3

ifeq ($(VG),1)
override VALGRIND:=1
export VALGRIND
endif

ifeq ($(VALGRIND),1)
override DEBUG:=1
export DEBUG
endif

MK_ALL_TARGETS=bindirs deps build pack

include $(ROOT)/deps/readies/mk/main

#----------------------------------------------------------------------------------------------  

export T_DIGEST_C_BINDIR=$(ROOT)/bin/$(FULL_VARIANT.release)/t-digest-c
include $(ROOT)/build/t-digest-c/Makefile.defs

# export RMUTIL_BINDIR=$(ROOT)/bin/$(FULL_VARIANT.release)/rmutil
# include $(ROOT)/build/rmutil/Makefile.defs

#----------------------------------------------------------------------------------------------  

define HELPTEXT
make setup         # install packages required for build
make fetch         # download and prepare dependant modules

make build
  DEBUG=1          # build debug variant
  VARIANT=name     # use a build variant 'name'
  PROFILE=1        # enable profiling compile flags (and debug symbols) for release type
                   # You can consider this as build type release with debug symbols and -fno-omit-frame-pointer
  DEPS=1           # also build dependant modules
  COV=1            # perform coverage analysis (implies debug build)
make clean         # remove binary files
  ALL=1            # remove binary directories
  DEPS=1           # also clean dependant modules

make deps          # build dependant modules
make all           # build all libraries and packages

make run           # run redis-server with module
make test          # run all tests

make unit_tests    # run unit tests

make flow_tests    # run tests
  TEST=name        # run test matching 'name'
  TEST_ARGS="..."  # RLTest arguments
  SIMPLE=1         # shortcut for GEN=1 AOF=0 SLAVES=0 OSS_CLUSTER=0
  GEN=1            # run general tests on a standalone Redis topology
  AOF=1            # run AOF persistency tests on a standalone Redis topology
  SLAVES=1         # run replication tests on standalone Redis topology
  OSS_CLUSTER=1    # run general tests on an OSS Cluster topology
  SHARDS=num       # run OSS cluster with `num` shards (default: 3)
  RLEC=1           # flow tests on RLEC
  COV=1            # perform coverage analysis
  VALGRIND|VG=1    # run specified tests with Valgrind
  EXT=1            # run tests with existing redis-server running

make pack          # build packages (ramp & dependencies)

make docker        # build artifacts for given platform
  OSNICK=nick        # build for OSNICK `nick`
  TEST=1             # also run tests

make static-analysis   # Perform static analysis via fbinter

make benchmarks     # run all benchmarks
  BENCHMARK=file      # run benchmark specified by `file`
  BENCH_ARGS="..."    # redisbench_admin extra arguments

endef

#----------------------------------------------------------------------------------------------  

MK_CUSTOM_CLEAN=1

BINDIR=$(BINROOT)
SRCDIR=.

#----------------------------------------------------------------------------------------------

TARGET=$(BINROOT)/redisbloom.so

CC=gcc

CC_FLAGS = \
	-I. \
	-Isrc \
	-I$(ROOT)/deps \
	-I$(ROOT)/deps/murmur2 \
	-I$(ROOT)/deps/t-digest-c/src \
	-Wall \
	-fPIC \
	-pedantic \
	-std=gnu99 \
	-MMD -MF $(@:.o=.d) \
	-include $(SRCDIR)/src/common.h \
	-DREDISMODULE_EXPERIMENTAL_API

LD_FLAGS += 
LD_LIBS += \
	  $(T_DIGEST_C) \
	  -lc -lm -lpthread

ifeq ($(OS),linux)
SO_LD_FLAGS += -shared -Bsymbolic $(LD_FLAGS)
endif

ifeq ($(OS),macos)
SO_LD_FLAGS += -bundle -undefined dynamic_lookup $(LD_FLAGS)
DYLIB_LD_FLAGS += -dynamiclib $(LD_FLAGS)
endif

ifeq ($(PROFILE),1)
CC_FLAGS += -g -ggdb -fno-omit-frame-pointer
endif

ifeq ($(DEBUG),1)
CC_FLAGS += -g -ggdb -O0 -DDEBUG -D_DEBUG
LD_FLAGS += -g

ifeq ($(VALGRIND),1)
CC_FLAGS += -D_VALGRIND
endif
else ifeq ($(PROFILE),1)
CC_FLAGS += -O2
else
CC_FLAGS += -O3
endif

CC_FLAGS += $(CC_FLAGS.coverage)
LD_FLAGS += $(LD_FLAGS.coverage)

_SOURCES=\
	deps/bloom/bloom.c \
	deps/murmur2/MurmurHash2.c \
	deps/rmutil/util.c \
	src/rebloom.c \
	src/sb.c \
	src/cf.c \
	src/rm_topk.c \
	src/rm_tdigest.c \
	src/topk.c \
	src/rm_cms.c \
	src/cms.c

SOURCES=$(addprefix $(SRCDIR)/,$(_SOURCES))
HEADERS=$(patsubst $(SRCDIR)/%.c,$(SRCDIR)/%.h,$(SOURCES))
OBJECTS=$(patsubst $(SRCDIR)/%.c,$(BINDIR)/%.o,$(SOURCES))

CC_DEPS = $(patsubst $(SRCDIR)/%.c, $(BINDIR)/%.d, $(SOURCES) $(TEST_SOURCES))

include $(MK)/defs

#----------------------------------------------------------------------------------------------

MISSING_DEPS:=
ifeq ($(wildcard $(T_DIGEST_C)),)
MISSING_DEPS += $(T_DIGEST_C)
endif

ifneq ($(MISSING_DEPS),)
DEPS=1
endif

DEPENDENCIES=t-digest-c

ifneq ($(filter all deps $(DEPENDENCIES) pack,$(MAKECMDGOALS)),)
DEPS=1
endif

.PHONY: deps $(DEPENDENCIES)

#----------------------------------------------------------------------------------------------

.PHONY: all deps clean lint format pack run tests unit_tests flow_tests docker bindirs

all: bindirs $(TARGET)

include $(MK)/rules

#----------------------------------------------------------------------------------------------

ifeq ($(DEPS),1)

.PHONY: t-digest-c

deps: $(T_DIGEST_C)

t-digest-c: $(T_DIGEST_C)

$(T_DIGEST_C):
	@echo Building $@ ...
	$(SHOW)$(MAKE) --no-print-directory -C $(ROOT)/build/t-digest-c DEBUG=''

#----------------------------------------------------------------------------------------------

else

deps: ;

endif # DEPS

#----------------------------------------------------------------------------------------------

clean:
ifeq ($(ALL),1)
	-$(SHOW)rm -rf $(BINDIR) $(TARGET)
else
	-$(SHOW)[ -e $(BINDIR) ] && find $(BINDIR) -name '*.[oadh]' -type f -delete
	-$(SHOW)rm -f $(TARGET)
endif
ifdef ($(DEPS),1)
	-$(SHOW)$(MAKE) --no-print-directory -C $(ROOT)/build/rmutil  DEBUG='' clean
	-$(SHOW)$(MAKE) --no-print-directory -C $(ROOT)/build/fast_double_parser_c DEBUG='' clean
	-$(SHOW)$(MAKE) --no-print-directory -C $(ROOT)/tests/unit DEBUG='' clean
endif

-include $(CC_DEPS)

$(BINDIR)/%.o: $(SRCDIR)/%.c
	@echo Compiling $<...
	$(SHOW)$(CC) $(CC_FLAGS) -c $< -o $@

$(TARGET): $(BIN_DIRS) $(MISSING_DEPS) $(OBJECTS)
	@echo Linking $@...
	$(SHOW)$(CC) $(SO_LD_FLAGS) -o $@ $(OBJECTS) $(LD_LIBS)
ifeq ($(OS),macos)
	$(SHOW)$(CC) $(DYLIB_LD_FLAGS) -o $(patsubst %.so,%.dylib,$@) $(OBJECTS) $(LD_LIBS)
endif
#	$(SHOW)cd $(BINROOT)/..; ln -sf $(FULL_VARIANT)/$(notdir $(TARGET)) $(notdir $(TARGET))

#----------------------------------------------------------------------------------------------

NO_LINT_PATTERNS=

LINT_SOURCES=$(call filter-out2,$(NO_LINT_PATTERNS),$(SOURCES) $(HEADERS))

lint:
	$(SHOW)clang-format -Werror -n $(LINT_SOURCES)

format:
	$(SHOW)clang-format -i $(LINT_SOURCES)

#----------------------------------------------------------------------------------------------

run: $(TARGET)
	$(SHOW)redis-server --loadmodule $(realpath $(TARGET))

#----------------------------------------------------------------------------------------------

test: unit_tests flow_tests

#----------------------------------------------------------------------------------------------

unit_tests: $(TARGET)
	@echo Running unit tests...
	$(SHOW)$(MAKE) -C tests/unit build test

#----------------------------------------------------------------------------------------------

ifeq ($(SIMPLE),1)
export GEN=1
export SLAVES=0
export AOF=0
export OSS_CLUSTER=0
else
export GEN ?= 1
export SLAVES ?= 1
export AOF ?= 1
export OSS_CLUSTER ?= 1
endif

ifneq ($(RLEC),1)

flow_tests: #$(TARGET)
	$(SHOW)\
	MODULE=$(realpath $(TARGET)) \
	GEN=$(GEN) AOF=$(AOF) SLAVES=$(SLAVES) OSS_CLUSTER=$(OSS_CLUSTER) \
	VALGRIND=$(VALGRIND) \
	TEST=$(TEST) \
	$(ROOT)/tests/flow/tests.sh

else # RLEC

flow_tests: #$(TARGET)
ifeq ($(COV),1)
	$(COVERAGE_RESET)
endif
	$(SHOW)RLEC=1 $(ROOT)/tests/flow/tests.sh
ifeq ($(COV),1)
	$(COVERAGE_COLLECT_REPORT)
endif

endif # RLEC

#----------------------------------------------------------------------------------------------

ifneq ($(REMOTE),1)
BENCHMARK_ARGS=run-local
else
BENCHMARK_ARGS=run-remote 
endif

BENCHMARK_ARGS += \
	--module_path $(realpath $(TARGET)) \
	--required-module bf \
	--dso $(realpath $(TARGET))

ifneq ($(BENCHMARK),)
BENCHMARK_ARGS += --test $(BENCHMARK)
endif

ifneq ($(BENCH_ARGS),)
BENCHMARK_ARGS += $(BENCH_ARGS)
endif

benchmark: $(TARGET)
	$(SHOW)set -e; cd $(ROOT)/tests/benchmarks; redisbench-admin $(BENCHMARK_ARGS)

.PHONY: benchmark

#----------------------------------------------------------------------------------------------

VALGRIND_ARGS=\
	--leak-check=full \
	--keep-debuginfo=yes \
	--show-reachable=no \
	--show-possibly-lost=no \
	--track-origins=yes \
	--suppressions=$(ROOT)/tests/redis_valgrind.sup \
	-v redis-server

valgrind: $(TARGET)
	$(SHOW)valgrind $(VALGRIND_ARGS) --loadmodule $(realpath $(TARGET)) $(REDIS_ARGS) --dir /tmp

CALLGRIND_ARGS=\
	--tool=callgrind \
	--dump-instr=yes \
	--simulate-cache=no \
	--collect-jumps=yes \
	--collect-atstart=yes \
	--instr-atstart=yes \
	-v redis-server --protected-mode no --save "" --appendonly no

callgrind: $(TARGET)
	$(SHOW)valgrind $(CALLGRIND_ARGS) --loadmodule $(realpath $(TARGET)) $(REDIS_ARGS) --dir /tmp

.PHONY: valgrind callgrind

#----------------------------------------------------------------------------------------------

pack: $(TARGET)
	@echo Creating packages...
	$(SHOW)MODULE=$(realpath $(TARGET)) BINDIR=$(BINDIR) $(ROOT)/sbin/pack.sh

.PHONY: pack

#----------------------------------------------------------------------------------------------

INFER=infer
INFER_DOCKER=redisbench/infer-linux64:1.0.0

static-analysis: #$(TARGET)
ifeq ($(DOCKER),1)
	$(SHOW)docker run -v $(ROOT)/:/RedisBloom/ --user "$(username):$(usergroup)" $(INFER_DOCKER) \
		bash -c "cd RedisBloom && CC=clang infer run --fail-on-issue --biabduction --skip-analysis-in-path '.*rmutil.*'  -- make"
else
	$(SHOW)CC=clang $(INFER) run --fail-on-issue --biabduction --skip-analysis-in-path '.*rmutil.*' -- $(MAKE) VARIANT=infer
endif

#----------------------------------------------------------------------------------------------

coverage:
	$(SHOW)$(COVERAGE_RESET)
	$(SHOW)$(MAKE) test
	$(SHOW)$(COVERAGE_COLLECT_REPORT)

.PHONY: coverage

#----------------------------------------------------------------------------------------------

docker:
	$(SHOW)$(MAKE) -C build/docker

.PHONY: docker

#----------------------------------------------------------------------------------------------
