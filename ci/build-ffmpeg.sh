#!/bin/sh

git clone https://github.com/ffmpeg/ffmpeg.git

cd ffmpeg 

export LDFLAGS="-L/usr/local/opt/openssl/lib"
export CPPFLAGS="-I/usr/local/opt/openssl/include"

./configure --prefix=./install/x86_64-darwin \
--enable-shared --disable-debug --disable-everything \
--disable-doc --disable-htmlpages --disable-manpages --disable-podpages --disable-txtpages \
--disable-everything --disable-programs --disable-swscale --disable-coreimage --disable-videotoolbox \
--enable-decoder=aac* --enable-decoder=flac --enable-decoder=mp3* --enable-decoder=pcm* --enable-decoder=vorbis \
--enable-demuxer=aac* --enable-demuxer=flac --enable-demuxer=mp3* --enable-demuxer=ogg --enable-demuxer=mov --enable-demuxer=wav \
--enable-demuxer=hls --enable-demuxer=mpegts \
--enable-protocol=crypto --enable-protocol=file --enable-protocol=hls --enable-protocol=http* --enable-protocol=tcp --enable-protocol=udp \
--enable-bsf=aac_adtstoasc \
--enable-openssl

make clean
make
make install

