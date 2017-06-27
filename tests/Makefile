all: test test-perf

test: test-basic
	./test-basic

perf: test-perf
	time ./test-perf

CPPFLAGS:=$(CPPFLAGS) -I$(ROOT)/src
CFLAGS:=$(CFLAGS)

test-basic: test-basic.o $(DEPS) $(MODULE_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test-perf: test-perf.o $(DEPS) $(MODULE_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) test-basic *.o *.rdb *.pyc
	$(RM) -fr *.dSYM
