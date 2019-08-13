#ifndef _NASHVILLE_H_
#define _NASHVILLE_H_

#define GAMMA       (0.7)
#define HUE         (100)
#define SATURATION   (150)
#define VALUE       (100)

// Actual work for the colortone function, without Composer.
void do_colortone(MagickWand *wand,
    const char *color,
    const char *compose_opt,
    int negate,
    MagickWand *colorized_wand,
    MagickWand *colorspace_wand);

#endif
