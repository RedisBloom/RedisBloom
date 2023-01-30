
ROOT=../..

MK.cmake=1

include $(ROOT)/deps/readies/mk/main

#----------------------------------------------------------------------------------------------

define HELPTEXT
make build          # compile and link
  DEBUG=1             # build for debugging
  WHY=1               # explain CMake decisions (in /tmp/cmake-why)
  FORCE=1             # Force CMake rerun (default)
  CMAKE_ARGS=...      # extra arguments to CMake
  SLOW=1              # do not parallelize build (for diagnostics)
make clean         # remove build artifacts
  ALL=1              # remove entire artifacts directory

endef

#----------------------------------------------------------------------------------------------

SRCDIR=$(ROOT)/deps/t-digest-c

BINDIR=$(BINROOT)/t-digest-c

TARGET=$(BINDIR)/libtdigest_static.a

#----------------------------------------------------------------------------------------------

include $(MK)/defs

define CMAKE_DEFS +=
	BUILD_SHARED=OFF
	BUILD_STATIC=ON
	ENABLE_CODECOVERAGE=OFF
	BUILD_TESTS=OFF
	BUILD_BENCHMARK=OFF
	BUILD_EXAMPLES=OFF
endef

#----------------------------------------------------------------------------------------------

MK_CUSTOM_CLEAN=1

include $(MK)/rules

#----------------------------------------------------------------------------------------------

clean:
ifeq ($(ALL),1)
	$(SHOW)rm -rf $(BINDIR)
else
	$(SHOW)$(MAKE) -C $(BINDIR) clean
endif

.PHONY: clean
