/// @file spectrogram.c
/// SDR I/Q data Spectrogram QuickLook Plugin.
///
/// @author Christian W. Zuckschwerdt on 2017-05-23.
/// @copyright 2017 Christian W. Zuckschwerdt. All rights reserved.
///

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <string.h>
#include <strings.h>
//#include <complex.h>
#include <fftw3.h>

#include "font.h"
#include "cube1_cmap.h"
#include "draw.h"

#include "spectrogram.h"

enum sample_format {
    FORMAT_CU4,
    FORMAT_CS4,
    FORMAT_CU8,
    FORMAT_CS8,
    FORMAT_CU12,
    FORMAT_CS12,
    FORMAT_CU16,
    FORMAT_CS16,
    FORMAT_CU32,
    FORMAT_CS32,
    FORMAT_CU64,
    FORMAT_CS64,
    FORMAT_CF32,
    FORMAT_CF64
};

union sample_data {
    uint8_t *cu8;
    int8_t *cs8;
    uint16_t *cu16;
    int16_t *cs16;
    uint32_t *cu32;
    int32_t *cs32;
    uint64_t *cu64;
    int64_t *cs64;
    float *cf32;
    double *cf64;
};

extern inline pixel_t *pixel_at(bitmap_t *bitmap, int x, int y);
extern inline void pixel_set(bitmap_t *bitmap, int x, int y, int c);
extern inline void pixel_color(bitmap_t *bitmap, int x, int y, uint8_t *c);
extern inline void pixel_cmap(bitmap_t *bitmap, int x, int y, uint8_t *cmap, int c);
extern inline void fill_rect(bitmap_t *bitmap, int left, int top, int width, int height, int c);

int parse_freq_rate(const char *name, double *freq_out, double *rate_out) {
    if (!name) {
        return 0;
    }

    // remove path
    const char *p = strrchr(name, '/');
    if (p) name = ++p;

    // skip until separator [-_ .]
    for (p = name; *p; ++p) {
        if (*p == '_' || *p == '-' || *p == ' ' || *p == '.') {
            // try to parse a double (float has too few digits)
            char *e = NULL;
            double f = strtod(++p, &e);
            if (p == e) continue;

            // suffixed with 'M' and separator?
            if ((*e == 'M' || *e == 'm')
                    && (*(e+1) == '_' || *(e+1) == '-' || *(e+1) == ' ' || *(e+1) == '.' || *(e+1) == '\0')) {
                p = e;
                *freq_out = f * 1000000.0;
            }

            // suffixed with 'k' and separator?
            if ((*e == 'k' || *e == 'K')
                    && (*(e+1) == '_' || *(e+1) == '-' || *(e+1) == ' ' || *(e+1) == '.' || *(e+1) == '\0')) {
                p = e;
                *rate_out = f * 1000.0;
            }
        }
    }

    return 0;
}

static double autosteps[] = {0.1, 0.2, 0.5, 1.0};
static const int autosteps_length = sizeof(autosteps) / sizeof(double);

static double autostep(double range, int max_ticks)
{
    int magnitude = log10(range);
    double scale = pow(10, magnitude);
    double norm_range = range / scale;
    for (int i = 0; i < autosteps_length; ++i) {
        double step = autosteps[i];
        if ((max_ticks + 1) * step > norm_range) {
            return step * scale;
        }
    }
    return scale; // fallback
}

typedef struct
{
    double scale;
    char *prefix;
} autorange_t;

static autorange_t autoranges[] = {
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
static autorange_t autorange(double num, double min_int)
{
    if (num == 0.0)
        return autoranges[8];
    if (min_int == 0.0) min_int = 10.0;
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

static int clip(int min, int max, int num)
{
    return num < min ? min : num > max ? max : num;
}

static int get_sample_format(const char *path)
{
    char *ext = strrchr(path, '.');
    if (ext) {
        if (!strcmp(ext, ".cu4")) {
            return FORMAT_CU4;
        }
        if (!strcmp(ext, ".cs4")) {
            return FORMAT_CS4;
        }
        if (!strcmp(ext, ".cu8") || !strcmp(ext, ".data") || !strcmp(ext, ".complex16u")) {
            return FORMAT_CU8;
        }
        if (!strcmp(ext, ".cs8") || !strcmp(ext, ".complex16s")) {
            return FORMAT_CS8;
        }
        if (!strcmp(ext, ".cu12")) {
            return FORMAT_CU12;
        }
        if (!strcmp(ext, ".cs12")) {
            return FORMAT_CS12;
        }
        if (!strcmp(ext, ".cu16")) {
            return FORMAT_CU16;
        }
        if (!strcmp(ext, ".cs16")) {
            return FORMAT_CS16;
        }
        if (!strcmp(ext, ".cu32")) {
            return FORMAT_CU32;
        }
        if (!strcmp(ext, ".cs32")) {
            return FORMAT_CS32;
        }
        if (!strcmp(ext, ".cu64")) {
            return FORMAT_CU64;
        }
        if (!strcmp(ext, ".cs64")) {
            return FORMAT_CS64;
        }
        if (!strcmp(ext, ".cf32") || !strcmp(ext, ".cfile") || !strcmp(ext, ".complex")) {
            return FORMAT_CF32;
        }
        if (!strcmp(ext, ".cf64")) {
            return FORMAT_CF64;
        }
    }
    return FORMAT_CU8; // fallback...
}

// http://www.fftw.org/fftw3_doc/Complex-One_002dDimensional-DFTs.html
// https://stackoverflow.com/questions/21283144/generating-correct-spectrogram-using-fftw-and-window-function
void draw_spectrogram(const char *fname, bitmap_t *bitmap, int width, int height, int top, int left)
{
    if (!fname) {
        printf("file missing!\n");
        return;
    }
    // read iq data
    int fd = open(fname, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    struct stat buf;
    fstat(fd, &buf);
    off_t size = buf.st_size;
    printf("%d: size = %jd\n", fd, (intmax_t)size);

    union sample_data data;
    data.cu8 = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (data.cu8 == MAP_FAILED) {
        perror("mmap");
        return;
    }

    int sample_format = get_sample_format(fname);
    printf("sample_format = %d\n", sample_format);

    //uint8_t *data = malloc(size);
    //uint8_t is[] = {127, 0, 127, 255};
    //uint8_t qs[] = {0, 127, 255, 127};
    //for (int i = 0; i < size / 2; ++i) {
    //    data[2 * i + 0] = is[i % 4];
    //    data[2 * i + 1] = qs[i % 4];
    //}
    //size_t r = fread(data, 1, size, fp);

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
    fftw_plan plan;


    double center_freq_out = 0.0;
    double sample_rate_out = 0.0;
    parse_freq_rate(fname, &center_freq_out, &sample_rate_out);

    // params
    int center_freq = center_freq_out;
    int sample_rate = sample_rate_out != 0.0 ? sample_rate_out : 250000;
    int decimation = 1;

    int decorations = width > 256 ? 1 : 0;
    int color_max = 255;
    // cu8 has a possible range of 0 to -60 dBfs
    float gain = 6.0;
    float db_range = 30;

    // stride setup
    int bytes_per_sample = 2;
    switch (sample_format) {
        case FORMAT_CU4:
        case FORMAT_CS4:
            bytes_per_sample = 1;
            //db_range = 20;
            break;
        case FORMAT_CU8:
        case FORMAT_CS8:
            bytes_per_sample = 2;
            //db_range = 50;
            break;
        case FORMAT_CU12:
        case FORMAT_CS12:
            bytes_per_sample = 3;
            break;
        case FORMAT_CU16:
        case FORMAT_CS16:
            bytes_per_sample = 4;
            break;
        case FORMAT_CU32:
        case FORMAT_CS32:
        case FORMAT_CF32:
            bytes_per_sample = 8;
            //db_range = 100;
            db_range = 40;
            break;
        case FORMAT_CU64:
        case FORMAT_CS64:
        case FORMAT_CF64:
            bytes_per_sample = 16;
            //db_range = 100;
            db_range = 60;
            break;
    }

    uint8_t *cmap = cube1_cmap;
    float color_norm = color_max / -db_range;

    int a_y = top + height + font_y + 2;
    int a_height = 16;

    off_t samples = size / bytes_per_sample;
    int points = width;
    float stride = (samples - N) / (points - 1);
    printf("stride = %f\n", stride);

    // plan
    in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
    plan = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    // Hann window
    float window[N];
    float window_sum = blackman_harris_window(window, N);

    //float block_norm = 1.0 / N;
    float block_norm = 1.0 / window_sum;
    float block_norm_db = 10 * log10(block_norm);
    printf("block_norm=%d, window_sum=%f\n", N, window_sum);

    // transform
    float dBfs_silence = -99.9; // instead of -INF
    float dBfs_min = 0.0;
    float dBfs_max = dBfs_silence;

    int c_hist[color_max + 1];
    memset(c_hist, 0, (color_max + 1) * sizeof(int));
    int cB_hist_size = -10.0 * dBfs_silence + 1; // centi Bell (0.1 dB)
    int cB_hist[cB_hist_size]; // -0.0 to -100.0 dB
    memset(cB_hist, 0, cB_hist_size * sizeof(int));

    const float s_bias = 127.5; // or 128?
    const float s_scale = 1.0 / 127.5;
    for (int x = 0; x < width; ++x) {
        size_t j = x * stride;

        switch (sample_format) {
            case FORMAT_CU4:
                for (int k = 0; k < N; ++k) {
                    uint8_t b0 = data.cu8[1 * (k + j) + 0];
                    //in[k][0] = window[k] * ((((b0 & 0xf0) >> 4) - 7.5) / 7.5);
                    //in[k][1] = window[k] * ((((b0 & 0x0f) >> 0) - 7.5) / 7.5);
                    in[k][0] = window[k] * (((b0 & 0xf0) >> 4) / 7.5 - 1.0);
                    in[k][1] = window[k] * (((b0 & 0x0f) >> 0) / 7.5 - 1.0);
                }
                break;
            case FORMAT_CS4:
                for (int k = 0; k < N; ++k) {
                    uint8_t b0 = data.cu8[1 * (k + j) + 0];
                    in[k][0] = window[k] * (int8_t)((b0 & 0xf0) << 0) / 0x0.8p8;
                    in[k][1] = window[k] * (int8_t)((b0 & 0x0f) << 4) / 0x0.8p8;
                }
                break;
            case FORMAT_CU8:
                for (int k = 0; k < N; ++k) {
                    in[k][0] = window[k] * (data.cu8[2 * (k + j) + 0] - s_bias) * s_scale;
                    in[k][1] = window[k] * (data.cu8[2 * (k + j) + 1] - s_bias) * s_scale;
                }
                break;
            case FORMAT_CS8:
                for (int k = 0; k < N; ++k) {
                    in[k][0] = window[k] * data.cs8[2 * (k + j) + 0] / 0x0.8p8;
                    in[k][1] = window[k] * data.cs8[2 * (k + j) + 1] / 0x0.8p8;
                }
                break;
            case FORMAT_CU12:
                for (int k = 0; k < N; ++k) {
                    uint8_t b0 = data.cu8[3 * (k + j) + 0];
                    uint8_t b1 = data.cu8[3 * (k + j) + 1];
                    uint8_t b2 = data.cu8[3 * (k + j) + 2];
                    // read 24 bit (iiqIQQ), note the intermediate is Q0.16, MSB aligned uint16_t
                    in[k][0] = window[k] * ((uint16_t)((b1 << 12) | (b0 << 4)) / 0x0.8p16 - 1.0);
                    in[k][1] = window[k] * ((uint16_t)((b2 << 8) | (b1 & 0xf0)) / 0x0.8p16 - 1.0);
                }
                break;
            case FORMAT_CS12:
                for (int k = 0; k < N; ++k) {
                    uint8_t b0 = data.cu8[3 * (k + j) + 0];
                    uint8_t b1 = data.cu8[3 * (k + j) + 1];
                    uint8_t b2 = data.cu8[3 * (k + j) + 2];
                    // read 24 bit (iiqIQQ), note the intermediate is Q0.15, MSB aligned int16_t
                    in[k][0] = window[k] * (int16_t)((b1 << 12) | (b0 << 4)) / 0x0.8p16;
                    in[k][1] = window[k] * (int16_t)((b2 << 8) | (b1 & 0xf0)) / 0x0.8p16;
                }
                break;
            case FORMAT_CU16:
                for (int k = 0; k < N; ++k) {
                    in[k][0] = window[k] * (data.cu16[2 * (k + j) + 0] / 0x0.8p16 - 1.0);
                    in[k][1] = window[k] * (data.cu16[2 * (k + j) + 1] / 0x0.8p16 - 1.0);
                }
                break;
            case FORMAT_CS16:
                for (int k = 0; k < N; ++k) {
                    in[k][0] = window[k] * data.cs16[2 * (k + j) + 0] / 0x0.8p16;
                    in[k][1] = window[k] * data.cs16[2 * (k + j) + 1] / 0x0.8p16;
                }
                break;
            case FORMAT_CU32:
                for (int k = 0; k < N; ++k) {
                    in[k][0] = window[k] * (data.cu32[2 * (k + j) + 0] / 0x0.8p32 - 1.0);
                    in[k][1] = window[k] * (data.cu32[2 * (k + j) + 1] / 0x0.8p32 - 1.0);
                }
                break;
            case FORMAT_CS32:
                for (int k = 0; k < N; ++k) {
                    in[k][0] = window[k] * data.cs32[2 * (k + j) + 0] / 0x0.8p32;
                    in[k][1] = window[k] * data.cs32[2 * (k + j) + 1] / 0x0.8p32;
                }
                break;
            case FORMAT_CU64:
                for (int k = 0; k < N; ++k) {
                    in[k][0] = window[k] * (data.cu64[2 * (k + j) + 0] / 0x0.8p64 - 1.0);
                    in[k][1] = window[k] * (data.cu64[2 * (k + j) + 1] / 0x0.8p64 - 1.0);
                }
                break;
            case FORMAT_CS64:
                for (int k = 0; k < N; ++k) {
                    in[k][0] = window[k] * data.cs64[2 * (k + j) + 0] / 0x0.8p64;
                    in[k][1] = window[k] * data.cs64[2 * (k + j) + 1] / 0x0.8p64;
                }
                break;
            case FORMAT_CF32:
                for (int k = 0; k < N; ++k) {
                    in[k][0] = window[k] * data.cf32[2 * (k + j) + 0];
                    in[k][1] = window[k] * data.cf32[2 * (k + j) + 1];
                }
                break;
            case FORMAT_CF64:
                for (int k = 0; k < N; ++k) {
                    in[k][0] = window[k] *data.cf64[2 * (k + j) + 0];
                    in[k][1] = window[k] *data.cf64[2 * (k + j) + 1];
                }
                break;
        }

        // run
        fftw_execute(plan); /* repeat as needed */

        float dBfs_min_i = 0.0;
        float dBfs_max_i = -200.0;
        for (int k = 0; k < N; ++k) {
            // the positive frequencies are stored in the first half and
            // the negative frequencies are stored in backwards order in the second half.
            // (The frequency -k/n is the same as the frequency (n-k)/n.)
            int y = k <= N / 2 ? N / 2 - k : N / 2 + N - k;

            float abs2 = out[k][0] * out[k][0] + out[k][1] * out[k][1];
            // TODO: float abs2_min_cutoff = exp10(dBfs_silence / 5); // = 1e-20
            // TODO: if (abs2 < abs2_min_cutoff) dBfs = dBfs_silence;
            //float dBfs = 10 * log10(sqrt(abs2) * block_norm) + gain;
            float dBfs = 5 * log10(abs2) + block_norm_db + gain;
            if (abs2 < 1e-20 || dBfs < dBfs_silence) dBfs = dBfs_silence;

            if (dBfs < dBfs_min) dBfs_min = dBfs;
            if (dBfs > dBfs_max) dBfs_max = dBfs;
            if (dBfs < dBfs_min_i) dBfs_min_i = dBfs;
            if (dBfs > dBfs_max_i) dBfs_max_i = dBfs;
            int b = 0.5 + (dBfs - gain) * -10.0;
            if (b >= cB_hist_size) b = cB_hist_size - 1;
            cB_hist[b] += 1;

            int p = color_max - clip(0, color_max, 0.5 + dBfs * color_norm);
            c_hist[p] += 1;

            pixel_cmap(bitmap, x + left, y + top, cmap, p);
        }

        if (decorations) {
            // amplitude gauge
            int min_gauge = clip(0, 255, (db_range + dBfs_min_i) * 256 / db_range);
            int max_gauge = clip(0, 255, (db_range + dBfs_max_i) * 256 / db_range);
            fill_rect(bitmap, x + left, a_y + min_gauge / 16, 1, max_gauge / 16 - min_gauge / 16, max_gauge);
        }
    }
    printf("\nrange %f to %f\n", dBfs_min, dBfs_max);

    // end
    if (munmap(data.cu8, size) == -1)
        perror("munmap");
    //free(data.cu8);
    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    if (!decorations) return;

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
    for (int i = 0; i < cB_hist_size; ++i) {
        if (cB_hist[i] > cB_hist_max)
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
    fill_rect(bitmap, left - 1, top - 1, width + 2, 1, 127);
    fill_rect(bitmap, left - 1, top + height, width + 2, 1, 127);
    fill_rect(bitmap, left - 1, top - 1, 1, height + 2, 127);
    fill_rect(bitmap, left + width, top - 1, 1, height + 2, 127);

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
    if (dbfs_markers_step < 1.0f) dbfs_markers_step = 1.0f;

    for (float d = gain; d < gain + db_range; d += dbfs_markers_step) {
        if (d >= gain + db_range - dbfs_markers_step)
            d = gain + db_range;
        int y = top + height * (d - gain) / db_range;
        // fill_rect(bitmap, width-120, y, 4, 1);
        int y1 = y - font_y / 2; // adjust the sign to line up
        if (d <= gain)
            y1 = y + font_y / 2; // push first label down
        else if (d >= gain + db_range)
            y1 = y - font_y; // pull last label up
        snprintf(text, text_len, "%.0f", -d);
        draw_text_at(left + width + 24, y1, 191, text);
    }

    // y-axis
    int min_spacing = 16;
    int max_ticks = height / 2 / min_spacing;
    double step = autostep(sample_rate / 2, max_ticks);
    int step_count = (int)(0.5 + (sample_rate / 2) / step) - 1; // round up and subtract the last tick

    autorange_t freq_scale = autorange(center_freq + sample_rate, 10.0);
    snprintf(text, text_len, "f[%sHz]", freq_scale.prefix);
    draw_text_at(16, top - 4, 255, text);
    for (int j = 8; j < height; j += 8) {
        int y = top + j;
        fill_rect(bitmap, left - 3, y, 3, 1, 95);
    }
    double pixel_per_hz = (height / 2) / (double)(sample_rate / 2);
    for (int j = -step_count; j <= step_count; ++j) {
        int y = top + height / 2 - j * step * pixel_per_hz;
        double scaleHz = ((double)center_freq + j * step) / freq_scale.scale;
        fill_rect(bitmap, left - 4, y, 4, 1, 127);
        //snprintf(text, text_len, "%+7.1f%s", scaleHz, freq_scale.prefix);
        if (center_freq)
            snprintf(text, text_len, "%8.3f", scaleHz);
        else
            snprintf(text, text_len, "%+8.2f", scaleHz);
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
    if (time_markers_step < 1.0f) time_markers_step = 1.0f;

    float time_per_pixel = width / total_time_scaled;
    printf("total_time_scaled=%f\n", total_time_scaled);
    for (float t = 0.0f; t < total_time_scaled; t += time_markers_step/5) {
        if (t >= total_time_scaled - time_markers_step)
            t = total_time_scaled;
        int x = left + t * time_per_pixel;
        fill_rect(bitmap, x, top + height, 1, 4, 95);
    }
    for (float t = 0.0f; t < total_time_scaled; t += time_markers_step) {
        if (t >= total_time_scaled - time_markers_step)
            t = total_time_scaled;
        int x = left + t * time_per_pixel;
        fill_rect(bitmap, x, top + height, 1, 6, 127);
        int x1 = x + 3;
        if (t >= total_time_scaled)
            x1 = x - 6 * font_X;
        snprintf(text, text_len, "%.0f%ss", t, time_scale.prefix);
        draw_text_at(x1, top + height + 2, 191, text);
    }

    // title
    autorange_t base_scale = autorange(center_freq, 10.0);
    autorange_t rate_scale = autorange(sample_rate, 10.0);
    snprintf(text, text_len, "%g%sHz @%.0f%sHz -%g+%gdBFS /%d :%d",
             (double)center_freq / base_scale.scale, base_scale.prefix,
             (double)sample_rate / rate_scale.scale, rate_scale.prefix,
             db_range, gain, decimation, height);
    draw_text_at(left + width/2 - 40, 1, 255, text);
}
