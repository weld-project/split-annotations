#ifndef _NASHVILLE_PARALLEL_H_
#define _NASHVILLE_PARALLEL_H_

#include <MagickWand.h>

MagickWand *colortone_parallel(MagickWand *input_wand, const char *color, const char *compose_opt, int negate, int threads);

#endif
