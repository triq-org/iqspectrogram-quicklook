/// @file SpectrogramImage.h
/// SDR I/Q data Spectrogram QuickLook Plugin.
///
/// @author Christian W. Zuckschwerdt on 2017-05-23.
/// @copyright 2017 Christian W. Zuckschwerdt. All rights reserved.
///

#import <Foundation/Foundation.h>


CGImageRef CreateImageForURL(CFURLRef url, NSUInteger width, NSUInteger height, BOOL decorations);
