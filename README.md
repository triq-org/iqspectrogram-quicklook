# SDR I/Q data Spectrogram QuickLook Plugin

This is an open-source QuickLook plugin to generate spectrogram thumbnails and previews for SDR I/Q data.
Requires Mac OS X 10.7 or later (but tested on 10.8).


## Installation

Click [Releases](https://github.com/zuckschwerdt/iqspectrogram-quicklook/releases) and download the latest version. Unzip it, and put `IQSpectrogram.qlgenerator` into `~/Library/QuickLook` (or `/Library/QuickLook`). You may need to reboot to enable it (or open Terminal and run `qlmanage -r`).


## Development Notes

The plugin needs a precompiled `libfftw3.a`.
