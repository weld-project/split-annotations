CFLAGS=-fPIC -g -O3 -Wall
LDFLAGS=-shared
INCLUDE=-I../../composer -I.
LD_LIBRARY_PATH=../../target/release

OS=$(shell uname -s)

ifeq ($(OS), Darwin)
	CC=gcc-7
	LIB=dylib
	INTEL= -L${MKLROOT}/lib -Wl,-rpath,${MKLROOT}/lib -lmkl_intel_ilp64 -lmkl_intel_thread -lmkl_core -liomp5 -lpthread -lm -ldl
else ifeq ($(OS), Linux)
	CC=gcc-5
	LIB=so
	INTEL= -I${MKLROOT}/compilers_and_libraries/linux/mkl/include -L${MKLROOT}/compilers_and_libraries/linux/mkl/lib/intel64 -L${MKLROOT}/lib/intel64 -Wl,--no-as-needed -lmkl_intel_ilp64 -lmkl_intel_thread -lmkl_core -liomp5 -lpthread -lm -ldl
else
	$(error Unsupported platform: $(OS))
endif


.PHONY: all annotate clean

all: annotate
	$(CC) $(INCLUDE) -L$(LD_LIBRARY_PATH) $(CFLAGS) $(LDFLAGS) vec.c splitters.c mkl_extensions.c -o libcomposer_mkl.$(LIB) -lcomposer $(INTEL)

annotate:
	rm -rf generated
	../../target/release/annotate -i mkl,mkl_extensions mkl.annotation

clean:
	rm -rf a.out generated libcomposer_mkl.$(LIB) *.dSYM
