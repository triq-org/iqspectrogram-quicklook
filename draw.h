#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>
#include <sys/types.h>

/* A coloured pixel. */

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} pixel_t;

/* A picture. */

typedef struct
{
    pixel_t *pixels;
    size_t width;
    size_t height;
} bitmap_t;

/* Given "bitmap", this returns the pixel of bitmap at the point 
   ("x", "y"). */

inline pixel_t *pixel_at(bitmap_t *bitmap, int x, int y) {
    return bitmap->pixels + bitmap->width * y + x;
}

inline void pixel_set(bitmap_t *bitmap, int x, int y, int c) {
    pixel_t *pixel = pixel_at(bitmap, x, y);
    pixel->red = c;
    pixel->green = c;
    pixel->blue = c;
}

inline void pixel_color(bitmap_t *bitmap, int x, int y, uint8_t *c) {
    pixel_t *pixel = pixel_at(bitmap, x, y);
    pixel->red = c[0];
    pixel->green = c[1];
    pixel->blue = c[2];
}

inline void pixel_cmap(bitmap_t *bitmap, int x, int y, uint8_t *cmap, int c) {
    pixel_color(bitmap, x, y, &cmap[c * 3]);
}

// lines

inline void fill_rect(bitmap_t *bitmap, int left, int top, int width, int height, int c) {
    for (int y = top; y < top + height; ++y) {
        for (int x = left; x < left + width; ++x) {
            pixel_set(bitmap, x, y, c);
        }
    }
}

#endif // DRAW_H
