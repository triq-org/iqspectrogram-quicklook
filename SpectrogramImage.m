/// @file SpectrogramImage.m
/// SDR I/Q data Spectrogram QuickLook Plugin.
///
/// @author Christian W. Zuckschwerdt on 2017-05-23.
/// @copyright 2017 Christian W. Zuckschwerdt. All rights reserved.
///

#import "SpectrogramImage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "triq.h"

UInt8 *CreateSpectrogram(const char * path, NSUInteger width, NSUInteger height, BOOL decorations)
{
    splt_t *plot = splt_create(path);
    if (!plot) {
        return NULL;
    }
    splt_set_dark_theme(plot, 1);
    // splt_set_db_gain(plot, 6.0);
    // splt_set_db_range(plot, 30.0);
    // splt_set_cmap(plot, 0);
    // splt_set_fft_size(plot, 512);
    // splt_set_fft_window(plot, 5);
    // splt_set_layout_histo_width(plot, 100);
    // splt_set_layout_deci_height(plot, 16);
    splt_set_fft_size(plot, 512);
    splt_set_layout_size(plot, (uint32_t)width, (uint32_t)height);
    splt_set_fft_size(plot, 512);

    uint32_t plot_width = splt_get_layout_width(plot);
    uint32_t plot_height = splt_get_layout_height(plot);
    uint32_t *pixels = malloc(plot_width * plot_height * sizeof(uint32_t));
    // or: void * CFAllocatorAllocate(NULL, CFIndex size, 0);
    // and: CFDataCreateWithBytesNoCopy(NULL, data, size, NULL);
    // rather:
    // CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, const UInt8 *bytes, CFIndex length, NULL); // auto-free
    // CGDataProvider provider = CGDataProviderCreateWithCFData( (CFDataRef) data );
    if (!pixels) {
        return NULL;
    }
    // NSLog(@"(IQSpectrogram) rendering %u x %u (%u x %u)", plot_width, plot_height, (uint32_t)width, (uint32_t)height);
    splt_draw(plot, pixels, plot_width, plot_height);
    // NSLog(@"(IQSpectrogram) rendered %u x %u", plot_width, plot_height);
    splt_destroy(plot);

    return (UInt8 *)pixels;
}

void cgDataProviderReleaseDataCallback(void *info, const void *data, size_t size)
{
    free((void *)data);
}

CGImageRef CreateImageForURL(CFURLRef url, NSUInteger width, NSUInteger height, BOOL decorations)
{
    // NSData *fileData = [[NSData alloc] initWithContentsOfURL:(NSURL*)url];
    NSString *path = [(__bridge NSURL*)url path];
    const char *pathCString = [path fileSystemRepresentation];

    // fudge
    size_t dataWidth = width;
    size_t dataHeight = height + 42 + 132;
    if (width <= 256) {
        dataWidth = width;
        dataHeight = height;
    }
    size_t bitsPerComponent = 8;
    size_t bitsPerPixel = 8 * 4;
    size_t bytesPerRow = dataWidth * 4;

    UInt8 *pixelData = CreateSpectrogram(pathCString, dataWidth, dataHeight, decorations);
    if (!pixelData) {
         NSLog(@"(IQSpectrogram) cannot convert SDR data for %@", url);
        return NULL;
    }

    // CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    // CFDataCreate + CGDataProviderCreateWithCFData would copy. use a release callback instead:
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, pixelData, dataWidth * dataHeight * 4, cgDataProviderReleaseDataCallback);

    CGImageRef rgbImageRef = CGImageCreate(dataWidth, dataHeight, bitsPerComponent, bitsPerPixel, bytesPerRow,
            colorspace, kCGBitmapByteOrderDefault | kCGImageAlphaNoneSkipLast, provider, NULL, true, kCGRenderingIntentDefault);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorspace);

    return rgbImageRef;
}
