
# Libraries
LD_LIBRARY_PATH=-L../../target/release -L../../lib/composer_mkl

OS=$(shell uname -s)

ifeq ($(OS), Darwin)
CC=gcc-7
LDFLAGS=
INTEL= -L${MKLROOT}/lib/intel64 -Wl,--no-as-needed -lmkl_intel_ilp64 -lmkl_intel_thread -lmkl_core -liomp5 -lpthread -lm -ldl
else ifeq ($(OS), Linux)
CC=gcc-5
LDFLAGS=-Wl,-rpath-link -Wno-format
INTEL= -L${MKLROOT}/lib/intel64 -Wl,--no-as-needed -lmkl_intel_ilp64 -lmkl_intel_thread -lmkl_core -liomp5 -lpthread -lm -ldl
else
$(error Unsupported platform: $(OS))
endif

CFLAGS=-O3 -Wall -pedantic
INCLUDE=-I../../composer/ -I../../lib/composer_mkl
EXEC=bench

.PHONY: all clean

all:
	$(CC) -fopenmp $(CFLAGS) $(INCLUDE) $(LD_LIBRARY_PATH) nbody.c nbody_mkl.c nbody_composer.c -o $(EXEC) -lcomposer -lcomposer_mkl -lm $(INTEL)

asm:
	$(CC) $(CFLAGS) $(INCLUDE) $(LD_LIBRARY_PATH) -fopenmp nbody.c -S

clean:
	rm -rf *.o *.s $(EXEC) $(VLIB)
