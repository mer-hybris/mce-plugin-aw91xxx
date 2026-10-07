/** @file effect_ctl.c
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include "effect_ctl.h"

#include "effect_vcpu.h"
#include "hal-aw91xxx.h"
#include "led_pattern.h"

#include "../plugin/plugin-logging.h"

#include <glib.h>

#define numof(array) (sizeof (array) / sizeof *(array))

#define effect_ctl_log(LEV, FMT, ARGS...)\
     mce_log(LEV, "%s: " FMT, effect_ctl_identity(self), ##ARGS)

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * EFFECT_CTL
 * ------------------------------------------------------------------------- */

static const char *effect_ctl_identity     (const EffectCtl *self);
EffectVCPU        *effect_ctl_get_vcpu     (const EffectCtl *self, unsigned led);
LedPattern        *effect_ctl_get_pattern  (const EffectCtl *self);
bool               effect_ctl_get_active   (const EffectCtl *self);
void               effect_ctl_set_active   (EffectCtl *self, bool active);
bool               effect_ctl_advance_clock(EffectCtl *self, unsigned tick);
bool               effect_ctl_compile      (EffectCtl *self, const char *srce);
static void        effect_ctl_unload       (EffectCtl *self);
EffectCtl         *effect_ctl_create       (LedPattern *pattern);
void               effect_ctl_delete       (EffectCtl *self);

/* ========================================================================= *
 * Code
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

static inline const char *bool_repr(int val)
{
    return val < 0 ? "unset" : val ? "true" : "false";
}

/* ------------------------------------------------------------------------- *
 * EFFECT_CTL
 * ------------------------------------------------------------------------- */

struct EffectCtl
{
    gchar      *ectl_identity;
    LedPattern *ectl_pattern;
    EffectVCPU *ectl_vcpu[AW91XXX_LED_COUNT];
    bool        ectl_active;
};

static const char *
effect_ctl_identity(const EffectCtl *self)
{
    const char *identity = self ? self->ectl_identity : NULL;
    return identity ?: "PatternUnknown";
}

EffectVCPU *
effect_ctl_get_vcpu(const EffectCtl *self, unsigned led)
{
    return self && led < numof(self->ectl_vcpu) ? self->ectl_vcpu[led] : NULL;
}

LedPattern *
effect_ctl_get_pattern(const EffectCtl *self)
{
    return self ? self->ectl_pattern : NULL;
}

bool
effect_ctl_get_active(const EffectCtl *self)
{
    return self ? self->ectl_active : false;
}

void
effect_ctl_set_active(EffectCtl *self, bool active)
{
    if( self && self->ectl_active != active ) {
        effect_ctl_log(LL_DEBUG, "active: %s -> %s", bool_repr(self->ectl_active), bool_repr(active));

        self->ectl_active = active;

        for( size_t led = 0; led < AW91XXX_LED_COUNT; ++led )
            effect_vcpu_set_active(effect_ctl_get_vcpu(self, led), active);
    }
}

bool
effect_ctl_advance_clock(EffectCtl *self, unsigned tick)
{
    bool clock_needed = false;

    if( self ) {
        for( size_t led = 0; led < AW91XXX_LED_COUNT; ++led )
            if( effect_vcpu_advance_clock(effect_ctl_get_vcpu(self, led), tick) )
                clock_needed = true;
    }

    return clock_needed;
}

bool
effect_ctl_compile(EffectCtl *self, const char *srce)
{
    bool            loaded   = false;
    EffectCompiler *compiler = NULL;

    effect_ctl_log(LL_DEBUG, "SRC %s", srce ?: "<null>");

    effect_ctl_unload(self);

    compiler = effect_compiler_create();

    if( effect_compiler_compile(compiler, srce) < 1 )
        goto EXIT;

    for( unsigned led = 0; led < AW91XXX_LED_COUNT; ++led ) {
        size_t   size = 0;
        uint8_t *code = effect_compiler_get_bytecode(compiler, led, &size);

        effect_ctl_log(LL_DEBUG, "LED(%d) %s", led, aw91xxx_led_name(led));
        for( size_t k = 0; k < size; ++k )
            effect_ctl_log(LL_DEBUG, "CODE [%04zx] 0x%02x", k, code[k]);

        if( code && size )
            self->ectl_vcpu[led] = effect_vcpu_create(self, led, code, size);
    }

    loaded = true;

EXIT:
    effect_compiler_delete(compiler);

    return loaded;
}

static void
effect_ctl_unload(EffectCtl *self)
{
    for( size_t led = 0; led < AW91XXX_LED_COUNT; ++led )
        effect_vcpu_delete(self->ectl_vcpu[led]), self->ectl_vcpu[led] = NULL;
}

EffectCtl *
effect_ctl_create(LedPattern *pattern)
{
    EffectCtl *self = g_malloc0(sizeof *self);

    self->ectl_identity = g_strdup(led_pattern_name(pattern));
    self->ectl_pattern  = pattern;
    self->ectl_active   = false;

    for( size_t led = 0; led < AW91XXX_LED_COUNT; ++led )
        self->ectl_vcpu[led] = NULL;

    return self;
}

void
effect_ctl_delete(EffectCtl *self)
{
    if( self ) {
        effect_ctl_unload(self);

        self->ectl_pattern = NULL;

        g_free(self->ectl_identity), self->ectl_identity = NULL;

        g_free(self);
    }
}
