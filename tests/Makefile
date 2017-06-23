all: test

test: test-basic
	./test-basic

CPPFLAGS:=$(CPPFLAGS) -I$(ROOT)/src
CFLAGS:=$(CFLAGS)

test-basic: test-basic.o $(DEPS) $(MODULE_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) test-basic *.o *.rdb *.pyc
