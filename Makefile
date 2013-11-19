FLAGS=-O3 -pedantic -Wall -Wno-long-long -Wconversion -I superglue/ -pthread

tests: unittest modular

unittest:
	mkdir -p bin
	$(CXX) $(FLAGS) test/main.cpp -o bin/$@
	./bin/$@

modular:
	( cd test/modular ; make )

examples:
	( cd examples ; make )

tools:
	( cd tools ; make )

csuperglue:
	$(CXX) $(FLAGS) -I csuperglue/ csuperglue/csuperglue.cpp -c -o bin/csuperglue.o
	$(CXX) $(FLAGS) -DSG_LOGGING -I csuperglue/ csuperglue/csuperglue.cpp -c -o bin/csupergluelog.o

csupergluetest:
	( cd test/csuperglue ; make )

clean:
	rm -f ./bin/* ./examples/bin/*

.PHONY: tests unittest examples tools csuperglue clean
