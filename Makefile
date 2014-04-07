FLAGS=-O3 -pedantic -Wall -Wno-long-long -Wconversion -I include/ -pthread

tests: unittest modular csuperglue

unittest:
	mkdir -p bin
	$(CXX) $(FLAGS) test/main.cpp -o bin/$@
	./bin/$@

modular:
	( cd test/modular ; make )
	( cd test/fail ; make )

examples:
	( cd examples ; make )

tools:
	( cd tools ; make )

csuperglue:
	( cd csuperglue ; make )

clean:
	rm -f ./bin/* ./examples/bin/*

.PHONY: tests unittest examples tools csuperglue clean
