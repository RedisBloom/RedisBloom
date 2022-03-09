
ifeq ($(VG),1)
override VALGRIND:=1
export VALGRIND
endif

ifeq ($(VALGRIND),1)
override DEBUG:=1
export DEBUG
endif

ROOT=../..
MK.pyver:=3
include $(ROOT)/deps/readies/mk/main

define HELPTEXT
make build    # configure and compile
make clean    # clean generated sbinaries
  ALL=1       # remote entire binary directory
endef

MK_ALL_TARGETS=build

#----------------------------------------------------------------------------------------------

BINDIR=$(BINROOT)/unit_tests
SRCDIR=$(ROOT)/tests/unit

.PHONY: unit_tests
TARGET=unit_tests

MODULE_SO=$(BINROOT)/redisbloom.so

#----------------------------------------------------------------------------------------------

MK_CUSTOM_CLEAN=1

include $(MK)/defs

CC=gcc

_SOURCES=\
	test-basic.c \
	test-cuckoo.c \
	test-perf.c

SOURCES=$(addprefix $(SRCDIR)/,$(_SOURCES))
OBJECTS=$(patsubst $(SRCDIR)/%.c,$(BINDIR)/%.o,$(SOURCES))

CC_DEPS = $(patsubst $(SRCDIR)/%.c, $(BINDIR)/%.d, $(SOURCES))

CC_FLAGS += \
	-fPIC \
	-std=gnu99 \
	-MMD -MF $(@:.o=.d) \
	-I$(SRCDIR) \
	-I$(BINDIR) \
	-I$(ROOT)/deps \
	-I$(ROOT)/src

ifeq ($(DEBUG),1)
CC_FLAGS += -g -O0
LD_FLAGS += -g
else
CC_FLAGS += -O2 -Wno-unused-result
endif

ifeq ($(OS),macos)
LD_FLAGS += -undefined dynamic_lookup
LD_sHARED_EXT=dylib
else
LD_sHARED_EXT=so
endif

LD_FLAGS += $(LD_FLAGS.coverage)

MODULE_SO=$(realpath $(BINROOT)/redisbloom.$(LD_sHARED_EXT))

LD_LIBS += $(MODULE_SO)

#----------------------------------------------------------------------------------------------

include $(MK)/rules

-include $(CC_DEPS)

$(BINDIR)/%.o: $(SRCDIR)/%.c
	@echo Compiling $<...
	$(SHOW)$(CC) $(CC_FLAGS) -fvisibility=hidden -c $< -o $@

TARGETS=$(addprefix $(BINDIR)/test-,basic cuckoo perf)

$(TARGET): $(TARGETS) 

$(BINDIR)/test-basic: $(BINDIR)/test-basic.o
	@echo Creating $@...
	$(SHOW)$(CC) $(LD_FLAGS) -o $@ $< $(LD_LIBS)

$(BINDIR)/test-cuckoo: $(BINDIR)/test-cuckoo.o
	@echo Creating $@...
	$(SHOW)$(CC) $(LD_FLAGS) -o $@ $< $(LD_LIBS)

$(BINDIR)/test-perf: $(BINDIR)/test-perf.o
	@echo Creating $@...
	$(SHOW)$(CC) $(LD_FLAGS) -o $@ $< $(LD_LIBS)

clean:
ifeq ($(ALL),1)
	$(SHOW)rm -rf $(BINDIR) $(TARGETS)
else
	-$(SHOW)find $(BINDIR) -name '*.[oadh]' -type f -delete
	$(SHOW)rm -f $(TARGETS)
endif

#----------------------------------------------------------------------------------------------

test:
	@$(BINDIR)/test-basic
	@$(BINDIR)/test-cuckoo

perf:
	@$(BINDIR)/test-perf

.PHONY: test pref
