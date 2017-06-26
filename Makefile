ROOT=$(shell pwd)
# Flags for preprocessor
CPPFLAGS=-Wall -std=c99
LDFLAGS=-lm

#Flags for compiler
CFLAGS=-g -fPIC -O3
CPPFLAGS+=-I$(ROOT) -I$(ROOT)/contrib

MODULE_OBJ=$(ROOT)/src/rebloom.o
MODULE_SO=$(ROOT)/rebloom.so

DEPS=$(ROOT)/contrib/MurmurHash2.o

export

all: $(MODULE_SO)

$(MODULE_SO): $(MODULE_OBJ) $(DEPS)
	$(CC) $^ -o$@ -shared $(LDFLAGS)


test:
	$(MAKE) -C tests test

clean:
	$(RM) $(MODULE_OBJ) $(MODULE_SO) $(DEPS)
	$(MAKE) -C tests clean
