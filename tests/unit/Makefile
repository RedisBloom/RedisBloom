all: test test-basic test-cuckoo test-perf

test: test-basic test-cuckoo
	./test-basic
	./test-cuckoo

perf: test-perf
	time ./test-perf

CPPFLAGS:=$(CPPFLAGS) -I$(ROOT)/src
CFLAGS:=$(CFLAGS)

test-basic: test-basic.o $(DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test-cuckoo: test-cuckoo.o $(DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test-perf: test-perf.o $(DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build-test: test-basic test-perf test-cuckoo

clean:
	$(RM) test-basic test-perf test-cuckoo 
	$(RM) *.o
	$(RM) -fr *.dSYM
