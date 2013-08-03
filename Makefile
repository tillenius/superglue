FLAGS=-O3 -pedantic -Wall -Wno-long-long -I superglue/ -pthread

unittest:
	mkdir -p bin
	$(CXX) $(FLAGS) test/main.cpp -o bin/$@
	./bin/$@

examples:
	( cd examples ; make )

tools:
	( cd tools ; make )

clean:
	rm -f ./bin/* ./examples/bin/*

.PHONY: unittest examples tools clean
