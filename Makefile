FLAGS=-O3 -pedantic -Wall -Wno-long-long -I superglue/ -pthread

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

clean:
	rm -f ./bin/* ./examples/bin/*

.PHONY: tests unittest examples tools clean
