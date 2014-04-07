FLAGS=-O3 -pedantic -Wall -Wno-long-long -Wconversion -I ../include -pthread

all: csuperglue csupergluetest

bin:
	mkdir -p bin

csuperglue: bin
	$(CXX) $(FLAGS) -I src/ src/csuperglue.cpp -c -o bin/csuperglue.o
	$(CXX) $(FLAGS) -DSG_LOGGING -I src/ src/csuperglue.cpp -c -o bin/csupergluelog.o

csupergluetest:
	( cd test ; make )

clean:
	rm -f ./bin/*

.PHONY: tests unittest examples tools csuperglue clean
