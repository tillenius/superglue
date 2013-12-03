FLAGS=-O3 -pedantic -Wall -Wno-long-long -I ../src -pthread

C_FILES=$(wildcard *.c)
TESTS=$(patsubst %.c,%.test,$(C_FILES))

CSUPERGLUE_LIB=../bin/csupergluelog.o

all: $(TESTS)

%.test: %.c
	$(CC) $(FLAGS) $< $(CSUPERGLUE_LIB) -lstdc++ && ./a.out

