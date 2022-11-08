/** @file
    triq -- TRansform I/Q data.

    Copyright (C) 2022 Christian Zuckschwerdt
*/

#ifndef INCLUDE_TRIQ_H_
#define INCLUDE_TRIQ_H_

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define OOK_MAX_HIGH_LEVEL 16384

#define OOK_MAX_LOW_LEVEL 518

#define OOK_EST_HIGH_RATIO 64

#define OOK_EST_LOW_RATIO 1024

typedef struct Spectrogram_SampleFile Spectrogram_SampleFile;

typedef struct Spectrogram_SampleFile splt_t;

/**
 * Convert a I/Q sample buffer.
 */
void pulse_convert(uint8_t *ptr, uintptr_t cap);

/**
 * Construct a new `Spectrogram` plot which will load the provided file path and fill out
 * all other fields with their defaults.
 *
 * # Note
 *
 * If the string passed in isn't a valid file path this will return a null pointer.
 *
 * # Safety
 *
 * Make sure you destroy the spectrogram with [`splt_destroy()`] once you are
 * done with it.
 *
 * [`splt_destroy()`]: fn.splt_destroy.html
 */
splt_t *splt_create(const char *path);

/**
 * Get the width on the Spectrogram plot.
 */
uint32_t splt_get_layout_width(const splt_t *plot);

/**
 * Get the height on the Spectrogram plot.
 */
uint32_t splt_get_layout_height(const splt_t *plot);

/**
 * Get the dark_theme on the Spectrogram plot.
 */
bool splt_get_dark_theme(const splt_t *plot);

/**
 * Set the dark_theme on the Spectrogram plot.
 */
void splt_set_dark_theme(splt_t *plot, bool dark);

/**
 * Get the origin on the Spectrogram plot.
 */
uint32_t splt_get_origin(const splt_t *plot);

/**
 * Set the origin on the Spectrogram plot.
 */
void splt_set_origin(splt_t *plot, uint32_t origin);

/**
 * Get the zoom on the Spectrogram plot.
 */
uint32_t splt_get_zoom(const splt_t *plot);

/**
 * Set the zoom on the Spectrogram plot.
 */
void splt_set_zoom(splt_t *plot, uint32_t zoom);

/**
 * Get the db_gain on the Spectrogram plot.
 */
float splt_get_db_gain(const splt_t *plot);

/**
 * Set the db_gain on the Spectrogram plot.
 */
void splt_set_db_gain(splt_t *plot, float db_gain);

/**
 * Get the db_range on the Spectrogram plot.
 */
float splt_get_db_range(const splt_t *plot);

/**
 * Set the db_range on the Spectrogram plot.
 */
void splt_set_db_range(splt_t *plot, float db_range);

/**
 * Get the cmap on the Spectrogram plot.
 */
uint32_t splt_get_cmap(const splt_t *plot);

/**
 * Set the cmap on the Spectrogram plot.
 */
void splt_set_cmap(splt_t *plot, uint32_t cmap);

/**
 * Get the fft_size on the Spectrogram plot.
 */
uint32_t splt_get_fft_size(const splt_t *plot);

/**
 * Set the fft_size on the Spectrogram plot.
 */
void splt_set_fft_size(splt_t *plot, uint32_t fft_size);

/**
 * Get the fft_window on the Spectrogram plot.
 */
uint8_t splt_get_fft_window(const splt_t *plot);

/**
 * Set the fft_window on the Spectrogram plot.
 */
void splt_set_fft_window(splt_t *plot, uint8_t fft_window_name);

/**
 * Set the layout_size on the Spectrogram plot.
 */
void splt_set_layout_size(splt_t *plot, uint32_t width, uint32_t height);

/**
 * Get the direction on the Spectrogram plot.
 */
uint8_t splt_get_layout_direction(const splt_t *plot);

/**
 * Set the direction on the Spectrogram plot.
 */
void splt_set_layout_direction(splt_t *plot, uint8_t direction);

/**
 * Get the plot_across on the Spectrogram plot.
 */
uint32_t splt_get_layout_plot_across(const splt_t *plot);

/**
 * Set the plot_across on the Spectrogram plot.
 */
void splt_set_layout_plot_across(splt_t *plot, uint32_t plot_across);

/**
 * Get the histo_width on the Spectrogram plot.
 */
uint32_t splt_get_layout_histo_width(const splt_t *plot);

/**
 * Set the histo_width on the Spectrogram plot.
 */
void splt_set_layout_histo_width(splt_t *plot, uint32_t histo_width);

/**
 * Get the deci_height on the Spectrogram plot.
 */
uint32_t splt_get_layout_deci_height(const splt_t *plot);

/**
 * Set the deci_height on the Spectrogram plot.
 */
void splt_set_layout_deci_height(splt_t *plot, uint32_t deci_height);

/**
 * Draw a `Spectrogram` into a pixel buffer.
 */
void splt_draw(splt_t *plot, uint32_t *pixels, uint32_t width, uint32_t height);

/**
 * Destroy a `Spectrogram` once you are done with it.
 */
void splt_destroy(splt_t *plot);

#endif /* INCLUDE_TRIQ_H_ */
