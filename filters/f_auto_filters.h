#pragma once

#include "filter.h"

// A filter which inserts the required deinterlacing filter based on the
// hardware decode mode and the deinterlace user option.

// Insert a filter that inserts scaletempo depending on speed settings.
struct mp_filter *mp_autoaspeed_create(struct mp_filter *parent);
