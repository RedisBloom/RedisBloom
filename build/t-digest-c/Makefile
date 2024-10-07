
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

# CMAKE_FLAGS will be used to compile tdigest using cmake, notice that we only want to compile the static library
# and we compile it with costume allocator that allocate data using the RedisModule Allocator.
# If we will try to compile the tests with this option it will failed to compile but because we do
# not really run the tdigest tests here there is no need to compile them.
CMAKE_FLAGS =-DBUILD_SHARED=OFF -DBUILD_STATIC=ON -DENABLE_CODECOVERAGE=OFF -DBUILD_TESTS=OFF -DBUILD_BENCHMARK=OFF -DBUILD_EXAMPLES=OFF -DCMAKE_C_FLAGS=-DTD_MALLOC_INCLUDE=\\\"../../../src/td_redismodule_malloc.h\\\"

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
