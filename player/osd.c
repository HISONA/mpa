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
#include <limits.h>
#include <assert.h>

#include "config.h"
#include "mpa_talloc.h"

#include "common/msg.h"
#include "common/msg_control.h"
#include "options/options.h"
#include "common/common.h"
#include "options/m_property.h"
#include "filters/f_decoder_wrapper.h"

#include "osdep/terminal.h"
#include "osdep/timer.h"

#include "demux/demux.h"
#include "stream/stream.h"
#include "osd.h"

#include "core.h"
#include "command.h"

#define saddf(var, ...) (*(var) = talloc_asprintf_append((*var), __VA_ARGS__))

// append time in the hh:mm:ss format (plus fractions if wanted)
static void sadd_hhmmssff(char **buf, double time, bool fractions)
{
    char *s = mp_format_time(time, fractions);
    *buf = talloc_strdup_append(*buf, s);
    talloc_free(s);
}

static void sadd_percentage(char **buf, int percent) {
    if (percent >= 0)
        *buf = talloc_asprintf_append(*buf, " (%d%%)", percent);
}

static char *join_lines(void *ta_ctx, char **parts, int num_parts)
{
    char *res = talloc_strdup(ta_ctx, "");
    for (int n = 0; n < num_parts; n++)
        res = talloc_asprintf_append(res, "%s%s", n ? "\n" : "", parts[n]);
    return res;
}

static void term_osd_update(struct MPContext *mpctx)
{
    int num_parts = 0;
    char *parts[3] = {0};

    if (!mpctx->opts->use_terminal)
        return;

    if (mpctx->term_osd_subs && mpctx->term_osd_subs[0])
        parts[num_parts++] = mpctx->term_osd_subs;
    if (mpctx->term_osd_text && mpctx->term_osd_text[0])
        parts[num_parts++] = mpctx->term_osd_text;
    if (mpctx->term_osd_status && mpctx->term_osd_status[0])
        parts[num_parts++] = mpctx->term_osd_status;

    char *s = join_lines(mpctx, parts, num_parts);

    if (strcmp(mpctx->term_osd_contents, s) == 0 &&
        mp_msg_has_status_line(mpctx->global))
    {
        talloc_free(s);
    } else {
        talloc_free(mpctx->term_osd_contents);
        mpctx->term_osd_contents = s;
        mp_msg(mpctx->statusline, MSGL_STATUS, "%s", s);
    }
}

static void term_osd_set_text_lazy(struct MPContext *mpctx, const char *text)
{
    talloc_free(mpctx->term_osd_text);
    mpctx->term_osd_text = talloc_strdup(mpctx, text);
}

static void term_osd_set_status_lazy(struct MPContext *mpctx, const char *text)
{
    talloc_free(mpctx->term_osd_status);
    mpctx->term_osd_status = talloc_strdup(mpctx, text);

    int w = 80, h = 24;
    terminal_get_size(&w, &h);
    if (strlen(mpctx->term_osd_status) > w && !strchr(mpctx->term_osd_status, '\n'))
        mpctx->term_osd_status[w] = '\0';
}

static void add_term_osd_bar(struct MPContext *mpctx, char **line, int width)
{
    struct MPOpts *opts = mpctx->opts;

    if (width < 5)
        return;

    int pos = get_current_pos_ratio(mpctx, false) * (width - 3);
    pos = MPCLAMP(pos, 0, width - 3);

    bstr chars = bstr0(opts->term_osd_bar_chars);
    bstr parts[5];
    for (int n = 0; n < 5; n++)
        parts[n] = bstr_split_utf8(chars, &chars);

    saddf(line, "\r%.*s", BSTR_P(parts[0]));
    for (int n = 0; n < pos; n++)
        saddf(line, "%.*s", BSTR_P(parts[1]));
    saddf(line, "%.*s", BSTR_P(parts[2]));
    for (int n = 0; n < width - 3 - pos; n++)
        saddf(line, "%.*s", BSTR_P(parts[3]));
    saddf(line, "%.*s", BSTR_P(parts[4]));
}

static bool is_busy(struct MPContext *mpctx)
{
    return !mpctx->restart_complete && mp_time_sec() - mpctx->start_timestamp > 0.3;
}

static char *get_term_status_msg(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    if (opts->status_msg)
        return mp_property_expand_escaped_string(mpctx, opts->status_msg);

    char *line = NULL;

    // Playback status
    if (is_busy(mpctx)) {
        saddf(&line, "(...) ");
    } else if (mpctx->paused_for_cache && !opts->pause) {
        saddf(&line, "(Buffering) ");
    } else if (mpctx->paused) {
        saddf(&line, "(Paused) ");
    }

    if (mpctx->ao_chain)
        saddf(&line, "A");

    saddf(&line, ": ");

    // Playback position
    sadd_hhmmssff(&line, get_playback_time(mpctx), opts->osd_fractions);
    saddf(&line, " / ");
    sadd_hhmmssff(&line, get_time_length(mpctx), opts->osd_fractions);

    sadd_percentage(&line, get_percent_pos(mpctx));

    // other
    if (opts->playback_speed != 1)
        saddf(&line, " x%4.2f", opts->playback_speed);

    // A-V sync
    if (mpctx->ao_chain && mpctx->vo_chain && !mpctx->vo_chain->is_coverart) {
        saddf(&line, " A-V:%7.3f", mpctx->last_av_difference);
        if (fabs(mpctx->total_avsync_change) > 0.05)
            saddf(&line, " ct:%7.3f", mpctx->total_avsync_change);
    }

    if (mpctx->demuxer && demux_is_network_cached(mpctx->demuxer)) {
        saddf(&line, " Cache: ");

        struct demux_ctrl_reader_state s = {.ts_duration = -1};
        demux_control(mpctx->demuxer, DEMUXER_CTRL_GET_READER_STATE, &s);

        if (s.ts_duration < 0) {
            saddf(&line, "???");
        } else {
            saddf(&line, "%2ds", (int)s.ts_duration);
        }
        int64_t cache_size = s.fw_bytes;
        if (cache_size > 0) {
            if (cache_size >= 1024 * 1024) {
                saddf(&line, "+%lldMB", (long long)(cache_size / 1024 / 1024));
            } else {
                saddf(&line, "+%lldKB", (long long)(cache_size / 1024));
            }
        }
    }

    return line;
}

static void term_osd_print_status_lazy(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    if (!opts->use_terminal)
        return;

    if (opts->quiet || !mpctx->playback_initialized ||
        !mpctx->playing_msg_shown || mpctx->stop_play)
    {
        if (!mpctx->playing || mpctx->stop_play) {
            mp_msg_flush_status_line(mpctx->log);
            term_osd_set_status_lazy(mpctx, "");
        }
        return;
    }

    char *line = get_term_status_msg(mpctx);

    if (opts->term_osd_bar) {
        saddf(&line, "\n");
        int w = 80, h = 24;
        terminal_get_size(&w, &h);
        add_term_osd_bar(mpctx, &line, w);
    }

    term_osd_set_status_lazy(mpctx, line);
    talloc_free(line);
}

static bool set_osd_msg_va(struct MPContext *mpctx, int level, int time,
                           const char *fmt, va_list ap)
{
    if (level > mpctx->opts->osd_level)
        return false;

    talloc_free(mpctx->osd_msg_text);
    mpctx->osd_msg_text = talloc_vasprintf(mpctx, fmt, ap);
    mpctx->osd_msg_next_duration = time / 1000.0;
    mp_wakeup_core(mpctx);
    return true;
}

bool set_osd_msg(struct MPContext *mpctx, int level, int time,
                 const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool r = set_osd_msg_va(mpctx, level, time, fmt, ap);
    va_end(ap);
    return r;
}

// osd_function is the symbol appearing in the video status, such as OSD_PLAY
void set_osd_function(struct MPContext *mpctx, int osd_function)
{
    mp_wakeup_core(mpctx);
}

// Update the OSD text (both on VO and terminal status line).
void update_osd_msg(struct MPContext *mpctx)
{
    term_osd_set_text_lazy(mpctx, mpctx->osd_msg_text);
    term_osd_print_status_lazy(mpctx);
    term_osd_update(mpctx);

}
