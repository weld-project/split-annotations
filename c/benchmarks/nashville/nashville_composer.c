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

#include "nashville_composer.h"
#include "nashville.h"

#include <splitters.h>
#include <composer.h>
#include <generated/generated.h>

// Actual work for the colortone function.
void c_do_colortone(MagickWand *wand,
    const char *color,
    const char *compose_opt,
    int negate,
    MagickWand *colorized_wand,
    MagickWand *colorspace_wand) {

    // Colorize image. 
    PixelWand *colorize = c_NewPixelWand();
    PixelWand *alpha = c_NewPixelWand();
    c_PixelSetColor(colorize, color);
    c_PixelSetColor(alpha, "#fff");
    c_MagickColorizeImage(colorized_wand, colorize, alpha);

    // Convert to grayspace.
    c_MagickSetImageColorspace(colorspace_wand, GRAYColorspace);
    if (negate) {
      c_MagickNegateImage(colorspace_wand, 1);
    }

    c_MagickSetImageArtifact(wand, "compose:args", compose_opt);
    c_MagickCompositeImage(wand, colorspace_wand, BlendCompositeOp, 1, 0, 0);
    c_MagickCompositeImage(wand, colorized_wand, BlendCompositeOp, 1, 0, 0);

    // Cleanup.
    colorize = c_DestroyPixelWand(colorize);
    alpha = c_DestroyPixelWand(alpha);
}

MagickWand *colortone_composer(MagickWand *input_wand,
    const char *color,
    const char *compose_opt,
    int negate) {

  MagickWand *wand = c_CloneMagickWand(input_wand);
  MagickWand *colorized_wand = c_CloneMagickWand(wand);
  MagickWand *colorspace_wand = c_CloneMagickWand(wand);

  c_do_colortone(wand, color, compose_opt, negate, colorized_wand, colorspace_wand);
  c_do_colortone(wand, color, compose_opt, negate, colorized_wand, colorspace_wand);
  
  // WriteImage (wand, colorized_wand, colorspace_wand, before execution: need
  // the output split type of each variable. If the output split type is
  // broadcast, we can just return one of the partitions instead of the result
  // of a merge. TODO Justify the logic behind doing this (or why it makes
  // sense to define API in this way).

  c_MagickModulateImage(wand, HUE, SATURATION, VALUE);
  c_MagickGammaImage(wand, GAMMA);

  colorized_wand = c_DestroyMagickWand(colorized_wand);
  colorspace_wand = c_DestroyMagickWand(colorspace_wand);

  // TODO we can do this automatically by adding a "mut"
  composer_emit(&wand, sizeof(wand), (intptr_t)WandSplit_merge);

  composer_execute();
  return wand;
}
