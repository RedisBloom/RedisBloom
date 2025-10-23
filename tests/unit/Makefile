
ROOT=../..

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

.PHONY: unit-tests
TARGET=unit-tests

MODULE_SO=$(BINROOT)/redisbloom.so

#----------------------------------------------------------------------------------------------

MK_CUSTOM_CLEAN=1

CC_C_STD=gnu99

include $(MK)/defs

_SOURCES=\
	test-basic.c \
	test-cuckoo.c \
	test-perf.c

SOURCES=$(addprefix $(SRCDIR)/,$(_SOURCES))
OBJECTS=$(patsubst $(SRCDIR)/%.c,$(BINDIR)/%.o,$(SOURCES))

CC_DEPS = $(patsubst $(SRCDIR)/%.c, $(BINDIR)/%.d, $(SOURCES))

define CC_DEFS +=
endef

define CC_INCLUDES +=
	$(SRCDIR)
	$(BINDIR)
	$(ROOT)/deps
	$(ROOT)/deps/RedisModulesSDK
	$(ROOT)/src
endef

LD_LIBS += $(MODULE_SO)

#----------------------------------------------------------------------------------------------

include $(MK)/rules

ifeq ($(OS),macos)
CC_FLAGS += -fblocks
endif

ifneq ($(SAN),)
CC_FLAGS += -fblocks
LD_LIBS += -lBlocksRuntime
endif

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
