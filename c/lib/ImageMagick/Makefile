CFLAGS=-fPIC -g -O3 -Wall
LDFLAGS=-shared
LD_LIBRARY_PATH=../../target/release

OS=$(shell uname -s)

ifeq ($(OS), Darwin)
CC=gcc-7
LIB=dylib
MAGICK=$(shell pkg-config --cflags --libs MagickWand)
else ifeq ($(OS), Linux)
CC=gcc-5
LIB=so
MAGICK=$(shell pkg-config --cflags --libs MagickWand)
else
	$(error Unsupported platform: $(OS))
endif

INCLUDE=-I../../composer/ -I/usr/local/include/ImageMagick-7/MagickWand

.PHONY: all annotate clean

all: annotate
	$(CC) $(INCLUDE) $(MAGICK) -fopenmp -L$(LD_LIBRARY_PATH) $(CFLAGS) $(LDFLAGS) splitters.c -o libcomposer_imagemagick.$(LIB) -lcomposer $(INTEL)

annotate:
	rm -rf generated
	annotate -i MagickWand imagemagick.annotation

clean:
	rm -rf a.out generated libcomposer_imagemagick.$(LIB) *.dSYM
