EXAMPLEDIRS=$(wildcard examples/*)
EXAMPLES=$(subst examples/,,$(EXAMPLEDIRS))
EXAMPLEBINS=$(foreach example,$(EXAMPLES),examples/$(example)/$(example))

unittest:
	mkdir -p bin
	$(CXX) -O3 -Wall -I superglue/ test/main.cpp -pthread -o bin/$@
	./bin/unittest

examples: $(EXAMPLEDIRS)

examples/%:
	( cd $@ ; make )

$(EXAMPLEDIRS):
	$(MAKE) -C $@

tools:
	( cd tools ; make )

clean:
	rm -f ./bin/* $(EXAMPLEBINS)

.PHONY: unittest examples tools $(EXAMPLEDIRS)
