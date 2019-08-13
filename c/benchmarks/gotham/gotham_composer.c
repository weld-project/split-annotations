#include <stdlib.h>

#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <stdio.h>

#include <assert.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/time.h>

#include <MagickWand.h>

#include <omp.h>

#include "gotham_composer.h"
#include "gotham.h"

#include <splitters.h>
#include <composer.h>
#include <generated/generated.h>

MagickWand *c_do_gotham(MagickWand *input_wand) {
  // modulate 120,10,100
  c_MagickModulateImage(input_wand, HUE, SATURATION, VALUE);

  // colorize
  PixelWand *colorize = c_NewPixelWand();
  PixelWand *alpha = c_NewPixelWand();
	c_PixelSetColor(colorize,"#222b6d");
	c_PixelSetColor(alpha, "rgb(20%,20%,20%)");
	c_MagickColorizeImage(input_wand, colorize, alpha);
	c_MagickColorizeImage(input_wand, colorize, alpha);
	c_MagickColorizeImage(input_wand, colorize, alpha);
	c_MagickColorizeImage(input_wand, colorize, alpha);
	c_MagickColorizeImage(input_wand, colorize, alpha);

  // gamma 0.5
  c_MagickGammaImage(input_wand, GAMMA);

  c_DestroyPixelWand(alpha);
  c_DestroyPixelWand(colorize);

  // contrast
  c_MagickContrastImage(input_wand, 1);
  // contrast
  c_MagickContrastImage(input_wand, 1);
}

MagickWand *gotham_composer(MagickWand *input_wand) {

  c_do_gotham(input_wand);

  // TODO we can do this automatically by adding a "mut"
  composer_emit(&input_wand, sizeof(input_wand), (intptr_t)WandSplit_merge);
  composer_execute();

  return input_wand;
}
