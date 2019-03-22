APP = mpa

CROSS = 

MAKE = make

CC = $(CROSS)gcc
LD = $(CROSS)ld
STRIP = $(CROSS)strip

CFLAGS = \
    -std=c11 -O2 -g -D_ISOC99_SOURCE -D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE \
    -Wall -Werror=implicit-function-declaration -Wno-error=deprecated-declarations -Wno-error=unused-function \
    -Wempty-body -Wdisabled-optimization -Wstrict-prototypes -Wno-format-zero-length -Werror=format-security \
    -Wno-redundant-decls -Wvla -Wno-logical-op-parentheses -Wno-tautological-compare \
    -Wno-tautological-constant-out-of-range-compare -Wundef -Wmissing-prototypes -Wshadow -Wno-switch \
    -Wparentheses -Wpointer-arith -Wno-pointer-sign -Wno-unused-result -fcolor-diagnostics -pthread

LDFLAGS = \
    -Xlinker -force_load_swift_libs -lc++ -rdynamic \
    -Wl,-framework,CoreFoundation -Wl,-framework,Security  \
    -Wl,-framework,AudioToolbox -Wl,-framework,VideoToolbox -Wl,-framework,CoreMedia \
    -Wl,-framework,CoreAudio -Wl,-framework,CoreVideo -Wl,-framework,CoreServices \
    -Wl,-framework,AudioUnit -Wl,-framework,OpenGL -Wl,-framework,CoreImage -Wl,-framework,AppKit

INCLUDES = -I. -I.. -I./ffmpeg/install/x86_64-darwin/include

LIBS = \
    -L/usr/local/opt/openssl/lib \
    -L./ffmpeg/install/x86_64-darwin/lib \
    ./ffmpeg/install/x86_64-darwin/lib/libavcodec.a \
    ./ffmpeg/install/x86_64-darwin/lib/libavformat.a \
    ./ffmpeg/install/x86_64-darwin/lib/libavutil.a \
    ./ffmpeg/install/x86_64-darwin/lib/libavfilter.a \
    ./ffmpeg/install/x86_64-darwin/lib/libswresample.a \
    -lm -lz -liconv -lcrypto -lssl -lrubberband

SOURCES = \
    audio/aframe.c                        \
    audio/audio_buffer.c                  \
    audio/chmap.c                         \
    audio/chmap_sel.c                     \
    audio/decode/ad_lavc.c                \
    audio/decode/ad_spdif.c               \
    audio/filter/af_format.c              \
    audio/filter/af_lavrresample.c        \
    audio/filter/af_rubberband.c          \
    audio/filter/af_scaletempo.c          \
    audio/fmt-conversion.c                \
    audio/format.c                        \
    audio/out/ao.c                        \
    audio/out/ao_coreaudio.c              \
    audio/out/ao_coreaudio_chmap.c        \
    audio/out/ao_coreaudio_exclusive.c    \
    audio/out/ao_coreaudio_properties.c   \
    audio/out/ao_coreaudio_utils.c        \
    audio/out/ao_null.c                   \
    audio/out/pull.c                      \
    audio/out/push.c                      \
    common/av_common.c                    \
    common/av_log.c                       \
    common/codecs.c                       \
    common/common.c                       \
    common/msg.c                          \
    common/playlist.c                     \
    common/tags.c                         \
    common/version.c                      \
    demux/codec_tags.c                    \
    demux/cue.c                           \
    demux/demux.c                         \
    demux/demux_lavf.c                    \
    demux/demux_null.c                    \
    demux/demux_playlist.c                \
    demux/demux_raw.c                     \
    demux/demux_timeline.c                \
    demux/packet.c                        \
    demux/timeline.c                      \
    filters/f_autoconvert.c               \
    filters/f_auto_filters.c              \
    filters/f_decoder_wrapper.c           \
    filters/f_demux_in.c                  \
    filters/f_lavfi.c                     \
    filters/f_output_chain.c              \
    filters/f_swresample.c                \
    filters/f_utils.c                     \
    filters/filter.c                      \
    filters/frame.c                       \
    filters/user_filters.c                \
    input/cmd.c                           \
    input/input.c                         \
    input/ipc.c                           \
    input/ipc-unix.c                      \
    input/keycodes.c                      \
    misc/bstr.c                           \
    misc/charset_conv.c                   \
    misc/dispatch.c                       \
    misc/json.c                           \
    misc/node.c                           \
    misc/rendezvous.c                     \
    misc/ring.c                           \
    misc/thread_pool.c                    \
    misc/thread_tools.c                   \
    options/m_config.c                    \
    options/m_option.c                    \
    options/m_property.c                  \
    options/options.c                     \
    options/parse_commandline.c           \
    options/parse_configfile.c            \
    options/path.c                        \
    stream/cookies.c                      \
    stream/stream.c                       \
    stream/stream_cb.c                    \
    stream/stream_file.c                  \
    stream/stream_lavf.c                  \
    stream/stream_memory.c                \
    stream/stream_null.c                  \
    osdep/main-fn-unix.c                  \
    osdep/terminal-unix.c                 \
    osdep/io.c                            \
    osdep/threads.c                       \
    osdep/timer.c                         \
    osdep/timer-darwin.c                  \
    osdep/polldev.c                       \
    osdep/path-unix.c                     \
    osdep/semaphore_osx.c                 \
    osdep/subprocess.c                    \
    osdep/subprocess-posix.c              \
    ta/ta.c                               \
    ta/ta_talloc.c                        \
    ta/ta_utils.c                         \
    player/audio.c                        \
    player/client.c                       \
    player/command.c                      \
    player/configfiles.c                  \
    player/loadfile.c                     \
    player/main.c                         \
    player/misc.c                         \
    player/osd.c                          \
    player/playloop.c                     \


OBJS = $(SOURCES:.c=.o)

.SUFFIXES:.c .o

all: $(APP)

$(APP): $(OBJS)
	@echo Linking: $@ ...
	@$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)
	@$(STRIP) -u $@
	@echo "Build finished successfully."

.c.o:
	@echo Compiling: $< ...
	@$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ $<

clean:
	@echo Cleaning: $(APP)
	@rm -f $(APP) $(OBJS)
	@echo "Cleaning finished successfully."


