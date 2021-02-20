all: test 

PYTHON:=python

flow_tests: 
	RLTest --module ../../redisbloom.so

test: flow_tests

clean:
	$(RM) *.rdb *.pyc
	$(RM) -fr *.dSYM
