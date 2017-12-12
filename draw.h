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

inline void draw_hline(bitmap_t *bitmap, int x1, int y1, int x2, int c) {
    for (int x = x1; x <= x2; ++x) {
        pixel_set(bitmap, x, y1, c);
    }
}

inline void draw_vline(bitmap_t *bitmap, int x1, int y1, int y2, int c) {
    for (int y = y1; y <= y2; ++y) {
        pixel_set(bitmap, x1, y, c);
    }
}

inline void draw_line(bitmap_t *bitmap, int x1, int y1, int x2, int y2, int c) {
    if (x1 == x2)
        draw_vline(bitmap, x1, y1, y2, c);
    else if (y1 == y2)
        draw_hline(bitmap, x1, y1, x2, c);
}

#endif // DRAW_H
