all: test 

test: unit_tests flow_tests

flow_tests: 
	make -C flow

unit_tests: 
	make -C unit

clean:
	make -C unit clean
	make -C flow clean
