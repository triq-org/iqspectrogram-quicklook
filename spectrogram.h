/// @file spectrogram.h
/// SDR I/Q data Spectrogram QuickLook Plugin.
///
/// @author Christian W. Zuckschwerdt on 2017-05-23.
/// @copyright 2017 Christian W. Zuckschwerdt. All rights reserved.
///

#ifndef SPECTROGRAM_H
#define SPECTROGRAM_H

#include <stdint.h>
#include "draw.h"

typedef struct
{
    int width;
    int height;
    int top;
    int left;
    uint8_t *cmap;
    int color_max;

    off_t size;
    uint8_t *data;
    int bytes_per_sample;

    int center_freq;
    int sample_rate;
    int decimation;
    float *window;

    float gain;
    float db_range;

    int a_y;
    int a_height;

    // output
    int samples;
    int points;
    float stride;
} spectrogram_t;

void draw_spectrogram(const char *fname, bitmap_t *bitmap, int width, int height, int top, int left);

#endif // SPECTROGRAM_H
