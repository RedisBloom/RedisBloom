
ROOT=../..

include $(ROOT)/deps/readies/mk/main

#----------------------------------------------------------------------------------------------

MK_CUSTOM_CLEAN=1

ifeq ($(RMUTIL_BINDIR),)
override BINDIR=$(RMUTIL_BINDIR)
else
BINDIR=$(BINROOT)/rmutil
endif

SRCDIR=$(ROOT)/deps/RedisModulesSDK/rmutil

#----------------------------------------------------------------------------------------------

define CC_INCLUDES +=
	$(SRCDIR)/..
endef

define CC_DEFS +=
	REDIS_MODULE_TARGET
endef

_SOURCES=\
	util.c \
	strings.c \
	vector.c \
	alloc.c \
	periodic.c

SOURCES=$(addprefix $(SRCDIR)/,$(_SOURCES))
OBJECTS=$(patsubst $(SRCDIR)/%.c,$(BINDIR)/%.o,$(SOURCES))

CC_DEPS = $(patsubst $(SRCDIR)/%.c, $(BINDIR)/%.d, $(SOURCES))

TARGET=$(BINDIR)/rmutil.a

include $(MK)/defs

#----------------------------------------------------------------------------------------------

.PHONY: all clean test_vector test_periodic test

all: bindirs $(TARGET)

include $(MK)/rules

-include $(CC_DEPS)

$(BINDIR)/%.o: $(SRCDIR)/%.c
	@echo Compiling $<...
	$(SHOW)$(CC) $(CC_FLAGS) $(CC_C_FLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	@echo Creating $@...
	$(SHOW)ar rcs $@ $^

clean:
	-$(SHOW)[[ -e $(BINDIR) ]] && find $(BINDIR) -name '*.[oadh]' -type f -delete

#----------------------------------------------------------------------------------------------

$(BINDIR)/test_vector: test_vector.o vector.o
	$(SHOW)$(CC) -Wall -o $@ $^ -lc -lpthread -O0
	$(SHOW)sh -c ./$@

$(BINDIR)/test_periodic: test_periodic.o periodic.o
	$(SHOW)$(CC) -Wall -o $@ $^ -lc -lpthread -O0
	$(SHOW)sh -c ./$@
	
test: test_periodic test_vector
