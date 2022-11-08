#import <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>
#import "SpectrogramImage.h"

/* -----------------------------------------------------------------------------
   Generate a preview for file

   This function's job is to create preview for designated file
   ----------------------------------------------------------------------------- */

OSStatus GeneratePreviewForURL(void *thisInterface, QLPreviewRequestRef preview, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options)
{
    // Preset values
    NSUInteger maxWidth = 1800;
    NSUInteger maxHeight = 512;

    CGImageRef image = CreateImageForURL(url, maxWidth, maxHeight, YES);
    if (image == NULL) {
        return -1;
    }
    CGFloat width = CGImageGetWidth(image);
    CGFloat height = CGImageGetHeight(image);
    // NSLog(@"Got render size of %f x %f", width, height);

    if (QLPreviewRequestIsCancelled(preview)) {
        CGImageRelease(image);
        return kQLReturnNoError;
    }

    @autoreleasepool {
        NSDictionary *newOpt = [NSDictionary  dictionaryWithObjectsAndKeys:(NSString *)[(__bridge NSDictionary *)options objectForKey:(NSString *)kQLPreviewPropertyDisplayNameKey], kQLPreviewPropertyDisplayNameKey, [NSNumber numberWithFloat:width], kQLPreviewPropertyWidthKey, [NSNumber numberWithFloat:height], kQLPreviewPropertyHeightKey, nil];
        CGContextRef ctx = QLPreviewRequestCreateContext(preview, CGSizeMake(width, height), YES, (__bridge CFDictionaryRef)newOpt);
        CGContextDrawImage(ctx, CGRectMake(0,0,width,height), image);
        QLPreviewRequestFlushContext(preview, ctx);
        CGImageRelease(image);
        CGContextRelease(ctx);
    }
    return kQLReturnNoError;
}

void CancelPreviewGeneration(void *thisInterface, QLPreviewRequestRef preview)
{
    // Implement only if supported
}
