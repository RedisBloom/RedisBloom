all: test test-cuckoo test-perf cover-murmur

PYTHON:=python

test: test-basic test-cuckoo cover-murmur
	./test-basic
	./test-cuckoo
	./cover-murmur
	$(PYTHON) pytests.py
	$(PYTHON) cuckoo.py
	$(PYTHON) cms.py
	$(PYTHON) topk.py
	$(PYTHON) init_test.py

perf: test-perf
	time ./test-perf

CPPFLAGS:=$(CPPFLAGS) -I$(ROOT)/src
CFLAGS:=$(CFLAGS)

test-basic: test-basic.o $(DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test-cuckoo: test-cuckoo.o $(DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

cover-murmur: cover-murmur.o $(DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test-perf: test-perf.o $(DEPS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build-test: test-basic test-perf test-cuckoo cover-murmur

clean:
	$(RM) test-basic *.o *.rdb *.pyc
	$(RM) -fr *.dSYM
