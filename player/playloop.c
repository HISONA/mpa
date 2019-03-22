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

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include <assert.h>

#include "config.h"
#include "mpa_talloc.h"

#include "common/msg.h"
#include "options/options.h"
#include "common/common.h"
#include "filters/f_decoder_wrapper.h"
#include "options/m_config.h"
#include "options/m_property.h"
#include "common/playlist.h"
#include "input/input.h"

#include "misc/dispatch.h"
#include "osdep/terminal.h"
#include "osdep/timer.h"

#include "audio/out/ao.h"
#include "demux/demux.h"
#include "stream/stream.h"
//#include "video/out/vo.h"

#include "core.h"
#include "client.h"
#include "command.h"
#include "osd.h"

// Wait until mp_wakeup_core() is called, since the last time
// mp_wait_events() was called.
void mp_wait_events(struct MPContext *mpctx)
{
    bool sleeping = mpctx->sleeptime > 0;
    if (sleeping)
        MP_STATS(mpctx, "start sleep");

    mp_dispatch_queue_process(mpctx->dispatch, mpctx->sleeptime);

    mpctx->sleeptime = INFINITY;

    if (sleeping)
        MP_STATS(mpctx, "end sleep");
}

// Set the timeout used when the playloop goes to sleep. This means the
// playloop will re-run as soon as the timeout elapses (or earlier).
// mp_set_timeout(c, 0) is essentially equivalent to mp_wakeup_core(c).
void mp_set_timeout(struct MPContext *mpctx, double sleeptime)
{
    if (mpctx->sleeptime > sleeptime) {
        mpctx->sleeptime = sleeptime;
        int64_t abstime = mp_add_timeout(mp_time_us(), sleeptime);
        mp_dispatch_adjust_timeout(mpctx->dispatch, abstime);
    }
}

// Cause the playloop to run. This can be called from any thread. If called
// from within the playloop itself, it will be run immediately again, instead
// of going to sleep in the next mp_wait_events().
void mp_wakeup_core(struct MPContext *mpctx)
{
    mp_dispatch_interrupt(mpctx->dispatch);
}

// Opaque callback variant of mp_wakeup_core().
void mp_wakeup_core_cb(void *ctx)
{
    struct MPContext *mpctx = ctx;
    mp_wakeup_core(mpctx);
}

void mp_core_lock(struct MPContext *mpctx)
{
    mp_dispatch_lock(mpctx->dispatch);
}

void mp_core_unlock(struct MPContext *mpctx)
{
    mp_dispatch_unlock(mpctx->dispatch);
}

// Process any queued input, whether it's user input, or requests from client
// API threads. This also resets the "wakeup" flag used with mp_wait_events().
void mp_process_input(struct MPContext *mpctx)
{
    for (;;) {
        mp_cmd_t *cmd = mp_input_read_cmd(mpctx->input);
        if (!cmd)
            break;
        run_command(mpctx, cmd, NULL, NULL, NULL);
    }
    mp_set_timeout(mpctx, mp_input_get_delay(mpctx->input));
}

double get_relative_time(struct MPContext *mpctx)
{
    int64_t new_time = mp_time_us();
    int64_t delta = new_time - mpctx->last_time;
    mpctx->last_time = new_time;
    return delta * 0.000001;
}

void update_core_idle_state(struct MPContext *mpctx)
{
    bool eof = (mpctx->audio_status == STATUS_EOF);
    bool active = !mpctx->paused && mpctx->restart_complete &&
                  !mpctx->stop_play && mpctx->in_playloop && !eof;

    if (mpctx->playback_active != active) {
        mpctx->playback_active = active;
        mp_notify(mpctx, MP_EVENT_CORE_IDLE, NULL);
    }
}

// The value passed here is the new value for mpctx->opts->pause
void set_pause_state(struct MPContext *mpctx, bool user_pause)
{
    struct MPOpts *opts = mpctx->opts;
    bool send_update = false;

    if (opts->pause != user_pause)
        send_update = true;
    opts->pause = user_pause;

    bool internal_paused = opts->pause || mpctx->paused_for_cache;
    if (internal_paused != mpctx->paused) {
        mpctx->paused = internal_paused;
        send_update = true;

        if (mpctx->ao && mpctx->ao_chain) {
            if (internal_paused) {
                ao_pause(mpctx->ao);
            } else {
                ao_resume(mpctx->ao);
            }
        }

// HISONA ...
//        if (mpctx->video_out)
//            vo_set_paused(mpctx->video_out, internal_paused);

//        mpctx->osd_function = 0;
//        mpctx->osd_force_update = true;

        mp_wakeup_core(mpctx);

        if (internal_paused) {
            mpctx->step_frames = 0;
            mpctx->time_frame -= get_relative_time(mpctx);
        } else {
            (void)get_relative_time(mpctx); // ignore time that passed during pause
        }
    }

    update_core_idle_state(mpctx);

    if (send_update)
        mp_notify(mpctx, opts->pause ? MPV_EVENT_PAUSE : MPV_EVENT_UNPAUSE, 0);
}

void update_internal_pause_state(struct MPContext *mpctx)
{
    set_pause_state(mpctx, mpctx->opts->pause);
}

void add_step_frame(struct MPContext *mpctx, int dir)
{
    if (!mpctx->vo_chain)
        return;
    if (dir > 0) {
        mpctx->step_frames += 1;
        set_pause_state(mpctx, false);
    } else if (dir < 0) {
        if (!mpctx->hrseek_active) {
            queue_seek(mpctx, MPSEEK_BACKSTEP, 0, MPSEEK_VERY_EXACT, 0);
            set_pause_state(mpctx, true);
        }
    }
}

// Clear some playback-related fields on file loading or after seeks.
void reset_playback_state(struct MPContext *mpctx)
{
    mp_filter_reset(mpctx->filter_root);

    reset_audio_state(mpctx);

    mpctx->hrseek_active = false;
    mpctx->hrseek_lastframe = false;
    mpctx->hrseek_backstep = false;
    mpctx->current_seek = (struct seek_params){0};
    mpctx->playback_pts = MP_NOPTS_VALUE;
    mpctx->step_frames = 0;
    mpctx->ab_loop_clip = true;
    mpctx->restart_complete = false;
    mpctx->paused_for_cache = false;
    mpctx->cache_buffer = 100;
    mpctx->seek_slave = NULL;

    update_internal_pause_state(mpctx);
    update_core_idle_state(mpctx);
}

static void mp_seek(MPContext *mpctx, struct seek_params seek)
{
    struct MPOpts *opts = mpctx->opts;

    if (!mpctx->demuxer || !seek.type || seek.amount == MP_NOPTS_VALUE)
        return;

    bool hr_seek_very_exact = seek.exact == MPSEEK_VERY_EXACT;
    double current_time = get_current_time(mpctx);
    if (current_time == MP_NOPTS_VALUE && seek.type == MPSEEK_RELATIVE)
        return;
    if (current_time == MP_NOPTS_VALUE)
        current_time = 0;
    double seek_pts = MP_NOPTS_VALUE;
    int demux_flags = 0;

    switch (seek.type) {
    case MPSEEK_ABSOLUTE:
        seek_pts = seek.amount;
        break;
    case MPSEEK_BACKSTEP:
        seek_pts = current_time;
        hr_seek_very_exact = true;
        break;
    case MPSEEK_RELATIVE:
        demux_flags = seek.amount > 0 ? SEEK_FORWARD : 0;
        seek_pts = current_time + seek.amount;
        break;
    case MPSEEK_FACTOR: ;
        double len = get_time_length(mpctx);
        if (len >= 0)
            seek_pts = seek.amount * len;
        break;
    default: abort();
    }

    double demux_pts = seek_pts;

    bool hr_seek = opts->correct_pts && seek.exact != MPSEEK_KEYFRAME &&
                 ((opts->hr_seek == 0 && seek.type == MPSEEK_ABSOLUTE) ||
                  opts->hr_seek > 0 || seek.exact >= MPSEEK_EXACT) &&
                 seek_pts != MP_NOPTS_VALUE;

    if (seek.type == MPSEEK_FACTOR || seek.amount < 0 ||
        (seek.type == MPSEEK_ABSOLUTE && seek.amount < mpctx->last_chapter_pts))
        mpctx->last_chapter_seek = -2;

    // Under certain circumstances, prefer SEEK_FACTOR.
    if (seek.type == MPSEEK_FACTOR && !hr_seek &&
        (mpctx->demuxer->ts_resets_possible || seek_pts == MP_NOPTS_VALUE))
    {
        demux_pts = seek.amount;
        demux_flags |= SEEK_FACTOR;
    }

    if (hr_seek) {
        double hr_seek_offset = opts->hr_seek_demuxer_offset;
        // Always try to compensate for possibly bad demuxers in "special"
        // situations where we need more robustness from the hr-seek code, even
        // if the user doesn't use --hr-seek-demuxer-offset.
        // The value is arbitrary, but should be "good enough" in most situations.
        if (hr_seek_very_exact)
            hr_seek_offset = MPMAX(hr_seek_offset, 0.5); // arbitrary
        for (int n = 0; n < mpctx->num_tracks; n++) {
            double offset = 0;
            if (!mpctx->tracks[n]->is_external)
                offset += get_track_seek_offset(mpctx, mpctx->tracks[n]);
            hr_seek_offset = MPMAX(hr_seek_offset, -offset);
        }
        demux_pts -= hr_seek_offset;
        demux_flags = (demux_flags | SEEK_HR) & ~SEEK_FORWARD;
    }

    if (!mpctx->demuxer->seekable)
        demux_flags |= SEEK_CACHED;

    if (!demux_seek(mpctx->demuxer, demux_pts, demux_flags)) {
        if (!mpctx->demuxer->seekable) {
            MP_ERR(mpctx, "Cannot seek in this stream.\n");
            MP_ERR(mpctx, "You can force it with '--force-seekable=yes'.\n");
        }
        return;
    }

    // Seek external, extra files too:
    bool has_video = false;
    struct track *external_audio = NULL;
    for (int t = 0; t < mpctx->num_tracks; t++) {
        struct track *track = mpctx->tracks[t];
        if (track->selected && track->is_external && track->demuxer) {
            double main_new_pos = demux_pts;
            if (!hr_seek || track->is_external)
                main_new_pos += get_track_seek_offset(mpctx, track);
            if (demux_flags & SEEK_FACTOR)
                main_new_pos = seek_pts;
            demux_seek(track->demuxer, main_new_pos, 0);
            if (track->type == STREAM_AUDIO && !external_audio)
                external_audio = track;
        }
        if (track->selected && !track->is_external && track->stream &&
            track->type == STREAM_VIDEO && !track->stream->attached_picture)
            has_video = true;
    }

    if (!(seek.flags & MPSEEK_FLAG_NOFLUSH))
        clear_audio_output_buffers(mpctx);

    reset_playback_state(mpctx);

    // When doing keyframe seeks (hr_seek=false) backwards (no SEEK_FORWARD),
    // then video can seek before the external audio track (because video seek
    // granularity is coarser than audio). The result would be playing video with
    // silence until the audio seek target is reached. Work around by blocking
    // the demuxer (decoders can't read) and seeking to video position later.
    if (has_video && external_audio && !hr_seek && !(demux_flags & SEEK_FORWARD)) {
        MP_VERBOSE(mpctx, "delayed seek for aid=%d\n", external_audio->user_tid);
        demux_block_reading(external_audio->demuxer, true);
        mpctx->seek_slave = external_audio;
    }

    /* Use the target time as "current position" for further relative
     * seeks etc until a new video frame has been decoded */
    mpctx->last_seek_pts = seek_pts;

    if (hr_seek) {
        mpctx->hrseek_active = true;
        mpctx->hrseek_backstep = seek.type == MPSEEK_BACKSTEP;
        mpctx->hrseek_pts = seek_pts;

        // allow decoder to drop frames before hrseek_pts
        bool hrseek_framedrop = !hr_seek_very_exact && opts->hr_seek_framedrop;

        MP_VERBOSE(mpctx, "hr-seek, skipping to %f%s%s\n", mpctx->hrseek_pts,
                   hrseek_framedrop ? "" : " (no framedrop)",
                   mpctx->hrseek_backstep ? " (backstep)" : "");

        for (int n = 0; n < mpctx->num_tracks; n++) {
            struct track *track = mpctx->tracks[n];
            struct mp_decoder_wrapper *dec = track->dec;
            if (dec && hrseek_framedrop)
                mp_decoder_wrapper_set_start_pts(dec, mpctx->hrseek_pts);
        }
    }

    if (mpctx->stop_play == AT_END_OF_FILE)
        mpctx->stop_play = KEEP_PLAYING;

    mpctx->start_timestamp = mp_time_sec();
    mp_wakeup_core(mpctx);

    mp_notify(mpctx, MPV_EVENT_SEEK, NULL);
    mp_notify(mpctx, MPV_EVENT_TICK, NULL);

    mpctx->ab_loop_clip = mpctx->last_seek_pts < opts->ab_loop[1];

    mpctx->current_seek = seek;
}

// This combines consecutive seek requests.
void queue_seek(struct MPContext *mpctx, enum seek_type type, double amount,
                enum seek_precision exact, int flags)
{
    struct seek_params *seek = &mpctx->seek;

    mp_wakeup_core(mpctx);

    if (mpctx->stop_play == AT_END_OF_FILE)
        mpctx->stop_play = KEEP_PLAYING;

    switch (type) {
    case MPSEEK_RELATIVE:
        seek->flags |= flags;
        if (seek->type == MPSEEK_FACTOR)
            return;  // Well... not common enough to bother doing better
        seek->amount += amount;
        seek->exact = MPMAX(seek->exact, exact);
        if (seek->type == MPSEEK_NONE)
            seek->exact = exact;
        if (seek->type == MPSEEK_ABSOLUTE)
            return;
        seek->type = MPSEEK_RELATIVE;
        return;
    case MPSEEK_ABSOLUTE:
    case MPSEEK_FACTOR:
    case MPSEEK_BACKSTEP:
        *seek = (struct seek_params) {
            .type = type,
            .amount = amount,
            .exact = exact,
            .flags = flags,
        };
        return;
    case MPSEEK_NONE:
        *seek = (struct seek_params){ 0 };
        return;
    }
    abort();
}

void execute_queued_seek(struct MPContext *mpctx)
{
    if (mpctx->seek.type) {
        // Let explicitly imprecise seeks cancel precise seeks:
        if (mpctx->hrseek_active && mpctx->seek.exact == MPSEEK_KEYFRAME)
            mpctx->start_timestamp = -1e9;
        /* If the user seeks continuously (keeps arrow key down)
         * try to finish showing a frame from one location before doing
         * another seek (which could lead to unchanging display). */
        mp_seek(mpctx, mpctx->seek);
        mpctx->seek = (struct seek_params){0};
    }
}

// NOPTS (i.e. <0) if unknown
double get_time_length(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    return demuxer && demuxer->duration >= 0 ? demuxer->duration : MP_NOPTS_VALUE;
}

double get_current_time(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (demuxer) {
        if (mpctx->playback_pts != MP_NOPTS_VALUE)
            return mpctx->playback_pts;
        if (mpctx->last_seek_pts != MP_NOPTS_VALUE)
            return mpctx->last_seek_pts;
    }
    return MP_NOPTS_VALUE;
}

double get_playback_time(struct MPContext *mpctx)
{
    double cur = get_current_time(mpctx);
    if (cur == MP_NOPTS_VALUE)
        return cur;
    // During seeking, the time corresponds to the last seek time - apply some
    // cosmetics to it.
    if (mpctx->playback_pts == MP_NOPTS_VALUE) {
        double length = get_time_length(mpctx);
        if (length >= 0)
            cur = MPCLAMP(cur, 0, length);
    }
    return cur;
}

// Return playback position in 0.0-1.0 ratio, or -1 if unknown.
double get_current_pos_ratio(struct MPContext *mpctx, bool use_range)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return -1;
    double ans = -1;
    double start = 0;
    double len = get_time_length(mpctx);
    if (use_range) {
        double startpos = get_play_start_pts(mpctx);
        double endpos = get_play_end_pts(mpctx);
        if (endpos == MP_NOPTS_VALUE || endpos > MPMAX(0, len))
            endpos = MPMAX(0, len);
        if (startpos == MP_NOPTS_VALUE || startpos < 0)
            startpos = 0;
        if (endpos < startpos)
            endpos = startpos;
        start = startpos;
        len = endpos - startpos;
    }
    double pos = get_current_time(mpctx);
    if (len > 0)
        ans = MPCLAMP((pos - start) / len, 0, 1);
    if (ans < 0 || demuxer->ts_resets_possible) {
        int64_t size;
        if (demux_stream_control(demuxer, STREAM_CTRL_GET_SIZE, &size) > 0) {
            if (size > 0 && demuxer->filepos >= 0)
                ans = MPCLAMP(demuxer->filepos / (double)size, 0, 1);
        }
    }
    if (use_range) {
        if (mpctx->opts->play_frames > 0)
            ans = MPMAX(ans, 1.0 -
                    mpctx->max_frames / (double) mpctx->opts->play_frames);
    }
    return ans;
}

// 0-100, -1 if unknown
int get_percent_pos(struct MPContext *mpctx)
{
    double pos = get_current_pos_ratio(mpctx, false);
    return pos < 0 ? -1 : pos * 100;
}

// -2 is no chapters, -1 is before first chapter
int get_current_chapter(struct MPContext *mpctx)
{
    if (!mpctx->num_chapters)
        return -2;
    double current_pts = get_current_time(mpctx);
    int i;
    for (i = 0; i < mpctx->num_chapters; i++)
        if (current_pts < mpctx->chapters[i].pts)
            break;
    return MPMAX(mpctx->last_chapter_seek, i - 1);
}

char *chapter_display_name(struct MPContext *mpctx, int chapter)
{
    char *name = chapter_name(mpctx, chapter);
    char *dname = NULL;
    if (name) {
        dname = talloc_asprintf(NULL, "(%d) %s", chapter + 1, name);
    } else if (chapter < -1) {
        dname = talloc_strdup(NULL, "(unavailable)");
    } else {
        int chapter_count = get_chapter_count(mpctx);
        if (chapter_count <= 0)
            dname = talloc_asprintf(NULL, "(%d)", chapter + 1);
        else
            dname = talloc_asprintf(NULL, "(%d) of %d", chapter + 1,
                                    chapter_count);
    }
    return dname;
}

// returns NULL if chapter name unavailable
char *chapter_name(struct MPContext *mpctx, int chapter)
{
    if (chapter < 0 || chapter >= mpctx->num_chapters)
        return NULL;
    return mp_tags_get_str(mpctx->chapters[chapter].metadata, "title");
}

// returns the start of the chapter in seconds (NOPTS if unavailable)
double chapter_start_time(struct MPContext *mpctx, int chapter)
{
    if (chapter == -1)
        return 0;
    if (chapter >= 0 && chapter < mpctx->num_chapters)
        return mpctx->chapters[chapter].pts;
    return MP_NOPTS_VALUE;
}

int get_chapter_count(struct MPContext *mpctx)
{
    return mpctx->num_chapters;
}

static void handle_pause_on_low_cache(struct MPContext *mpctx)
{
    bool force_update = false;
    struct MPOpts *opts = mpctx->opts;
    if (!mpctx->demuxer)
        return;

    double now = mp_time_sec();

    struct demux_ctrl_reader_state s = {.idle = true, .ts_duration = -1};
    demux_control(mpctx->demuxer, DEMUXER_CTRL_GET_READER_STATE, &s);

    int cache_buffer = 100;
    bool use_pause_on_low_cache = demux_is_network_cached(mpctx->demuxer) &&
                                  opts->cache_pause;

    if (!mpctx->restart_complete) {
        // Audio or video is restarting, and initial buffering is enabled. Make
        // sure we actually restart them in paused mode, so no audio gets
        // dropped and video technically doesn't start yet.
        use_pause_on_low_cache &= opts->cache_pause_initial &&
                                  mpctx->audio_status == STATUS_READY;
    }

    bool is_low = use_pause_on_low_cache && !s.idle &&
                  s.ts_duration < opts->cache_pause_wait;

    // Enter buffering state only if there actually was an underrun (or if
    // initial caching before playback restart is used).
    if (is_low && !mpctx->paused_for_cache && mpctx->restart_complete)
        is_low = s.underrun;

    if (mpctx->paused_for_cache != is_low) {
        mpctx->paused_for_cache = is_low;
        update_internal_pause_state(mpctx);
        force_update = true;
        if (is_low)
            mpctx->cache_stop_time = now;
    }

    if (mpctx->paused_for_cache) {
        cache_buffer =
            100 * MPCLAMP(s.ts_duration / opts->cache_pause_wait, 0, 0.99);
        mp_set_timeout(mpctx, 0.2);
    }

    // Also update cache properties.
    bool busy = !s.idle;
    if (busy || mpctx->next_cache_update > 0) {
        if (mpctx->next_cache_update <= now) {
            mpctx->next_cache_update = busy ? now + 0.25 : 0;
            force_update = true;
        }
        if (mpctx->next_cache_update > 0)
            mp_set_timeout(mpctx, mpctx->next_cache_update - now);
    }

    if (mpctx->cache_buffer != cache_buffer) {
        if ((mpctx->cache_buffer == 100) != (cache_buffer == 100)) {
            if (cache_buffer < 100) {
                MP_VERBOSE(mpctx, "Enter buffering (buffer went from %d%% -> %d%%) [%fs].\n",
                           mpctx->cache_buffer, cache_buffer, s.ts_duration);
            } else {
                double t = now - mpctx->cache_stop_time;
                MP_VERBOSE(mpctx, "End buffering (waited %f secs) [%fs].\n",
                           t, s.ts_duration);
            }
        } else {
            MP_VERBOSE(mpctx, "Still buffering (buffer went from %d%% -> %d%%) [%fs].\n",
                       mpctx->cache_buffer, cache_buffer, s.ts_duration);
        }
        mpctx->cache_buffer = cache_buffer;
        force_update = true;
    }

    if (s.eof && !busy)
        prefetch_next(mpctx);

    if (force_update)
        mp_notify(mpctx, MP_EVENT_CACHE_UPDATE, NULL);
}

int get_cache_buffering_percentage(struct MPContext *mpctx)
{
    return mpctx->demuxer ? mpctx->cache_buffer : -1;
}

static void handle_sstep(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (mpctx->stop_play || !mpctx->restart_complete)
        return;

    if (opts->step_sec > 0 && !mpctx->paused) {
        set_osd_function(mpctx, OSD_FFW);
        queue_seek(mpctx, MPSEEK_RELATIVE, opts->step_sec, MPSEEK_DEFAULT, 0);
    }

    if (mpctx->max_frames >= 0 && !mpctx->stop_play)
        mpctx->stop_play = AT_END_OF_FILE; // force EOF even if audio left
    if (mpctx->step_frames > 0 && !mpctx->paused)
        set_pause_state(mpctx, true);
}

static void handle_loop_file(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    if (mpctx->stop_play == AT_END_OF_FILE &&
        (opts->ab_loop[0] != MP_NOPTS_VALUE || opts->ab_loop[1] != MP_NOPTS_VALUE))
    {
        // Assumes execute_queued_seek() happens before next audio/video is
        // attempted to be decoded or filtered.
        mpctx->stop_play = KEEP_PLAYING;
        double start = get_ab_loop_start_time(mpctx);
        if (start == MP_NOPTS_VALUE)
            start = 0;
        mark_seek(mpctx);
        queue_seek(mpctx, MPSEEK_ABSOLUTE, start, MPSEEK_EXACT,
                   MPSEEK_FLAG_NOFLUSH);
    }

    // Do not attempt to loop-file if --ab-loop is active.
    else if (opts->loop_file && mpctx->stop_play == AT_END_OF_FILE) {
        mpctx->stop_play = KEEP_PLAYING;
        set_osd_function(mpctx, OSD_FFW);
        queue_seek(mpctx, MPSEEK_ABSOLUTE, 0, MPSEEK_DEFAULT, MPSEEK_FLAG_NOFLUSH);
        if (opts->loop_file > 0)
            opts->loop_file--;
    }
}

void seek_to_last_frame(struct MPContext *mpctx)
{
    if (!mpctx->vo_chain)
        return;
    if (mpctx->hrseek_lastframe) // exit if we already tried this
        return;
    MP_VERBOSE(mpctx, "seeking to last frame...\n");
    // Approximately seek close to the end of the file.
    // Usually, it will seek some seconds before end.
    double end = get_play_end_pts(mpctx);
    if (end == MP_NOPTS_VALUE)
        end = get_time_length(mpctx);
    mp_seek(mpctx, (struct seek_params){
                   .type = MPSEEK_ABSOLUTE,
                   .amount = end,
                   .exact = MPSEEK_VERY_EXACT,
                   });
    // Make it exact: stop seek only if last frame was reached.
    if (mpctx->hrseek_active) {
        mpctx->hrseek_pts = 1e99; // "infinite"
        mpctx->hrseek_lastframe = true;
    }
}

static void handle_keep_open(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (opts->keep_open && mpctx->stop_play == AT_END_OF_FILE &&
        (opts->keep_open == 2 || !playlist_get_next(mpctx->playlist, 1)) &&
        opts->loop_times == 1)
    {
        mpctx->stop_play = KEEP_PLAYING;

// HISONA ...
/*
        if (mpctx->vo_chain) {
            if (!vo_has_frame(mpctx->video_out)) // EOF not reached normally
                seek_to_last_frame(mpctx);
            mpctx->playback_pts = mpctx->last_vo_pts;
        }
*/
        if (opts->keep_open_pause)
            set_pause_state(mpctx, true);
    }
}

// Potentially needed by some Lua scripts, which assume TICK always comes.
static void handle_dummy_ticks(struct MPContext *mpctx)
{
    if (mpctx->paused) {
        if (mp_time_sec() - mpctx->last_idle_tick > 0.050) {
            mpctx->last_idle_tick = mp_time_sec();
            mp_notify(mpctx, MPV_EVENT_TICK, NULL);
        }
    }
}

// Update current playback time.
static void handle_playback_time(struct MPContext *mpctx)
{
    if (mpctx->audio_status >= STATUS_PLAYING &&
               mpctx->audio_status < STATUS_EOF)
    {
        mpctx->playback_pts = playing_audio_pts(mpctx);
    }
}

static void handle_delayed_audio_seek(struct MPContext *mpctx)
{
    if (mpctx->seek_slave) {
        if (mpctx->video_pts != MP_NOPTS_VALUE) {
            // We know the video position now, so seek external audio to the
            // correct position.
            double pts = mpctx->video_pts +
                            get_track_seek_offset(mpctx, mpctx->seek_slave);
            demux_seek(mpctx->seek_slave->demuxer, pts, 0);
            mpctx->seek_slave = NULL;
        } else {
            // We won't get a video position; don't stall the audio stream.
            demux_block_reading(mpctx->seek_slave->demuxer, false);
            mpctx->seek_slave = NULL;
        }
    }
}

// We always make sure audio and video buffers are filled before actually
// starting playback. This code handles starting them at the same time.
static void handle_playback_restart(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    // Do not wait for video stream if it only has sparse frames.
    if (mpctx->audio_status < STATUS_READY)
        return;

    handle_pause_on_low_cache(mpctx);

    if (mpctx->audio_status == STATUS_READY) {

        MP_VERBOSE(mpctx, "starting audio playback\n");

        mpctx->audio_status = STATUS_PLAYING;

        fill_audio_out_buffers(mpctx); // actually play prepared buffer
        mp_wakeup_core(mpctx);
    }

    if (!mpctx->restart_complete) {
        mpctx->hrseek_active = false;
        mpctx->restart_complete = true;
        mpctx->current_seek = (struct seek_params){0};
        handle_playback_time(mpctx);
        mp_notify(mpctx, MPV_EVENT_PLAYBACK_RESTART, NULL);
        update_core_idle_state(mpctx);
        if (!mpctx->playing_msg_shown) {
            if (opts->playing_msg && opts->playing_msg[0]) {
                char *msg =
                    mp_property_expand_escaped_string(mpctx, opts->playing_msg);
                struct mp_log *log = mp_log_new(NULL, mpctx->log, "!term-msg");
                mp_info(log, "%s\n", msg);
                talloc_free(log);
                talloc_free(msg);
            }
            if (opts->osd_playing_msg && opts->osd_playing_msg[0]) {
                char *msg =
                    mp_property_expand_escaped_string(mpctx, opts->osd_playing_msg);
                set_osd_msg(mpctx, 1, opts->osd_duration, "%s", msg);
                talloc_free(msg);
            }
        }
        mpctx->playing_msg_shown = true;
        mp_wakeup_core(mpctx);
        mpctx->ab_loop_clip = mpctx->playback_pts < opts->ab_loop[1];
        MP_VERBOSE(mpctx, "playback restart complete\n");
    }
}

static void handle_eof(struct MPContext *mpctx)
{
    /* Don't quit while paused and we're displaying the last video frame. On the
     * other hand, if we don't have a video frame, then the user probably seeked
     * outside of the video, and we do want to quit. */

    if(mpctx->ao_chain && mpctx->audio_status == STATUS_EOF
       && !mpctx->stop_play) {
        mpctx->stop_play = AT_END_OF_FILE;
    }
}

void run_playloop(struct MPContext *mpctx)
{

    update_demuxer_properties(mpctx);

    handle_command_updates(mpctx);

    if (mpctx->lavfi && mp_filter_has_failed(mpctx->lavfi))
        mpctx->stop_play = AT_END_OF_FILE;

    fill_audio_out_buffers(mpctx);

    handle_delayed_audio_seek(mpctx);

    handle_playback_restart(mpctx);

    handle_playback_time(mpctx);

    handle_dummy_ticks(mpctx);

    update_osd_msg(mpctx);

    handle_eof(mpctx);

    handle_loop_file(mpctx);

    handle_keep_open(mpctx);

    handle_sstep(mpctx);

    update_core_idle_state(mpctx);

    if (mpctx->stop_play)
        return;

    if (mp_filter_run(mpctx->filter_root))
        mp_wakeup_core(mpctx);

    mp_wait_events(mpctx);

    handle_pause_on_low_cache(mpctx);

    mp_process_input(mpctx);

    execute_queued_seek(mpctx);
}

void mp_idle(struct MPContext *mpctx)
{
    handle_dummy_ticks(mpctx);
    mp_wait_events(mpctx);
    mp_process_input(mpctx);
    handle_command_updates(mpctx);
    update_osd_msg(mpctx);
}

// Waiting for the slave master to send us a new file to play.
void idle_loop(struct MPContext *mpctx)
{
    // ================= idle loop (STOP state) =========================
    bool need_reinit = true;
    while (mpctx->opts->player_idle_mode && !mpctx->playlist->current
           && mpctx->stop_play != PT_QUIT)
    {
        if (need_reinit) {
            uninit_audio_out(mpctx);
            mp_wakeup_core(mpctx);
            mp_notify(mpctx, MPV_EVENT_IDLE, NULL);
            need_reinit = false;
        }
        mp_idle(mpctx);
    }
}
