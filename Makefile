all: rebloom.so

CPPFLAGS=-Wall -fPIC
CFLAGS=-g

DEPS=deps/MurmurHash2.o

rebloom.so: rebloom.o $(DEPS)
	$(CC) $^ -o$@ -shared $(LDFLAGS)

clean:
	$(RM) rebloom.so
