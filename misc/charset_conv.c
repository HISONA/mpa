/*
 * This file is part of mpv.
 *
 * Based on code taken from libass (ISC license), which was originally part
 * of MPlayer (GPL).
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
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

#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <assert.h>

#include "config.h"
#include "common/msg.h"
#include "charset_conv.h"

bool mp_charset_is_utf8(const char *user_cp)
{
    return user_cp && (strcasecmp(user_cp, "utf8") == 0 ||
                       strcasecmp(user_cp, "utf-8") == 0);
}

bool mp_charset_is_utf16(const char *user_cp)
{
    bstr s = bstr0(user_cp);
    return bstr_case_startswith(s, bstr0("utf16")) ||
           bstr_case_startswith(s, bstr0("utf-16"));
}

static const char *const utf_bom[3] = {"\xEF\xBB\xBF", "\xFF\xFE", "\xFE\xFF"};
static const char *const utf_enc[3] = {"utf-8",        "utf-16le", "utf-16be"};

static const char *ms_bom_guess(bstr buf)
{
    for (int n = 0; n < 3; n++) {
        if (bstr_startswith0(buf, utf_bom[n]))
            return utf_enc[n];
    }
    return NULL;
}

// Runs charset auto-detection on the input buffer, and returns the result.
// If auto-detection fails, NULL is returned.
// If user_cp doesn't refer to any known auto-detection (for example because
// it's a real iconv codepage), user_cp is returned without even looking at
// the buf data.
// The return value may (but doesn't have to) be allocated under talloc_ctx.
const char *mp_charset_guess(void *talloc_ctx, struct mp_log *log,  bstr buf,
                             const char *user_cp, int flags)
{
    if (strcasecmp(user_cp, "enca") == 0 || strcasecmp(user_cp, "guess") == 0 ||
        strcasecmp(user_cp, "uchardet") == 0 || strchr(user_cp, ':'))
    {
        mp_err(log, "This syntax for the --sub-codepage option was deprecated "
                    "and has been removed.\n");
        if (strncasecmp(user_cp, "utf8:", 5) == 0) {
            user_cp = user_cp + 5;
        } else {
            user_cp = "";
        }
    }

    if (user_cp[0] == '+') {
        mp_verbose(log, "Forcing charset '%s'.\n", user_cp + 1);
        return user_cp + 1;
    }

    const char *bom_cp = ms_bom_guess(buf);
    if (bom_cp) {
        mp_verbose(log, "Data has a BOM, assuming %s as charset.\n", bom_cp);
        return bom_cp;
    }

    int r = bstr_validate_utf8(buf);
    if (r >= 0 || (r > -8 && (flags & MP_ICONV_ALLOW_CUTOFF))) {
        mp_verbose(log, "Data looks like UTF-8, ignoring user-provided charset.\n");
        return "utf-8";
    }

    const char *res = NULL;
    if (strcasecmp(user_cp, "auto") == 0) {
        if (!res) {
            mp_verbose(log, "Charset auto-detection failed.\n");
            res = "UTF-8-BROKEN";
        }
    } else {
        res = user_cp;
    }

    mp_verbose(log, "Using charset '%s'.\n", res);
    return res;
}

// Use iconv to convert buf to UTF-8.
// Returns buf.start==NULL on error. Returns buf if cp is NULL, or if there is
// obviously no conversion required (e.g. if cp is "UTF-8").
// Returns a newly allocated buffer if conversion is done and succeeds. The
// buffer will be terminated with 0 for convenience (the terminating 0 is not
// included in the returned length).
// Free the returned buffer with talloc_free().
//  buf: input data
//  cp: iconv codepage (or NULL)
//  flags: combination of MP_ICONV_* flags
//  returns: buf (no conversion), .start==NULL (error), or allocated buffer
bstr mp_iconv_to_utf8(struct mp_log *log, bstr buf, const char *cp, int flags)
{
    if (flags & MP_NO_LATIN1_FALLBACK) {
        return buf;
    } else {
        return bstr_sanitize_utf8_latin1(NULL, buf);
    }
}
