#!/bin/sh

export PKG_CONFIG_PATH="usr/lib/pkgconfig:/usr/local/lib/pkgconfig:./ffmpeg/install/lib/pkgconfig";

./waf configure \
--enable-libmpa-static --enable-static-build
./waf build --verbose 

