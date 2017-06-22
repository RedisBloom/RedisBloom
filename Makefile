ROOT=$(shell pwd)
# Flags for preprocessor
CPPFLAGS=-Wall

#Flags for compiler
CFLAGS=-g -fPIC -O3 -I$(ROOT) -I$(ROOT)/contrib


DEPS=contrib/MurmurHash2.o

all: rebloom.so

rebloom.so: src/rebloom.o $(DEPS)
	$(CC) $^ -o$@ -shared $(LDFLAGS)

clean:
	$(RM) rebloom.so $(DEPS)
