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


    int draw_color_histogram;
    int color_histogram_top;  // height + top
    int color_histogram_left; // width + left + 40

    int draw_db_histogram;
    int db_histogram_top;  // top
    int db_histogram_left; // width + left

    int draw_box;
    int box_color; // 127

    int draw_db_ramp;
    int db_ramp_top;  // top
    int db_ramp_left; // width + left + 8

    int draw_db_marker; // dbFS marker for about every 70 pixels
    int db_marker_color;  // 191
    int db_marker_top;  // top
    int db_marker_left; // width + left + 24

    int draw_freq_marker;  // freq marker for every 16 pixels
    int freq_marker_color; // 127
    int freq_text_color; // 191
    int freq_marker_top;   // top
    int freq_marker_left;  // left -4 to left

    int draw_time_marker;  // time marker for about every 85 pixels
    int time_marker_color; // 127
    int time_text_color; // 191
    int time_marker_top;   // top + height to top + height + 4
    int time_marker_left;  // left

    int draw_title;
    int title_color; // 255
    int title_top;   // 1
    int title_left;  // left + width/2 - 40


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
