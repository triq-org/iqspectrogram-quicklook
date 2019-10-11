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

#include "font.h"
#include "draw.h"
#include "spectrogram.h"

UInt8 *CreateSpectrogram(const char * path, NSUInteger width, NSUInteger height, BOOL decorations)
{
    int spectro_width = (int)width;
    int spectro_height = (int)height;
    int border_top = font_y + 2; // title
    int border_bottom = font_y + 2 + 18; // time + amplitude
    int border_left = font_X * 9;
    int border_right = 40 + 100; // 20px (cmap), 100px (histogram)

    if (spectro_width <= 256) {
        border_top = 0;
        border_bottom = 0;
        border_left = 0;
        border_right = 0;
    }

    bitmap_t canvas;
    canvas.width = spectro_width + border_left + border_right;
    canvas.height = spectro_height + border_top + border_bottom;
    canvas.pixels = calloc(canvas.width * canvas.height, sizeof(pixel_t));
    // or: void * CFAllocatorAllocate(NULL, CFIndex size, 0);
    // and: CFDataCreateWithBytesNoCopy(NULL, data, size, NULL);
    // rather:
    // CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, const UInt8 *bytes, CFIndex length, NULL); // auto-free
    // CGDataProvider provider = CGDataProviderCreateWithCFData( (CFDataRef) data );
    if (!canvas.pixels)
    {
        return NULL;
    }

    draw_spectrogram(path, &canvas, spectro_width, spectro_height, border_top, border_left);

    return (UInt8 *)canvas.pixels;
}

void cgDataProviderReleaseDataCallback(void *info, const void *data, size_t size)
{
    free((void *)data);
}

CGImageRef CreateImageForURL(CFURLRef url, NSUInteger width, NSUInteger height, BOOL decorations)
{

    //NSData *fileData = [[NSData alloc] initWithContentsOfURL:(NSURL*)url];
    NSString *path = [(__bridge NSURL*)url path];
    const char *pathCString = [path fileSystemRepresentation];

    UInt8 *pixelData = CreateSpectrogram(pathCString, width, height, decorations);
    if (!pixelData) {
         NSLog(@"(Spectrogram) cannot convert SDR data for %@", url);
        return NULL;
    }
    // fudge
    size_t dataWidth = width + 194;
    size_t dataHeight = height + 46;
    if (width <= 256) {
        dataWidth = width;
        dataHeight = height;
    }
    size_t bitsPerComponent = 8;
    size_t bitsPerPixel = 8 * 3;
    size_t bytesPerRow = dataWidth * 3;

    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();

    // CFDataCreate + CGDataProviderCreateWithCFData would copy. use a release callback instead:
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, pixelData, dataWidth * dataHeight * 3, cgDataProviderReleaseDataCallback);

    CGImageRef rgbImageRef = CGImageCreate(dataWidth, dataHeight, bitsPerComponent, bitsPerPixel, bytesPerRow, colorspace, kCGBitmapByteOrderDefault, provider, NULL, true, kCGRenderingIntentDefault);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorspace);

    return rgbImageRef;
}
