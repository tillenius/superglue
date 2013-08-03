FLAGS=-O3 -pedantic -Wall -Wno-long-long -I superglue/

viewer: viewer.cpp
	$(CXX) $(FLAGS) viewer.cpp -lGL -lglut -o $@

clean:
	rm -f viewer
