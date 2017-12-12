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

UInt8 *convert(const char * path, NSUInteger width, NSUInteger height)
{
    int spectro_width = 3000;
    int spectro_height = 512;
    int border_top = font_y + 2; // title
    int border_bottom = font_y + 2 + 18; // time + amplitude
    int border_left = font_X * 9;
    int border_right = 40 + 100; // 20px (cmap), 100px (histogram)

    bitmap_t canvas;
    canvas.width = spectro_width + border_left + border_right;
    canvas.height = spectro_height + border_top + border_bottom;
    canvas.pixels = calloc(canvas.width * canvas.height, sizeof(pixel_t));
    if (!canvas.pixels)
    {
        return NULL;
    }

    draw_spectrogram(path, &canvas, spectro_width, spectro_height, border_top, border_left);
    //free(canvas.pixels);

    return (UInt8 *)canvas.pixels;
}

CGImageRef CreateImageForURL(CFURLRef url, NSUInteger width, NSUInteger height)
{

    //NSData *fileData = [[NSData alloc] initWithContentsOfURL:(NSURL*)url];
    NSString *path = [(__bridge NSURL*)url path];
    const char *pathCString = [path fileSystemRepresentation];

    UInt8 *pixelData = convert(pathCString, width, height);
    if (!pixelData) {
         NSLog(@"(Spectrogram) cannot convert SDR data for %@", url);
        return NULL;
    }

    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CFDataRef rgbData = CFDataCreate(NULL, pixelData, 3194 * 558 * 3);
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(rgbData);
    CGImageRef rgbImageRef = CGImageCreate(3194, 558, 8, 24, 3194 * 3, colorspace, kCGBitmapByteOrderDefault, provider, NULL, true, kCGRenderingIntentDefault);
    CFRelease(rgbData);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorspace);

    return rgbImageRef;
}
