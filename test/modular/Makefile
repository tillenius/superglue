FLAGS=-O3 -pedantic -Wall -Wno-long-long -I ../../superglue -pthread

CPP_FILES=$(wildcard *.cpp)
TESTS=$(patsubst %.cpp,%.test,$(CPP_FILES))

all: $(TESTS)

%.test: %.cpp
	$(CXX) $(FLAGS) $< && ./a.out
