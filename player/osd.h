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

#ifndef MPLAYER_SUB_H
#define MPLAYER_SUB_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "options/m_option.h"

// OSD symbols. osd_font.pfb has them starting from codepoint OSD_CODEPOINTS.
// Symbols with a value >= 32 are normal unicode codepoints.
enum mp_osd_font_codepoints {
    OSD_PLAY = 0x01,
    OSD_PAUSE = 0x02,
    OSD_STOP = 0x03,
    OSD_REW = 0x04,
    OSD_FFW = 0x05,
    OSD_CLOCK = 0x06,
    OSD_CONTRAST = 0x07,
    OSD_SATURATION = 0x08,
    OSD_VOLUME = 0x09,
    OSD_BRIGHTNESS = 0x0A,
    OSD_HUE = 0x0B,
    OSD_BALANCE = 0x0C,
    OSD_PANSCAN = 0x50,

    OSD_PB_START = 0x10,
    OSD_PB_0 = 0x11,
    OSD_PB_END = 0x12,
    OSD_PB_1 = 0x13,
};


// Never valid UTF-8, so we expect it's free for use.
// Specially interpreted by osd_libass.c, in order to allow/escape ASS tags.
#define OSD_ASS_0 "\xFD"
#define OSD_ASS_1 "\xFE"

#endif /* MPLAYER_SUB_H */
