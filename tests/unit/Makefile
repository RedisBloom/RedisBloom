all: test test-basic test-cuckoo test-perf

test: test-basic test-cuckoo
	./test-basic
	./test-cuckoo

perf: test-perf
	time ./test-perf

CPPFLAGS:=$(CPPFLAGS)
CFLAGS:=$(CFLAGS)

test-basic: test-basic.o $(DEPS_TEST)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test-cuckoo: test-cuckoo.o $(DEPS_TEST)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test-perf: test-perf.o $(DEPS_TEST)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) test-basic test-perf test-cuckoo 
	$(RM) *.o
	$(RM) -fr *.dSYM
