all: test test-cuckoo test-perf

test: test-basic test-cuckoo
	./test-basic
	./test-cuckoo
	python pytests.py

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
	$(RM) test-basic *.o *.rdb *.pyc
	$(RM) -fr *.dSYM
