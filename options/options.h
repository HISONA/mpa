#ifndef MPLAYER_OPTIONS_H
#define MPLAYER_OPTIONS_H

#include <stdbool.h>
#include <stdint.h>
#include "m_option.h"
#include "common/common.h"


typedef struct MPOpts {
    int property_print_help;
    int use_terminal;
    char *dump_stats;
    int verbose;
    int msg_really_quiet;
    char **msg_levels;
    int msg_color;
    int msg_module;
    int msg_time;
    char *log_file;

    int operation_mode;

    char **reset_options;

    int audio_exclusive;
    int ao_null_fallback;
    int audio_stream_silence;
    float audio_wait_open;
    float softvol_volume;
    int rgain_mode;
    float rgain_preamp;         // Set replaygain pre-amplification
    int rgain_clip;             // Enable/disable clipping prevention
    float rgain_fallback;
    int softvol_mute;
    float softvol_max;
    int gapless_audio;

    struct ao_opts *ao_opts;

    char *media_title;

    char *audio_decoders;
    char *audio_spdif;

    int osd_level;
    int osd_duration;
    int osd_fractions;

    int untimed;
    char *stream_dump;
    int stop_playback_on_init_failure;
    int loop_times;
    int loop_file;
    int shuffle;
    int ordered_chapters;
    char *ordered_chapters_files;
    int chapter_merge_threshold;
    double chapter_seek_threshold;
    char *chapter_file;
    int load_unsafe_playlists;
    int merge_files;
    int quiet;
    int load_config;
    char *force_configdir;
    int use_filedir_conf;
    int hls_bitrate;
    int chapterrange[2];
    int edition_id;
    int correct_pts;
    int initial_audio_sync;
    int video_sync;
    double sync_max_video_change;
    double sync_max_audio_change;
    double sync_audio_drop_size;
    int hr_seek;
    float hr_seek_demuxer_offset;
    int hr_seek_framedrop;
    float audio_delay;
    float default_max_pts_correction;
    int autosync;
    int frame_dropping;
    int video_latency_hacks;
    int term_osd;
    int term_osd_bar;
    char *term_osd_bar_chars;
    char *playing_msg;
    char *osd_playing_msg;
    char *status_msg;
    int player_idle_mode;
    int consolecontrols;
    int playlist_pos;
    struct m_rel_time play_start;
    struct m_rel_time play_end;
    struct m_rel_time play_length;
    int rebase_start_time;
    int play_frames;
    double ab_loop[2];
    double step_sec;
    int position_resume;
    int position_save_on_quit;
    int write_filename_in_watch_later_config;
    int ignore_path_in_watch_later_config;
    char *watch_later_directory;
    int pause;
    int keep_open;
    int keep_open_pause;
    double image_display_duration;
    char *lavfi_complex;
    int stream_id[2][STREAM_TYPE_COUNT];
    char **stream_lang[STREAM_TYPE_COUNT];
    int stream_auto_sel;
    int audio_display;
    char **display_tags;

    char **audio_files;
    char *demuxer_name;
    int demuxer_thread;
    double demux_termination_timeout;
    int prefetch_open;
    char *audio_demuxer_name;
    char *sub_demuxer_name;

    int cache_pause;
    int cache_pause_initial;
    float cache_pause_wait;

    double force_fps;
    int index_mode;

    struct m_channels audio_output_channels;
    int audio_output_format;
    int force_srate;
    double playback_speed;
    int pitch_correction;
    struct m_obj_settings *vf_settings, *vf_defs;
    struct m_obj_settings *af_settings, *af_defs;
    struct filter_opts *filter_opts;
    float movie_aspect;
    int aspect_method;
    char **audiofile_paths;
    char **external_files;
    int autoload_files;
    int audiofile_auto;

    int w32_priority;

    struct stream_lavf_params *stream_lavf_opts;

    double mf_fps;
    char *mf_type;

    struct demux_rawaudio_opts *demux_rawaudio;
    struct demux_lavf_opts *demux_lavf;

    struct demux_opts *demux_opts;

    struct vd_lavc_params *vd_lavc_params;
    struct ad_lavc_params *ad_lavc_params;

    struct input_opts *input_opts;

    char *ipc_path;
    char *input_file;

    struct mp_resample_opts *resample_opts;

    int cuda_device;

} MPOpts;

extern const m_option_t mp_opts[];
extern const struct MPOpts mp_default_opts;
extern const struct m_sub_options filter_conf;
extern const struct m_sub_options resample_conf;

#endif
