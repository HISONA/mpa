#include <math.h>

#include "common/common.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "options/options.h"

#include "f_auto_filters.h"
#include "f_utils.h"
#include "filter.h"
#include "filter_internal.h"
#include "user_filters.h"

struct aspeed_priv {
    struct mp_subfilter sub;
    double cur_speed;
};

static void aspeed_process(struct mp_filter *f)
{
    struct aspeed_priv *p = f->priv;

    if (!mp_subfilter_read(&p->sub))
        return;

    if (fabs(p->cur_speed - 1.0) < 1e-8) {
        if (p->sub.filter)
            MP_VERBOSE(f, "removing scaletempo\n");
        if (!mp_subfilter_drain_destroy(&p->sub))
            return;
    } else if (!p->sub.filter) {
        MP_VERBOSE(f, "adding scaletempo\n");
        p->sub.filter =
            mp_create_user_filter(f, MP_OUTPUT_CHAIN_AUDIO, "scaletempo", NULL);
        if (!p->sub.filter) {
            MP_ERR(f, "could not create scaletempo filter\n");
            mp_subfilter_continue(&p->sub);
            return;
        }
    }

    if (p->sub.filter) {
        struct mp_filter_command cmd = {
            .type = MP_FILTER_COMMAND_SET_SPEED,
            .speed = p->cur_speed,
        };
        mp_filter_command(p->sub.filter, &cmd);
    }

    mp_subfilter_continue(&p->sub);
}

static bool aspeed_command(struct mp_filter *f, struct mp_filter_command *cmd)
{
    struct aspeed_priv *p = f->priv;

    if (cmd->type == MP_FILTER_COMMAND_SET_SPEED) {
        p->cur_speed = cmd->speed;
        return true;
    }

    if (cmd->type == MP_FILTER_COMMAND_IS_ACTIVE) {
        if(p->sub.filter != NULL) cmd->is_active = 1;
        else                      cmd->is_active = 0;
        return true;
    }

    return false;
}

static void aspeed_reset(struct mp_filter *f)
{
    struct aspeed_priv *p = f->priv;

    mp_subfilter_reset(&p->sub);
}

static void aspeed_destroy(struct mp_filter *f)
{
    struct aspeed_priv *p = f->priv;

    mp_subfilter_reset(&p->sub);
    TA_FREEP(&p->sub.filter);
}

static const struct mp_filter_info aspeed_filter = {
    .name = "autoaspeed",
    .priv_size = sizeof(struct aspeed_priv),
    .command = aspeed_command,
    .process = aspeed_process,
    .reset = aspeed_reset,
    .destroy = aspeed_destroy,
};

struct mp_filter *mp_autoaspeed_create(struct mp_filter *parent)
{
    struct mp_filter *f = mp_filter_create(parent, &aspeed_filter);
    if (!f)
        return NULL;

    struct aspeed_priv *p = f->priv;
    p->cur_speed = 1.0;

    p->sub.in = mp_filter_add_pin(f, MP_PIN_IN, "in");
    p->sub.out = mp_filter_add_pin(f, MP_PIN_OUT, "out");

    return f;
}
