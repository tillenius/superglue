EXAMPLES:=$(basename $(notdir $(wildcard src/*.cpp)))

FLAGS=-O3 -pedantic -Wall -Wno-long-long -I ../include -pthread

all: $(EXAMPLES)

$(EXAMPLES): bin/
	$(CXX) $(FLAGS) src/$@.cpp -o bin/$@

bin:
	mkdir -p bin

clean:
	rm -rf bin

.PHONY: all clean $(EXAMPLES)
