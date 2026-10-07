/** @file effect_mgr.h
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include "effect_mgr.h"

#include "effect_ctl.h"
#include "effect_vcpu.h"
#include "hal-aw91xxx.h"
#include "led_pattern.h"

#include "../plugin/plugin-config.h"
#include "../plugin/plugin-logging.h"

/* ========================================================================= *
 * Types
 * ========================================================================= */

struct EffectMgr
{
    LedPattern     **emgr_pattern_lut;
    size_t           emgr_pattern_cnt;
    bool             emgr_breathing_allowed;
    unsigned         emgr_clock_tick;
    guint            emgr_clock_id;
};

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

static bool      effect_mgr_is_valid             (const EffectMgr *self);
static bool      effect_mgr_is_breathing_allowed (const EffectMgr *self);
static unsigned  effect_mgr_get_tick             (const EffectMgr *self);
void             effect_mgr_set_breathing_allowed(EffectMgr *self, bool breathing_allowed);
static bool      effect_mgr_sync_to_leds         (EffectMgr *self);
void             effect_mgr_rethink_state        (EffectMgr *self);
static gboolean  effect_mgr_clock_cb             (gpointer aptr);
static void      effect_mgr_start_clock          (EffectMgr *self);
static void      effect_mgr_stop_clock           (EffectMgr *self);
EffectMgr       *effect_mgr_create               (void);
void             effect_mgr_delete               (EffectMgr *self);
LedPattern      *effect_mgr_lookup_pattern       (EffectMgr *self, const char *name);
static int       effect_mgr_priority_sort_cb     (const void *p1, const void *p2);
static void      effect_mgr_load_patterns        (EffectMgr *self);
void             effect_mgr_unload_patterns      (EffectMgr *self);

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
 * EFFECT_MGR
 * ------------------------------------------------------------------------- */

static bool
effect_mgr_is_valid(const EffectMgr *self)
{
    return self != NULL;
}

static bool
effect_mgr_is_breathing_allowed(const EffectMgr *self)
{
    return effect_mgr_is_valid(self) ? self->emgr_breathing_allowed : false;
}

static unsigned
effect_mgr_get_tick(const EffectMgr *self)
{
    return effect_mgr_is_valid(self) ? self->emgr_clock_tick : 0;
}

void
effect_mgr_set_breathing_allowed(EffectMgr *self, bool breathing_allowed)
{
    if( effect_mgr_is_valid(self) && self->emgr_breathing_allowed != breathing_allowed ) {
        mce_log(LL_DEBUG, "breathing_allowed: %s -> %s",
                bool_repr(self->emgr_breathing_allowed), bool_repr(breathing_allowed));
        self->emgr_breathing_allowed = breathing_allowed;
        effect_mgr_rethink_state(self);
    }
}

static bool
effect_mgr_sync_to_leds(EffectMgr *self)
{
    bool clock_needed = false;

    if( effect_mgr_is_valid(self) ) {
        Aw91xxxLedsState reserved = {};
        Aw91xxxLedsState state    = {};

        for( size_t i = 0; i < self->emgr_pattern_cnt; ++i ) {
            LedPattern *pattern = self->emgr_pattern_lut[i];
            if( !led_pattern_is_active(pattern) )
                continue;

            EffectCtl *control = NULL;
            if( effect_mgr_is_breathing_allowed(self) )
                control = led_pattern_get_effect(pattern);

            if( !control ) {
                unsigned led = led_pattern_led(pattern);
                if( !reserved.ls_active[led] ) {
                    reserved.ls_active[led] = true;
                    state.ls_active[led]    = AW91XXX_DIM_MAX;
                }
                continue;
            }

            if( effect_ctl_advance_clock(control, effect_mgr_get_tick(self)) )
                clock_needed = true;

            for( unsigned led = 0; led < AW91XXX_LED_COUNT; ++led ) {
                if( reserved.ls_active[led] )
                    continue;

                EffectVCPU *vcpu = effect_ctl_get_vcpu(control, led);
                if( vcpu ) {
                    reserved.ls_active[led] = true;
                    state.ls_active[led]    = effect_vcpu_get_output(vcpu);
                }
            }
        }

        aw91xxx_leds_set_state(&state);
    }
    return clock_needed;
}

void
effect_mgr_rethink_state(EffectMgr *self)
{
    if( effect_mgr_sync_to_leds(self) )
        effect_mgr_start_clock(self);
    else
        effect_mgr_stop_clock(self);
}

static gboolean
effect_mgr_clock_cb(gpointer aptr)
{
    EffectMgr *self = aptr;

    self->emgr_clock_tick += 1;

    mce_log(LL_DEBUG, "TICK %u", self->emgr_clock_tick);

    bool clock_needed = effect_mgr_sync_to_leds(self);

    if( clock_needed )
        return G_SOURCE_CONTINUE;

    mce_log(LL_DEBUG, "CLOCK DISABLED");
    self->emgr_clock_id = 0;
    return G_SOURCE_REMOVE;
}

static void
effect_mgr_start_clock(EffectMgr *self)
{
    if( effect_mgr_is_valid(self) && !self->emgr_clock_id ) {
        self->emgr_clock_id = g_timeout_add(EFFECT_TICK_MS, effect_mgr_clock_cb, self);
        mce_log(LL_DEBUG, "CLOCK STARTED");
    }
}

static void
effect_mgr_stop_clock(EffectMgr *self)
{
    if( self && self->emgr_clock_id ) {
        g_source_remove(self->emgr_clock_id), self->emgr_clock_id = 0;
        mce_log(LL_DEBUG, "CLOCK STOPPED");
    }
}

EffectMgr *
effect_mgr_create(void)
{
    EffectMgr *self = g_malloc0(sizeof *self);

    self->emgr_pattern_lut       = NULL;
    self->emgr_pattern_cnt       = 0;
    self->emgr_breathing_allowed = false;
    self->emgr_clock_id          = 0;

    /* Make sure pattern, clock and led states are in sync
     * before control returns from plugin to mce side.
     */
    effect_mgr_load_patterns(self);

    return self;
}

void
effect_mgr_delete(EffectMgr *self)
{
    if( effect_mgr_is_valid(self) ) {
        effect_mgr_unload_patterns(self);
        effect_mgr_stop_clock(self);
        g_free(self);
    }
}

LedPattern *
effect_mgr_lookup_pattern(EffectMgr *self, const char *name)
{
    if( effect_mgr_is_valid(self) ) {
        for( size_t i = 0; i < self->emgr_pattern_cnt; ++i ) {
            LedPattern *pattern = self->emgr_pattern_lut[i];
            if( !strcmp(led_pattern_name(pattern), name) )
                return pattern;
        }
        mce_log(LL_WARN, "pattern %s is unknown", name);
    }
    return NULL;
}

static int
effect_mgr_priority_sort_cb(const void *p1, const void *p2)
{
    const LedPattern *P1 = *(const LedPattern **)p1;
    const LedPattern *P2 = *(const LedPattern **)p2;

    return led_pattern_priority(P1) - led_pattern_priority(P2);
}

static void
effect_mgr_load_patterns(EffectMgr *self)
{
    /* Discard any old data we might have
     */
    effect_mgr_unload_patterns(self);

    /* Parse Hybris led configuration and generate hybris color -> led mapping
     */
    gsize   cnt = 0;
    gchar **arr = mce_conf_get_keys(MCE_CONF_LED_PATTERN_HYBRIS_GROUP, &cnt);

    self->emgr_pattern_lut = g_malloc0_n(cnt, sizeof *self->emgr_pattern_lut);

    for( gsize i = 0; i < cnt; ++i ) {
        const char *name    = arr[i];
        LedPattern *pattern = led_pattern_create(self, name);
        if( led_pattern_is_valid(pattern) )
            self->emgr_pattern_lut[self->emgr_pattern_cnt++] = pattern;
        else
            led_pattern_delete(pattern);
    }

    /* Sort in highest priority first order */
    if( self->emgr_pattern_cnt > 1 )
        qsort(self->emgr_pattern_lut, self->emgr_pattern_cnt, sizeof *self->emgr_pattern_lut, effect_mgr_priority_sort_cb);

    for( size_t i = 0; i < self->emgr_pattern_cnt; ++i ) {
        LedPattern *pattern = self->emgr_pattern_lut[i];
        mce_log(LL_DEBUG, "config %s: pri=%d rgb=0x%06x -> %s", led_pattern_name(pattern),
                led_pattern_priority(pattern), led_pattern_rgb(pattern),
                aw91xxx_led_name(led_pattern_led(pattern)));
    }

    /* Reset sysfs bookkeeping and controls to a known state
     */
    aw91xxx_leds_reset_state();

    g_strfreev(arr);
}

void
effect_mgr_unload_patterns(EffectMgr *self)
{
    if( effect_mgr_is_valid(self) ) {
        /* 1) Disable breathing
         * 2) Deactivate all but PatternPowerOff
         * 3) Sync to sysfs
         *
         * -> If PatternPowerOff is active, its static variant is left on
         */
        effect_mgr_set_breathing_allowed(self, false);
        for( size_t i = 0; i < self->emgr_pattern_cnt; ++i ) {
            LedPattern *pattern = self->emgr_pattern_lut[i];
            if( strcmp(led_pattern_name(pattern), MCE_LED_PATTERN_POWER_OFF) )
                led_pattern_set_active(pattern, false);
        }
        effect_mgr_rethink_state(self);

        /* Free memory
         */
        for( gsize i = 0; i < self->emgr_pattern_cnt; ++i )
            led_pattern_delete(self->emgr_pattern_lut[i]);

        g_free(self->emgr_pattern_lut), self->emgr_pattern_lut = 0, self->emgr_pattern_cnt = 0;
    }
}
