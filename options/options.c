/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_CFG_MPLAYER_H
#define MPLAYER_CFG_MPLAYER_H

/*
 * config for cfgparser
 */

#include <stddef.h>
#include <sys/types.h>
#include <limits.h>
#include <math.h>

#include "config.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "options.h"
#include "m_config.h"
#include "m_option.h"
#include "common/common.h"
#include "stream/stream.h"
#include "player/core.h"
#include "player/command.h"
#include "stream/stream.h"

static void print_version(struct mp_log *log)
{
    mp_print_version(log, true);
}

extern const struct m_sub_options stream_lavf_conf;
extern const struct m_sub_options demux_rawaudio_conf;
extern const struct m_sub_options demux_lavf_conf;
extern const struct m_sub_options ad_lavc_conf;
extern const struct m_sub_options input_config;
extern const struct m_sub_options ao_alsa_conf;

extern const struct m_sub_options demux_conf;

extern const struct m_obj_list af_obj_list;
extern const struct m_sub_options ao_conf;

#undef OPT_BASE_STRUCT
#define OPT_BASE_STRUCT struct MPOpts

const m_option_t mp_opts[] = {
    // handled in command line pre-parser (parse_commandline.c)
    {"v", &m_option_type_dummy_flag, M_OPT_FIXED | CONF_NOCFG | M_OPT_NOPROP,
     .offset = -1},
    {"playlist", CONF_TYPE_STRING, CONF_NOCFG | M_OPT_MIN | M_OPT_FIXED | M_OPT_FILE,
     .min = 1, .offset = -1},
    {"{", &m_option_type_dummy_flag, CONF_NOCFG | M_OPT_FIXED | M_OPT_NOPROP,
     .offset = -1},
    {"}", &m_option_type_dummy_flag, CONF_NOCFG | M_OPT_FIXED | M_OPT_NOPROP,
     .offset = -1},

    // handled in m_config.c
    { "include", CONF_TYPE_STRING, M_OPT_FILE, .offset = -1},
    { "profile", CONF_TYPE_STRING_LIST, 0, .offset = -1},
    { "show-profile", CONF_TYPE_STRING, CONF_NOCFG | M_OPT_FIXED | M_OPT_NOPROP,
      .offset = -1},
    { "list-options", &m_option_type_dummy_flag, CONF_NOCFG | M_OPT_FIXED |
      M_OPT_NOPROP, .offset = -1},
    OPT_FLAG("list-properties", property_print_help,
             CONF_NOCFG | M_OPT_FIXED | M_OPT_NOPROP),
    { "help", CONF_TYPE_STRING, CONF_NOCFG | M_OPT_FIXED | M_OPT_NOPROP |
              M_OPT_OPTIONAL_PARAM, .offset = -1},
    { "h", CONF_TYPE_STRING, CONF_NOCFG | M_OPT_FIXED | M_OPT_NOPROP |
           M_OPT_OPTIONAL_PARAM, .offset = -1},

    OPT_PRINT("list-protocols", stream_print_proto_list),
    OPT_PRINT("version", print_version),
    OPT_PRINT("V", print_version),

    OPT_CHOICE("player-operation-mode", operation_mode,
               M_OPT_FIXED | M_OPT_PRE_PARSE | M_OPT_NOPROP,
               ({"cplayer", 0}, {"pseudo-gui", 1})),

    OPT_FLAG("shuffle", shuffle, 0),

// ------------------------- common options --------------------
    OPT_FLAG("quiet", quiet, 0),
    OPT_FLAG("really-quiet", msg_really_quiet, CONF_PRE_PARSE | UPDATE_TERM),
    OPT_FLAG("terminal", use_terminal, CONF_PRE_PARSE | UPDATE_TERM),
    OPT_GENERAL(char**, "msg-level", msg_levels, CONF_PRE_PARSE | UPDATE_TERM,
                .type = &m_option_type_msglevels),
    OPT_STRING("dump-stats", dump_stats, UPDATE_TERM | CONF_PRE_PARSE),
    OPT_FLAG("msg-color", msg_color, CONF_PRE_PARSE | UPDATE_TERM),
    OPT_STRING("log-file", log_file, CONF_PRE_PARSE | M_OPT_FILE | UPDATE_TERM),
    OPT_FLAG("msg-module", msg_module, UPDATE_TERM),
    OPT_FLAG("msg-time", msg_time, UPDATE_TERM),

    OPT_FLAG("config", load_config, M_OPT_FIXED | CONF_PRE_PARSE),
    OPT_STRING("config-dir", force_configdir,
               M_OPT_FIXED | CONF_NOCFG | CONF_PRE_PARSE | M_OPT_FILE),
    OPT_STRINGLIST("reset-on-next-file", reset_options, 0),

// ------------------------- stream options --------------------

    OPT_INTPAIR("chapter", chapterrange, 0, .deprecation_message = "instead of "
        "--chapter=A-B use --start=#A --end=#B+1"),
    OPT_CHOICE_OR_INT("edition", edition_id, 0, 0, 8190,
                      ({"auto", -1})),

// ------------------------- demuxer options --------------------

    OPT_CHOICE_OR_INT("frames", play_frames, 0, 0, INT_MAX, ({"all", -1})),

    OPT_REL_TIME("start", play_start, 0),
    OPT_REL_TIME("end", play_end, 0),
    OPT_REL_TIME("length", play_length, 0),

    OPT_FLAG("rebase-start-time", rebase_start_time, 0),

    OPT_TIME("ab-loop-a", ab_loop[0], 0, .min = MP_NOPTS_VALUE),
    OPT_TIME("ab-loop-b", ab_loop[1], 0, .min = MP_NOPTS_VALUE),

    OPT_CHOICE_OR_INT("playlist-start", playlist_pos, 0, 0, INT_MAX,
                      ({"auto", -1}, {"no", -1})),

    OPT_FLAG("pause", pause, 0),
    OPT_CHOICE("keep-open", keep_open, 0,
               ({"no", 0},
                {"yes", 1},
                {"always", 2})),
    OPT_FLAG("keep-open-pause", keep_open_pause, 0),
    OPT_DOUBLE("image-display-duration", image_display_duration,
               M_OPT_RANGE, 0, INFINITY),

    OPT_CHOICE("index", index_mode, 0, ({"default", 1}, {"recreate", 0})),

    // select audio/video/subtitle stream
    OPT_TRACKCHOICE("aid", stream_id[0][STREAM_AUDIO]),
    OPT_TRACKCHOICE("vid", stream_id[0][STREAM_VIDEO]),
    OPT_TRACKCHOICE("sid", stream_id[0][STREAM_SUB]),
    OPT_TRACKCHOICE("secondary-sid", stream_id[1][STREAM_SUB]),
    OPT_ALIAS("sub", "sid"),
    OPT_ALIAS("video", "vid"),
    OPT_ALIAS("audio", "aid"),
    OPT_STRINGLIST("alang", stream_lang[STREAM_AUDIO], 0),
    OPT_STRINGLIST("slang", stream_lang[STREAM_SUB], 0),
    OPT_STRINGLIST("vlang", stream_lang[STREAM_VIDEO], 0),
    OPT_FLAG("track-auto-selection", stream_auto_sel, 0),

    OPT_STRING("lavfi-complex", lavfi_complex, UPDATE_LAVFI_COMPLEX),

    OPT_CHOICE("audio-display", audio_display, 0,
               ({"no", 0}, {"attachment", 1})),

    OPT_CHOICE_OR_INT("hls-bitrate", hls_bitrate, 0, 0, INT_MAX,
                      ({"no", -1}, {"min", 0}, {"max", INT_MAX})),

    OPT_STRINGLIST("display-tags", display_tags, 0),

    // demuxer.c - select audio/sub file/demuxer
    OPT_PATHLIST("audio-files", audio_files, 0),
    OPT_CLI_ALIAS("audio-file", "audio-files-append"),
    OPT_STRING("demuxer", demuxer_name, 0),
    OPT_STRING("audio-demuxer", audio_demuxer_name, 0),
    OPT_STRING("sub-demuxer", sub_demuxer_name, 0),
    OPT_FLAG("demuxer-thread", demuxer_thread, 0),
    OPT_DOUBLE("demuxer-termination-timeout", demux_termination_timeout, 0),
    OPT_FLAG("prefetch-playlist", prefetch_open, 0),
    OPT_FLAG("cache-pause", cache_pause, 0),
    OPT_FLAG("cache-pause-initial", cache_pause_initial, 0),
    OPT_FLOAT("cache-pause-wait", cache_pause_wait, M_OPT_MIN, .min = 0),

    OPT_DOUBLE("mf-fps", mf_fps, 0),
    OPT_STRING("mf-type", mf_type, 0),
    OPT_SUBSTRUCT("", stream_lavf_opts, stream_lavf_conf, 0),

// ------------------------- a-v sync options --------------------

    // set A-V sync correction speed (0=disables it):
    OPT_FLOATRANGE("mc", default_max_pts_correction, 0, 0, 100),

    // force video/audio rate:
    OPT_DOUBLE("fps", force_fps, CONF_MIN, .min = 0),
    OPT_INTRANGE("audio-samplerate", force_srate, UPDATE_AUDIO, 0, 16*48000),
    OPT_CHANNELS("audio-channels", audio_output_channels, UPDATE_AUDIO),
    OPT_AUDIOFORMAT("audio-format", audio_output_format, UPDATE_AUDIO),
    OPT_DOUBLE("speed", playback_speed, M_OPT_RANGE, .min = 0.01, .max = 100.0),

    OPT_FLAG("audio-pitch-correction", pitch_correction, 0),

    // set a-v distance
    OPT_FLOAT("audio-delay", audio_delay, 0),

// ------------------------- codec/vfilter options --------------------

    OPT_SETTINGSLIST("af-defaults", af_defs, 0, &af_obj_list,
                     .deprecation_message = "use --af + enable/disable flags"),
    OPT_SETTINGSLIST("af", af_settings, 0, &af_obj_list, ),

    OPT_STRING("ad", audio_decoders, 0),

    OPT_STRING("audio-spdif", audio_spdif, 0),

    OPT_SUBSTRUCT("ad-lavc", ad_lavc_params, ad_lavc_conf, 0),

    OPT_SUBSTRUCT("", demux_lavf, demux_lavf_conf, 0),
    OPT_SUBSTRUCT("demuxer-rawaudio", demux_rawaudio, demux_rawaudio_conf, 0),

//---------------------- libao/libvo options ------------------------
    OPT_SUBSTRUCT("", ao_opts, ao_conf, 0),
    OPT_FLAG("audio-exclusive", audio_exclusive, UPDATE_AUDIO),
    OPT_FLAG("audio-fallback-to-null", ao_null_fallback, 0),
    OPT_FLAG("audio-stream-silence", audio_stream_silence, 0),
    OPT_FLOATRANGE("audio-wait-open", audio_wait_open, 0, 0, 60),

    OPT_FLOATRANGE("volume-max", softvol_max, 0, 100, 1000),
    // values <0 for volume and mute are legacy and ignored
    OPT_FLOATRANGE("volume", softvol_volume, UPDATE_VOL, -1, 1000),
    OPT_CHOICE("mute", softvol_mute, UPDATE_VOL,
               ({"no", 0},
                {"auto", 0},
                {"yes", 1})),
    OPT_CHOICE("replaygain", rgain_mode, UPDATE_VOL,
               ({"no", 0},
                {"track", 1},
                {"album", 2})),
    OPT_FLOATRANGE("replaygain-preamp", rgain_preamp, UPDATE_VOL, -15, 15),
    OPT_FLAG("replaygain-clip", rgain_clip, UPDATE_VOL),
    OPT_FLOATRANGE("replaygain-fallback", rgain_fallback, UPDATE_VOL, -200, 60),
    OPT_CHOICE("gapless-audio", gapless_audio, 0,
               ({"no", 0},
                {"yes", 1},
                {"weak", -1})),

    OPT_CHOICE("osd-level", osd_level, 0,
               ({"0", 0}, {"1", 1}, {"2", 2}, {"3", 3})),
    OPT_INTRANGE("osd-duration", osd_duration, 0, 0, 3600000),
    OPT_FLAG("osd-fractions", osd_fractions, 0),

    OPT_DOUBLE("sstep", step_sec, CONF_MIN, 0),

    OPT_CHOICE("framedrop", frame_dropping, 0,
               ({"no", 0},
                {"vo", 1},
                {"decoder", 2},
                {"decoder+vo", 3})),
    OPT_FLAG("video-latency-hacks", video_latency_hacks, 0),

    OPT_FLAG("untimed", untimed, 0),

    OPT_STRING("stream-dump", stream_dump, M_OPT_FILE),

    OPT_FLAG("stop-playback-on-init-failure", stop_playback_on_init_failure, 0),

    OPT_CHOICE_OR_INT("loop-playlist", loop_times, 0, 1, 10000,
                      ({"no", 1},
                       {"inf", -1}, {"yes", -1},
                       {"force", -2})),
    OPT_CHOICE_OR_INT("loop-file", loop_file, 0, 0, 10000,
                      ({"no", 0},
                       {"yes", -1},
                       {"inf", -1})),
    OPT_ALIAS("loop", "loop-file"),

    OPT_FLAG("resume-playback", position_resume, 0),
    OPT_FLAG("save-position-on-quit", position_save_on_quit, 0),
    OPT_FLAG("write-filename-in-watch-later-config", write_filename_in_watch_later_config, 0),
    OPT_FLAG("ignore-path-in-watch-later-config", ignore_path_in_watch_later_config, 0),
    OPT_STRING("watch-later-directory", watch_later_directory, M_OPT_FILE),

    OPT_FLAG("ordered-chapters", ordered_chapters, 0),
    OPT_STRING("ordered-chapters-files", ordered_chapters_files, M_OPT_FILE),
    OPT_INTRANGE("chapter-merge-threshold", chapter_merge_threshold, 0, 0, 10000),

    OPT_DOUBLE("chapter-seek-threshold", chapter_seek_threshold, 0),

    OPT_STRING("chapters-file", chapter_file, M_OPT_FILE),

    OPT_FLAG("load-unsafe-playlists", load_unsafe_playlists, 0),
    OPT_FLAG("merge-files", merge_files, 0),

    OPT_CHOICE("term-osd", term_osd, 0,
               ({"force", 1},
                {"auto", 2},
                {"no", 0})),

    OPT_FLAG("term-osd-bar", term_osd_bar, 0),
    OPT_STRING("term-osd-bar-chars", term_osd_bar_chars, 0),

    OPT_STRING("term-playing-msg", playing_msg, 0),
    OPT_STRING("osd-playing-msg", osd_playing_msg, 0),
    OPT_STRING("term-status-msg", status_msg, 0),

    OPT_CHOICE("idle", player_idle_mode, 0,
               ({"no",   0},
                {"once", 1},
                {"yes",  2})),

    OPT_FLAG("input-terminal", consolecontrols, UPDATE_TERM),

    OPT_STRING("input-file", input_file, M_OPT_FILE | UPDATE_INPUT),
    OPT_STRING("input-ipc-server", ipc_path, M_OPT_FILE | UPDATE_INPUT),

    OPT_SUBSTRUCT("", resample_opts, resample_conf, 0),
    OPT_SUBSTRUCT("", input_opts, input_config, 0),
    OPT_SUBSTRUCT("", demux_opts, demux_conf, 0),

    OPT_REMOVED("a52drc", "use --ad-lavc-ac3drc=level"),
    OPT_REMOVED("afm", "use --ad=..."),
    OPT_REPLACED("audiofile", "audio-file"),
    OPT_REMOVED("benchmark", "use --untimed (no stats)"),
    OPT_REMOVED("channels", "use --audio-channels (changed semantics)"),
    OPT_REPLACED("delay", "audio-delay"),
    OPT_REPLACED("endpos", "length"),
    OPT_REPLACED("format", "audio-format"),
    OPT_REMOVED("hardframedrop", NULL),
    OPT_REMOVED("identify", "use TOOLS/mpv_identify.sh"),
    OPT_REMOVED("lavfdopts", "use --demuxer-lavf-..."),
    OPT_REMOVED("mixer-channel", "use AO suboptions (alsa, oss)"),
    OPT_REMOVED("mixer", "use AO suboptions (alsa, oss)"),
    OPT_REPLACED("msgcolor", "msg-color"),
    OPT_REMOVED("msglevel", "use --msg-level (changed semantics)"),
    OPT_REPLACED("msgmodule", "msg-module"),
    OPT_REPLACED("noconsolecontrols", "no-input-terminal"),
    OPT_REPLACED("nosound", "no-audio"),
    OPT_REPLACED("osdlevel", "osd-level"),
    OPT_REPLACED("playing-msg", "term-playing-msg"),
    OPT_REMOVED("pp", NULL),
    OPT_REMOVED("pphelp", NULL),
    OPT_REMOVED("rawaudio", "use --demuxer-rawaudio-..."),
    OPT_REPLACED("srate", "audio-samplerate"),
    OPT_REPLACED("ss", "start"),
    OPT_REMOVED("use-filename-title", "use --title='${filename}'"),
    OPT_REPLACED("right-alt-gr", "input-right-alt-gr"),
    OPT_REPLACED("autosub", "sub-auto"),
    OPT_REPLACED("autosub-match", "sub-auto"),
    OPT_REPLACED("status-msg", "term-status-msg"),
    OPT_REPLACED("idx", "index"),
    OPT_REPLACED("forceidx", "index"),
    OPT_REMOVED("cache-pause-below", "for 'no', use --no-cache-pause"),
    OPT_REMOVED("no-cache-pause-below", "use --no-cache-pause"),
    OPT_REMOVED("volstep", "edit input.conf directly instead"),
    OPT_REPLACED("media-title", "force-media-title"),
    OPT_REPLACED("softvol-max", "volume-max"),
    OPT_REPLACED("playlist-pos", "playlist-start"),
    OPT_REMOVED("fs-black-out-screens", NULL),
    OPT_REMOVED("no-ometadata", "use --no-ocopy-metadata"),

    {0}
};

const struct MPOpts mp_default_opts = {
    .use_terminal = 1,
    .msg_color = 1,
    .audio_decoders = NULL,
    .softvol_max = 130,
    .softvol_volume = 100,
    .softvol_mute = 0,
    .gapless_audio = -1,
    .osd_level = 1,
    .osd_duration = 1000,
    .loop_times = 1,
    .ordered_chapters = 1,
    .chapter_merge_threshold = 100,
    .chapter_seek_threshold = 5.0,
    .hr_seek_framedrop = 1,
    .sync_max_video_change = 1,
    .sync_max_audio_change = 0.125,
    .sync_audio_drop_size = 0.020,
    .load_config = 1,
    .position_resume = 1,
    .autoload_files = 1,
    .demuxer_thread = 1,
    .demux_termination_timeout = 0.1,
    .hls_bitrate = INT_MAX,
    .cache_pause = 1,
    .cache_pause_wait = 1.0,
    .chapterrange = {-1, -1},
    .ab_loop = {MP_NOPTS_VALUE, MP_NOPTS_VALUE},
    .edition_id = -1,
    .default_max_pts_correction = -1,
    .correct_pts = 1,
    .initial_audio_sync = 1,
    .frame_dropping = 1,
    .term_osd = 2,
    .term_osd_bar_chars = "[-+-]",
    .consolecontrols = 1,
    .playlist_pos = -1,
    .play_frames = -1,
    .rebase_start_time = 1,
    .keep_open = 0,
    .keep_open_pause = 1,
    .image_display_duration = 1.0,
    .stream_id = { { [STREAM_AUDIO] = -1,
                     [STREAM_VIDEO] = -2,  // Default = Disable
                     [STREAM_SUB] = -2, }, // Default = Disable
                   { [STREAM_AUDIO] = -2,
                     [STREAM_VIDEO] = -2,
                     [STREAM_SUB] = -2, }, },
    .stream_auto_sel = 1,
    .audio_display = 1,
    .audio_output_format = 0,  // AF_FORMAT_UNKNOWN
    .playback_speed = 1.,
    .pitch_correction = 1,
    .movie_aspect = -1.,
    .aspect_method = 2,
    .audiofile_auto = -1,

    .audio_output_channels = {
        .set = 1,
        .auto_safe = 1,
    },

    .index_mode = 1,

    .mf_fps = 1.0,

    .display_tags = (char **)(const char*[]){
        "Artist", "Album", "Album_Artist", "Comment", "Composer",
        "Date", "Description", "Genre", "Performer", "Rating",
        "Series", "Title", "Track", "icy-title", "service_name",
        NULL
    },

    .cuda_device = -1,
};

#endif /* MPLAYER_CFG_MPLAYER_H */
