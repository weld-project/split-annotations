#ifndef _NASHVILLE_COMPOSER_H_
#define _NASHVILLE_COMPOSER_H_

#include <MagickWand.h>

MagickWand *colortone_composer(MagickWand *input_wand, const char *color, const char *compose_opt, int negate);

#endif
