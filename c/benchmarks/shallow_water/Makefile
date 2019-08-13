
# Libraries
LD_LIBRARY_PATH=-L../../target/release -L../../lib/composer_mkl

OS=$(shell uname -s)

ifeq ($(OS), Darwin)
CC=gcc-7
LDFLAGS=
INTEL= -DMKL_ILP64 -m64 -I${MKLROOT}/include -L${MKLROOT}/lib -Wl,-rpath,${MKLROOT}/lib -lmkl_intel_ilp64 -lmkl_intel_thread -lmkl_core -liomp5 -lpthread -lm -ldl
else ifeq ($(OS), Linux)
CC=gcc-5
LDFLAGS=-Wl,-rpath-link -Wno-format
INTEL= -DMKL_ILP64 -m64 -I${MKLROOT}/include -L${MKLROOT}/lib/intel64 -Wl,--no-as-needed -lmkl_intel_ilp64 -lmkl_intel_thread -lmkl_core -liomp5 -lpthread -lm -ldl
else
$(error Unsupported platform: $(OS))
endif

CFLAGS=-O3 -Wall -pedantic
INCLUDE=-I../../composer/ -I../../lib/composer_mkl
EXEC=bench

.PHONY: all clean

all:
	$(CC) -fopenmp $(CFLAGS) $(INCLUDE) $(LD_LIBRARY_PATH) shallow_water_mkl.c shallow_water_composer.c shallow_water.c -o $(EXEC) -lcomposer -lcomposer_mkl -lm $(INTEL)
	
asm:
	$(CC) $(CFLAGS) $(INCLUDE) $(LD_LIBRARY_PATH) -fopenmp shallow_water_mkl.c shallow_water_composer.c shallow_water.c -S

clean:
	rm -rf *.o *.s $(EXEC) $(VLIB)
