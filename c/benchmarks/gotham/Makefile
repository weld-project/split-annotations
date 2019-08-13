
# Libraries
LD_LIBRARY_PATH=-L../../target/release -L../../lib/ImageMagick/

OS=$(shell uname -s)

ifeq ($(OS), Darwin)
CC=gcc-7
CPP=g++-7
LDFLAGS=
MAGICK=$(shell pkg-config --cflags --libs MagickWand)
else ifeq ($(OS), Linux)
CC=gcc-5
CPP=g++-5
LDFLAGS=-Wl,-rpath-link -Wno-format
MAGICK=$(shell pkg-config --cflags --libs MagickWand)
else
$(error Unsupported platform: $(OS))
endif

CFLAGS=-O3 -Wall -pedantic -Wno-discarded-qualifiers
INCLUDE=-I../../composer/ -I/usr/local/include/ImageMagick-7/MagickWand -I../../lib/ImageMagick
EXEC=bench

.PHONY: all clean

all:
	$(CC) -fopenmp $(CFLAGS) $(INCLUDE) $(LD_LIBRARY_PATH) gotham.c gotham_composer.c -o $(EXEC) $(MAGICK) -lcomposer -lcomposer_imagemagick -lm

clean:
	rm -rf *.o $(EXEC)
