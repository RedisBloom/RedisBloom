
ROOT=.

include $(ROOT)/deps/readies/mk/main

MK_ALL_TARGETS=bindirs deps build

#----------------------------------------------------------------------------------------------

# export T_DIGEST_C_BINDIR=$(ROOT)/bin/$(FULL_VARIANT.release)/t-digest-c
export T_DIGEST_C_BINDIR=$(ROOT)/bin/$(FULL_VARIANT)/t-digest-c
include $(ROOT)/build/t-digest-c/Makefile.defs

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

make unit-tests    # run unit tests

make flow-tests    # run tests
  TEST=name        # run test matching 'name'
  TEST_ARGS="..."  # RLTest arguments
  QUICK=1          # shortcut for GEN=1 AOF=0 SLAVES=0 OSS_CLUSTER=0
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
make upload-artifacts   # copy snapshot packages to S3
  OSNICK=nick             # copy snapshots for specific OSNICK
make upload-release     # copy release packages to S3

common options for upload operations:
  STAGING=1             # copy to staging lab area (for validation)
  FORCE=1               # allow operation outside CI environment
  VERBOSE=1             # show more details
  NOP=1                 # do not copy, just print commands

make coverage      # perform coverage analysis
make show-cov      # show coverage analysis results (implies COV=1)
make upload-cov    # upload coverage analysis results to codecov.io (implies COV=1)

make docker        # build for specific Linux distribution
  OSNICK=nick        # Linux distribution to build for
  REDIS_VER=ver      # use Redis version `ver`
  TEST=1             # test aftar build
  PACK=1             # create packages
  ARTIFACTS=1        # copy artifacts from docker image
  PUBLISH=1          # publish (i.e. docker push) after build

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

CC_C_STD=gnu99

CC_COMMON_H=$(SRCDIR)/src/common.h

define CC_DEFS +=
	_GNU_SOURCE
	REDIS_MODULE_TARGET
	REDISMODULE_SDK_RLEC
endef

define CC_INCLUDES +=
	.
	src
	$(ROOT)/deps/RedisModulesSDK
	$(ROOT)/deps
	$(ROOT)/deps/murmur2
	$(ROOT)/deps/t-digest-c/src
endef

LD_LIBS += $(T_DIGEST_C)

ifeq ($(VG),1)
	CC_DEFS += _VALGRIND
endif

ifeq ($(PROFILE),1)
SO_LD_FLAGS += -flto
endif

ifneq ($(DEBUG),1)
SO_LD_FLAGS += -flto
endif

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

ifeq ($(DEBUG),1)
_SOURCES += deps/readies/cetara/diag/gdb.c
endif

SOURCES=$(addprefix $(SRCDIR)/,$(_SOURCES))
HEADERS=$(sort $(wildcard src/*.h $(patsubst %.c,%.h,$(SOURCES))))
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

.PHONY: all deps clean lint format pack run tests unit-tests flow-tests docker bindirs

all: bindirs $(TARGET)

include $(MK)/rules

#----------------------------------------------------------------------------------------------

ifeq ($(DEPS),1)

.PHONY: t-digest-c

deps: $(T_DIGEST_C)

t-digest-c: $(T_DIGEST_C)

$(T_DIGEST_C):
	@echo Building $@ ...
	$(SHOW)$(MAKE) --no-print-directory -C $(ROOT)/build/t-digest-c

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
	-$(SHOW)$(MAKE) --no-print-directory -C $(ROOT)/build/t-digest-c clean
endif

-include $(CC_DEPS)

$(BINDIR)/%.o: $(SRCDIR)/%.c
	@echo Compiling $<...
	$(SHOW)$(CC) $(CC_FLAGS) -c $< -o $@

$(TARGET): $(BIN_DIRS) $(MISSING_DEPS) $(OBJECTS)
	@echo Linking $@...
	$(SHOW)$(CC) $(SO_LD_FLAGS) $(LD_FLAGS) -o $@ $(OBJECTS) $(LD_LIBS)

#----------------------------------------------------------------------------------------------

NO_LINT_PATTERNS=./deps/

LINT_SOURCES=$(call filter-out2,$(NO_LINT_PATTERNS),$(SOURCES) $(HEADERS))

lint:
	$(SHOW)clang-format -Werror -n $(LINT_SOURCES)

format:
	$(SHOW)clang-format -i $(LINT_SOURCES)

#----------------------------------------------------------------------------------------------

run: $(TARGET)
	$(SHOW)redis-server --loadmodule $(realpath $(TARGET))

#----------------------------------------------------------------------------------------------

test: unit-tests flow-tests

#----------------------------------------------------------------------------------------------

unit-tests: $(TARGET)
	@echo Running unit tests...
	$(SHOW)$(MAKE) -C tests/unit build test

#----------------------------------------------------------------------------------------------

ifeq ($(QUICK),1)
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

flow-tests: #$(TARGET)
	$(SHOW)\
	MODULE=$(realpath $(TARGET)) \
	GEN=$(GEN) AOF=$(AOF) SLAVES=$(SLAVES) OSS_CLUSTER=$(OSS_CLUSTER) \
	VALGRIND=$(VALGRIND) \
	TEST=$(TEST) \
	$(ROOT)/tests/flow/tests.sh

else # RLEC

flow-tests: #$(TARGET)
	$(SHOW)RLEC=1 $(ROOT)/tests/flow/tests.sh

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

pack: $(TARGET)
	@echo Creating packages...
	$(SHOW)BINDIR=$(BINDIR) $(ROOT)/sbin/pack.sh $(realpath $(TARGET))

upload-release:
	$(SHOW)RELEASE=1 ./sbin/upload-artifacts

upload-artifacts:
	$(SHOW)SNAPSHOT=1 ./sbin/upload-artifacts

.PHONY: pack upload-artifacts upload-release

#----------------------------------------------------------------------------------------------

INFER=infer
INFER_DOCKER=redisbench/infer-linux64:1.0.0

static-analysis: #$(TARGET)
ifeq ($(DOCKER),1)
	$(SHOW)docker run -v $(ROOT)/:/RedisBloom/ --user "$(username):$(usergroup)" $(INFER_DOCKER) \
		bash -c "cd RedisBloom && CC=clang infer run --fail-on-issue --biabduction --skip-analysis-in-path  -- make"
else
	$(SHOW)CC=clang $(INFER) run --fail-on-issue --biabduction --skip-analysis-in-path -- $(MAKE) VARIANT=infer
endif

#----------------------------------------------------------------------------------------------

coverage:
	$(SHOW)$(MAKE) build COV=1
	$(SHOW)$(COVERAGE_RESET)
	$(SHOW)$(MAKE) test COV=1
	$(SHOW)$(COVERAGE_COLLECT_REPORT)

.PHONY: coverage

#----------------------------------------------------------------------------------------------

docker:
	$(SHOW)$(MAKE) -C build/docker
ifeq ($(PUBLISH),1)
	$(SHOW)make -C build/docker publish
endif

.PHONY: docker

#----------------------------------------------------------------------------------------------
