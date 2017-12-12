/// @file spectrogram.c
/// SDR I/Q data Spectrogram QuickLook Plugin.
///
/// @author Christian W. Zuckschwerdt on 2017-05-23.
/// @copyright 2017 Christian W. Zuckschwerdt. All rights reserved.
///

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>

#include <math.h>
#include <string.h>
//#include <complex.h>
#include <fftw3.h>

#include "font.h"
#include "cube1_cmap.h"
#include "draw.h"

#include "spectrogram.h"

extern inline pixel_t *pixel_at(bitmap_t *bitmap, int x, int y);
extern inline void pixel_set(bitmap_t *bitmap, int x, int y, int c);
extern inline void pixel_color(bitmap_t *bitmap, int x, int y, uint8_t *c);
extern inline void pixel_cmap(bitmap_t *bitmap, int x, int y, uint8_t *cmap, int c);
extern inline void draw_hline(bitmap_t *bitmap, int x1, int y1, int x2, int c);
extern inline void draw_vline(bitmap_t *bitmap, int x1, int y1, int y2, int c);
extern inline void draw_line(bitmap_t *bitmap, int x1, int y1, int x2, int y2, int c);

typedef struct
{
    float scale;
    char *prefix;
} autorange_t;

autorange_t autoranges[] = {
    {1e24, "Y"}, // yotta
    {1e21, "Z"}, // zetta
    {1e18, "E"}, // exa
    {1e15, "P"}, // peta
    {1e12, "T"}, // tera
    {1e9, "G"},  // giga
    {1e6, "M"},  // mega
    {1e3, "k"},  // kilo
    {1e0, ""},
    {1e-3, "m"},  // milli
    {1e-6, "u"},  // micro
    {1e-9, "n"},  // nano
    {1e-12, "p"}, // pico
    {1e-15, "f"}, // femto
    {1e-18, "a"}, // atto
    {1e-21, "z"}, // zepto
    {1e-24, "y"}, // yocto
};

static const int autoranges_length = sizeof(autoranges) / sizeof(autorange_t);

// Determine divisor and SI prefix.
static autorange_t autorange(float num, float min_int)
{
    if (!min_int) min_int = 10.0;
    num = num / min_int;
    for (int i = 0; i < autoranges_length; ++i)
    {
        if (num >= autoranges[i].scale)
                return autoranges[i];
    }
    return autoranges[autoranges_length - 1];
}

// ---

// font

#define draw_text_at(x, y, c, t) draw_text_(bitmap, x, y, c, t, 0)
#define draw_text_up(x, y, c, t) draw_text_(bitmap, x, y, c, t, 1)

static void draw_text_(bitmap_t *bitmap, int x, int y, int c, char const *text, int orientation) {
    for (; *text; ++text) {
        int pos = ((*text < ' ' || *text > '~' ? '~' + 1 : *text) - ' ') * font_y;
        for (int i = 0; i < font_y; ++i) {
            unsigned line = font[pos++];
            for (int j = 0; j < font_x; ++j, line <<= 1)
                if (line & 0x80)
                    switch (orientation) {
                        case 0:
                            pixel_set(bitmap, x + j, y + i, c);
                            break;
                        case 1:
                            pixel_set(bitmap, x + i, y + j, c);
                            break;
                    }
        }
        switch (orientation) {
            case 0:
                x += font_X;
                break;
            case 1:
                y += font_X;
                break;
        }
    }
}

// windows

typedef float (*window_func)(float *out, unsigned int num);

float rectangular_window(float *out, unsigned int num) {
    float window_sum = 0.0f;
    for (unsigned int i = 0; i < num; i++) {
        out[i] = 1.0f;
        window_sum += out[i];
    }
    return window_sum;
}

float bartlett_window(float *out, unsigned int num) {
    float window_sum = 0.0f;
    for (unsigned int i = 0; i < num; i++) {
        out[i] = 1.0f - fabsf((i - 0.5f * (num - 1)) / (0.5f * (num - 1)));
        window_sum += out[i];
    }
    return window_sum;
}

float hamming_window(float *out, unsigned int num) {
    const float a = 0.54f;
    const float b = 0.46f;

    float window_sum = 0.0f;
    for (unsigned int i = 0; i < num; i++) {
        out[i] = a - b * cosf(2.0f * M_PI * i / (num - 1));
        window_sum += out[i];
    }
    return window_sum;
}

float hann_window(float *out, unsigned int num) {
    float window_sum = 0.0f;
    for (unsigned int i = 0; i < num; i++) {
        out[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (num - 1)));
        window_sum += out[i];
    }
    return window_sum;
}

float blackman_window(float *out, unsigned int num) {
    const float a0 = 0.42f;
    const float a1 = 0.5f;
    const float a2 = 0.08f;

    float window_sum = 0.0f;
    for (unsigned int i = 0; i < num; ++i) {
        out[i] = a0 - (a1 * cosf((2.0f * M_PI * i) / (num - 1))) + (a2 * cosf((4.0f * M_PI * i) / (num - 1)));
        window_sum += out[i];
    }
    return window_sum;
}

float blackman_harris_window(float *out, unsigned int num) {
    const float a0 = 0.35875f;
    const float a1 = 0.48829f;
    const float a2 = 0.14128f;
    const float a3 = 0.01168f;

    float window_sum = 0.0f;
    for (unsigned int i = 0; i < num; ++i) {
        out[i] = a0 - (a1 * cosf((2.0f * M_PI * i) / (num - 1))) + (a2 * cosf((4.0f * M_PI * i) / (num - 1))) - (a3 * cosf((6.0f * M_PI * i) / (num - 1)));
        window_sum += out[i];
    }
    return window_sum;
}

float named_window(const char *window_name, float *out, unsigned int num) {
    if (!window_name)
        return 0.0f;
    else if (!strcasecmp(window_name, "rectangular"))
        return rectangular_window(out, num);
    else if (!strcasecmp(window_name, "bartlett"))
        return bartlett_window(out, num);
    else if (!strcasecmp(window_name, "hann"))
        return hann_window(out, num);
    else if (!strcasecmp(window_name, "hamming"))
        return hamming_window(out, num);
    else if (!strcasecmp(window_name, "blackman"))
        return blackman_window(out, num);
    else if (!strcasecmp(window_name, "blackman-harris"))
        return blackman_harris_window(out, num);
    else
        return 0.0f;
}

// http://www.fftw.org/fftw3_doc/Complex-One_002dDimensional-DFTs.html
// https://stackoverflow.com/questions/21283144/generating-correct-spectrogram-using-fftw-and-window-function
void draw_spectrogram(const char *fname, bitmap_t *bitmap, int width, int height, int top, int left)
{
    // read iq data
    FILE *fp = fopen(fname, "rb");

    int fd = fileno(fp);
    struct stat buf;
    fstat(fd, &buf);
    off_t size = buf.st_size;
    printf("size = %lld\n", size);

    uint8_t *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED)
        perror("mmap");

    //uint8_t *data = malloc(size);
    //uint8_t is[] = {127, 0, 127, 255};
    //uint8_t qs[] = {0, 127, 255, 127};
    //for (int i = 0; i < size / 2; ++i) {
    //    data[2 * i + 0] = is[i % 4];
    //    data[2 * i + 1] = qs[i % 4];
    //}
    //size_t r = fread(data, size, 1, fp);

    fclose(fp);

    //int s_min = 127;
    //int s_max = 127;
    //for (int x = 0; x < size; ++x) {
    //    if (data[x] < s_min) s_min = data[x];
    //    if (data[x] > s_max) s_max = data[x];
    //}
    //printf("min=%d, max=%d\n", s_min, s_max);

    // fftw setup
    int N = height;
    fftw_complex *in, *out;
    fftw_plan p;

    // params
    int center_freq = 433920000;
    int sample_rate = 250000;
    int decimation = 1;

    int color_max = 255;
    // cu8 has a possible range of 0 to -60 dBfs
    float gain = 6.0;
    float db_range = 30;
    uint8_t *cmap = cube1_cmap;
    float color_norm = color_max / -db_range;

    int a_y = top + height + font_y + 2;
    int a_height = 16;

    // stride setup
    int bytes_per_sample = 2;
    off_t samples = size / bytes_per_sample;
    int points = width;
    float stride = (samples - N) / (points - 1);
    printf("stride = %f\n", stride);

    // plan
    in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
    p = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    // Hann window
    float window[N];
    float window_sum = blackman_harris_window(window, N);

    //float block_norm = 1.0 / N;
    float block_norm = 1.0 / window_sum;
    float block_norm_db = 10 * log10(block_norm);
    printf("block_norm=%d, window_sum=%f\n", N, window_sum);

    // transform
    float dBfs_min = 0.0;
    float dBfs_max = -200.0;

    float s_bias = 127.5; // or 127? or 128?
    float s_scale = 1.0 / 127.5;

    int c_hist[color_max + 1];
    memset(c_hist, 0, (color_max + 1) * sizeof(int));
    int cB_hist_size = 1000; // centi Bell (0.1 dB)
    int cB_hist[cB_hist_size]; // -0.0 to -100.0 dB
    memset(cB_hist, 0, cB_hist_size * sizeof(int));

    for (int x = 0; x < width; ++x) {
        int j = x * stride;

        // copy cu8
        // -128 to +127 ?
        for (int k = 0; k < N; ++k) {
            in[k][0] = window[k] * (data[2 * (k + j)] - s_bias) * s_scale;
            in[k][1] = window[k] * (data[2 * (k + j) + 1] - s_bias) * s_scale;
        }

        // run
        fftw_execute(p); /* repeat as needed */

        for (int k = 0; k < N; ++k) {
            int y = k <= N / 2 ? N / 2 - k : N + N / 2 - k;

            float abs2 = out[k][0] * out[k][0] + out[k][1] * out[k][1];
            //float dBfs = 10 * log10(sqrt(abs2) * block_norm) + gain;
            float dBfs = 5 * log10(abs2) + block_norm_db + gain;

            if (dBfs < dBfs_min) dBfs_min = dBfs;
            if (dBfs > dBfs_max) dBfs_max = dBfs;
            int b = 0.5 + (dBfs - gain) * -10.0;
            if (b >= cB_hist_size) b = cB_hist_size - 1;
            cB_hist[b] += 1;

            // amplitude gauge
            int a_gauge = (db_range + dBfs) * 256 / db_range;
            if (a_gauge > 255) a_gauge = 255;
            if (a_gauge < 0) a_gauge = 0;
            draw_line(bitmap, x + left, a_y + a_height - a_gauge / 16, x + left, a_y + a_height, a_gauge);

            int p = 0.5 + dBfs * color_norm;
            if (p > color_max) p = color_max;
            if (p < 0) p = 0;
            p = color_max - p;
            c_hist[p] += 1;

            pixel_cmap(bitmap, x + left, y + top, cmap, p);
        }
    }
    printf("\nrange %f to %f\n", dBfs_min, dBfs_max);

    // end
    if (munmap(data, size) == -1)
        perror("munmap");
    //free(data);
    fftw_destroy_plan(p);
    fftw_free(in);
    fftw_free(out);

    // print color histogram
    int c_hist_max = 0;
    for (int i = 0; i <= color_max; ++i)
        if (c_hist[i] > c_hist_max) c_hist_max = c_hist[i];
    for (int y = 0; y < height; ++y) {
        int i = 0.5 + y * color_max / (height - 1);
        int h = 100.0 * c_hist[i] / c_hist_max;
        //int h1 = i == 0 ? 100.0 * c_hist[1] / c_hist_max : 100.0 * c_hist[i - 1] / c_hist_max;
        for (int x = 0; x < h; ++x) {
            pixel_t *pixel = pixel_at(bitmap, x + width + left + 40, height - y + top);
            //pixel->red = x > h - 2 - abs(h - h1) ? 255 : 63;
            pixel->red = x > h - 10 ? 55 + 20 * (10 - h + x) : 63;
            //pixel->green = x > h - 4 ? 255 : 63;
            //pixel->blue = x > h - 4 ? 255 : 63;
        }
    }

    // print dB histogram
    int cB_hist_max = 0;
    float cB_hist_mean = 0;
    for (int i = 0; i < cB_hist_size; ++i)
        if (cB_hist[i] > cB_hist_max) {
            cB_hist_max = cB_hist[i];
            cB_hist_mean += cB_hist[i] / cB_hist_size;
        }
    printf("cB_hist_mean = %f\n", cB_hist_mean);
    for (int y = 0; y < height; ++y) {
        int i = 0.5 + y * (cB_hist_size - 1) / (height - 1);
        int h = 100.0 * cB_hist[i] / cB_hist_max;
        //int h1 = i == 0 ? 100.0 * cB_hist[1] / cB_hist_max : 100.0 * cB_hist[i - 1] / cB_hist_max;
        for (int x = 0; x < h; ++x) {
            pixel_t *pixel = pixel_at(bitmap, x + width + left + 40, y + top);
            //pixel->red = x > h - 4 ? 255 : 63;
            //pixel->green = x > h - 3 - abs(h - h1) ? 255 : 63;
            pixel->green = x > h - 10 ? 55 + 20 * (10 - h + x) : 63;
            //pixel->blue = x > h - 4 ? 255 : 63;
        }
    }

    // box
    draw_line(bitmap, left - 1, top - 1, left + width, top - 1, 127);
    draw_line(bitmap, left - 1, top + height, left + width, top + height, 127);
    draw_line(bitmap, left - 1, top - 1, left - 1, top + height, 127);
    draw_line(bitmap, left + width, top - 1, left + width, top + height, 127);

    // color ramp
    for (int y = 0; y < height; ++y) {
        for (int x = width + 8; x < width + 24; ++x) {
            int idx = 0.5 + y * color_max / (height - 1);
            int p = color_max - idx;
            pixel_cmap(bitmap, x + left, y + top, cmap, p);
        }
    }

    int text_len = 255;
    char text[text_len];

    // dbFS

    // want a dbFS marker for about every 70 pixels
    float num_dbfs_markers = height / 50;
    float dbfs_markers_step = (gain + db_range) / num_dbfs_markers;
    // round to 3
    dbfs_markers_step = roundf(dbfs_markers_step / 3) * 3;

    for (float d = gain; d < gain + db_range; d += dbfs_markers_step) {
        if (d >= gain + db_range - dbfs_markers_step)
            d = gain + db_range;
        int y = top + height * (d - gain) / db_range;
        // draw_line(bitmap, width-120, y, width-116, y);
        int y1 = y - font_y / 2; // adjust the sign to line up
        if (d <= gain)
            y1 = y + font_y / 2; // push first label down
        else if (d >= gain + db_range)
            y1 = y - font_y; // pull last label up
        snprintf(text, text_len, "%.0f", -d);
        draw_text_at(left + width + 24, y1, 191, text);
    }

    // y-axis
    autorange_t freq_scale = autorange(center_freq + sample_rate, 10.0);
    snprintf(text, text_len, "f[%sHz]", freq_scale.prefix);
    draw_text_at(16, top - 4, 255, text);
    for (int j = 16; j < height; j += 16) {
        int y = top + j;
        //int f = j <= height / 2 ? height / 2 - j : height - 1 + height / 2 - j;
        //float scaleHz = (freqs[int(f)] - center_freq) / freq_scale.scale;
        float scaleHz = (center_freq + (sample_rate * j / height - sample_rate / 2)) / freq_scale.scale;
        draw_line(bitmap, left -4, y, left, y, 127);
        //snprintf(text, text_len, "%+7.1f%s", scaleHz, freq_scale.prefix);
        snprintf(text, text_len, "%+7.3f", scaleHz);
        draw_text_at(0, y - font_y / 2, 191, text);
    }

    // x-axis
    float total_time = (float)samples / sample_rate;
    autorange_t time_scale = autorange(total_time, 10.0);
    snprintf(text, text_len, "t[%ss]", time_scale.prefix);
    draw_text_at(16, top + height + 1, 255, text);
    float total_time_scaled = total_time / time_scale.scale;

    // want a time marker for about every 85 pixels
    float num_time_markers = width / 85;
    float time_markers_step = total_time_scaled / num_time_markers;
    // round to 5
    time_markers_step = roundf(time_markers_step / 5) * 5;

    float time_per_pixel = width / total_time_scaled;
    printf("total_time_scaled=%f\n", total_time_scaled);
    for (float t = 0.0f; t < total_time_scaled; t += time_markers_step) {
        if (t >= total_time_scaled - time_markers_step)
            t = total_time_scaled;
        int x = left + t * time_per_pixel;
        draw_line(bitmap, x, top + height, x, top + height + 4, 127);
        int x1 = x + 3;
        if (t >= total_time_scaled)
            x1 = x - 6 * font_X;
        snprintf(text, text_len, "%.0f%ss", t, time_scale.prefix);
        draw_text_at(x1, top + height + 2, 191, text);
    }

    // title
    autorange_t base_scale = autorange(center_freq, 10.0);
    autorange_t rate_scale = autorange(sample_rate, 10.0);
    snprintf(text, text_len, "%.6f%sHz @%.0f%sHz -%g+%gdBFS /%d :%d",
             center_freq / base_scale.scale, base_scale.prefix,
             sample_rate / rate_scale.scale, rate_scale.prefix,
             db_range, gain, decimation, height);
    draw_text_at(left + width/2 - 40, 1, 255, text);
}
