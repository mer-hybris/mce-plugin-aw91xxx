/** @file led_pattern.c
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include "led_pattern.h"

#include "effect_mgr.h"
#include "effect_ctl.h"
#include "hal-aw91xxx.h"

#include "../plugin/plugin-config.h"
#include "../plugin/plugin-logging.h"

/* ========================================================================= *
 * Types
 * ========================================================================= */

struct LedPattern {
    // Immutable values from hybris configuration
    gchar     *ps_name;
    gint       ps_hybris_priority;
    gint       ps_hybris_screen_on;
    gint       ps_hybris_timeout;
    gint       ps_hybris_on_period;
    gint       ps_hybris_off_period;
    unsigned   ps_hybris_rgb;
    unsigned   ps_hybris_led;

    bool       ps_valid;
    bool       ps_active;

    EffectMgr *ps_effect_mgr;
    EffectCtl *ps_effect_ctl;
};

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

static gint  parse_int  (const char *str, bool *parsed);
static guint parse_rgb24(const char *str, bool *parsed);

/* ------------------------------------------------------------------------- *
 * LED_PATTERN
 * ------------------------------------------------------------------------- */

LedPattern       *led_pattern_create            (EffectMgr *manager, const char *pattern);
void              led_pattern_delete            (LedPattern *self);
static EffectMgr *led_pattern_get_manager       (const LedPattern *self);
EffectCtl        *led_pattern_get_effect        (const LedPattern *self);
void              led_pattern_set_effect        (LedPattern *self, EffectCtl *effect);
bool              led_pattern_is_valid          (const LedPattern *self);
static void       led_pattern_set_valid         (LedPattern *self, bool valid);
bool              led_pattern_is_active         (const LedPattern *self);
void              led_pattern_set_active        (LedPattern *self, bool active);
const char       *led_pattern_name              (const LedPattern *self);
int               led_pattern_priority          (const LedPattern *self);
int               led_pattern_screen_on         (const LedPattern *self);
int               led_pattern_timeout           (const LedPattern *self);
int               led_pattern_on_period         (const LedPattern *self);
int               led_pattern_off_period        (const LedPattern *self);
unsigned          led_pattern_rgb               (const LedPattern *self);
unsigned          led_pattern_led               (const LedPattern *self);
static void       led_pattern_parse_priority    (LedPattern *self, const char *str);
static void       led_pattern_parse_screen_on   (LedPattern *self, const char *str);
static void       led_pattern_parse_timeout     (LedPattern *self, const char *str);
static void       led_pattern_parse_on_period   (LedPattern *self, const char *str);
static void       led_pattern_parse_off_period  (LedPattern *self, const char *str);
static void       led_pattern_parse_rgb24       (LedPattern *self, const char *str);
static bool       led_pattern_load_configuration(LedPattern *self);
static void       led_pattern_load_bytecode     (LedPattern *self);

/* ========================================================================= *
 * Inline helpers
 * ========================================================================= */

static inline const char *bool_repr(bool val)
{
    return val ? "true" : "false";
}

static gint parse_int(const char *str, bool *parsed)
{
    char *end = NULL;
    gint  num = strtol(str, &end, 0);

    *parsed = end > str && *end == 0;

    //mce_log(LL_DEBUG, "'%s' -> num=%dx valid=%d", str, num, *parsed);

    return num;
}

static guint parse_rgb24(const char *str, bool *parsed)
{
    char  *end = NULL;
    guint  num = strtoul(str, &end, 16);

    *parsed = end > str && *end == 0;

    //mce_log(LL_DEBUG, "'%s' -> num=0x%06x valid=%d", str, num, *parsed);

    return num;
}

/* ========================================================================= *
 * Code
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * LED_PATTERN
 * ------------------------------------------------------------------------- */

LedPattern *
led_pattern_create(EffectMgr *manager, const char *pattern)
{
    LedPattern *self = g_malloc0(sizeof *self);

    self->ps_hybris_priority   = 0;
    self->ps_hybris_screen_on  = 0;
    self->ps_hybris_timeout    = 0;
    self->ps_hybris_on_period  = 0;
    self->ps_hybris_off_period = 0;
    self->ps_hybris_rgb        = 0;
    self->ps_hybris_led        = AW91XXX_LED_DEFAULT;

    self->ps_name   = g_strdup(pattern);
    self->ps_valid  = true;
    self->ps_active = false;

    self->ps_effect_mgr = manager;
    self->ps_effect_ctl = NULL;

    if( !led_pattern_load_configuration(self) )
        mce_log(LL_WARN, "pattern configuration %s is malformed", led_pattern_name(self));
    else
        led_pattern_load_bytecode(self);

    return self;
}

void
led_pattern_delete(LedPattern *self)
{
    if( self ) {
        effect_ctl_delete(self->ps_effect_ctl), self->ps_effect_ctl = NULL;

        g_free(self->ps_name), self->ps_name = NULL;

        self->ps_effect_mgr = NULL;

        g_free(self);
    }
}

static EffectMgr *
led_pattern_get_manager(const LedPattern *self)
{
    return self ? self->ps_effect_mgr : NULL;
}

EffectCtl *
led_pattern_get_effect(const LedPattern *self)
{
    return self ? self->ps_effect_ctl : NULL;
}

void
led_pattern_set_effect(LedPattern *self, EffectCtl *effect)
{
    if( self )
        effect_ctl_delete(self->ps_effect_ctl), self->ps_effect_ctl = effect;
    else
        effect_ctl_delete(effect);
}

bool
led_pattern_is_valid(const LedPattern *self)
{
    return self ? self->ps_valid : false;
}

static void
led_pattern_set_valid(LedPattern *self, bool valid)
{
    if( self && self->ps_valid != valid ) {
        mce_log(LL_DEBUG, "%s: valid: %s -> %s", led_pattern_name(self), bool_repr(self->ps_valid), bool_repr(valid));
        self->ps_valid = valid;
    }
}

bool
led_pattern_is_active(const LedPattern *self)
{
    return self ? self->ps_active : false;
}

void
led_pattern_set_active(LedPattern *self, bool active)
{
    if( self && self->ps_active != active ) {
        mce_log(LL_DEBUG, "%s: active: %s -> %s", led_pattern_name(self), bool_repr(self->ps_active), bool_repr(active));
        self->ps_active = active;

        effect_ctl_set_active(led_pattern_get_effect(self), active);

        effect_mgr_rethink_state(led_pattern_get_manager(self));
    }
}

const char *
led_pattern_name(const LedPattern *self)
{
    const char *name = self ? self->ps_name : NULL;
    return name ?: "PatternUnknown";
}

int
led_pattern_priority(const LedPattern *self)
{
    return self ? self->ps_hybris_priority : 0;
}

int
led_pattern_screen_on(const LedPattern *self)
{
    return self ? self->ps_hybris_screen_on : 0;
}

int
led_pattern_timeout(const LedPattern *self)
{
    return self ? self->ps_hybris_timeout : 0;
}

int
led_pattern_on_period(const LedPattern *self)
{
    return self ? self->ps_hybris_on_period : 0;
}

int
led_pattern_off_period(const LedPattern *self)
{
    return self ? self->ps_hybris_off_period : 0;
}

unsigned
led_pattern_rgb(const LedPattern *self)
{
    return self ? self->ps_hybris_rgb : 0;
}

unsigned
led_pattern_led(const LedPattern *self)
{
    return self ? self->ps_hybris_led : 0;
}

static void
led_pattern_parse_priority(LedPattern *self, const char *str)
{
    bool ack = false;
    gint num = parse_int(str, &ack);

    if( !ack || num < 0 || num > 255 ) {
        mce_log(LL_WARN, "%s: invalid priority value '%s'", led_pattern_name(self), str);
        led_pattern_set_valid(self, false);
    }
    else {
        self->ps_hybris_priority = num;
    }
}

static void
led_pattern_parse_screen_on(LedPattern *self, const char *str)
{
    bool ack = false;
    gint num = parse_int(str, &ack);

    if( !ack || num < 0 || num > 7 ) {
        mce_log(LL_WARN, "%s: invalid screen_on value '%s'", led_pattern_name(self), str);
        led_pattern_set_valid(self, false);
    }
    else {
        self->ps_hybris_screen_on = num;
    }
}

static void
led_pattern_parse_timeout(LedPattern *self, const char *str)
{
    bool ack = false;
    gint num = parse_int(str, &ack);

    if( !ack || num < 0 ) {
        mce_log(LL_WARN, "%s: invalid timeout value '%s'", led_pattern_name(self), str);
        led_pattern_set_valid(self, false);
    }
    else {
        self->ps_hybris_timeout = num;
    }
}

static void
led_pattern_parse_on_period(LedPattern *self, const char *str)
{
    bool ack = false;
    gint num = parse_int(str, &ack);

    if( !ack || num < 0 ) {
        mce_log(LL_WARN, "%s: invalid on_period value '%s'", led_pattern_name(self), str);
        led_pattern_set_valid(self, false);
    }
    else {
        self->ps_hybris_on_period = num;
    }
}

static void
led_pattern_parse_off_period(LedPattern *self, const char *str)
{
    bool ack = false;
    gint num = parse_int(str, &ack);

    if( !ack || num < 0 ) {
        mce_log(LL_WARN, "%s: invalid off_period value '%s'", led_pattern_name(self), str);
        led_pattern_set_valid(self, false);
    }
    else {
        self->ps_hybris_off_period = num;
    }
}

static void
led_pattern_parse_rgb24(LedPattern *self, const char *str)
{
    bool  ack = false;
    guint num = parse_rgb24(str, &ack);

    if( !ack || num > 0xffffff ) {
        mce_log(LL_WARN, "%s: invalid rgb value '%s'", led_pattern_name(self), str);
        led_pattern_set_valid(self, false);
    }
    else {
        self->ps_hybris_rgb = num;
        self->ps_hybris_led = aw91xxx_led_for_color(num);
    }
}

static bool
led_pattern_load_configuration(LedPattern *self)
{
    gsize   cnt = 0;
    gchar **arr = mce_conf_get_string_list(MCE_CONF_LED_PATTERN_HYBRIS_GROUP, led_pattern_name(self), &cnt);

    if( cnt < HYBRIS_COUNT ) {
        mce_log(LL_WARN, "pattern %s is missing configuration values", led_pattern_name(self));
        led_pattern_set_valid(self, false);
    }
    else {
        led_pattern_parse_priority  (self, arr[HYBRIS_PRIORITY]);
        led_pattern_parse_screen_on (self, arr[HYBRIS_SCREEN_ON]);
        led_pattern_parse_timeout   (self, arr[HYBRIS_TIMEOUT]);
        led_pattern_parse_on_period (self, arr[HYBRIS_ON_PERIOD]);
        led_pattern_parse_off_period(self, arr[HYBRIS_OFF_PERIOD]);
        led_pattern_parse_rgb24     (self, arr[HYBRIS_RGB24]);
    }

    g_strfreev(arr);

    return led_pattern_is_valid(self);
}

static void
led_pattern_load_bytecode(LedPattern *self)
{
    gchar      *source  = NULL;
    EffectCtl  *effect  = NULL;

    // Get effect source code
    if( !(source = mce_conf_get_string(MCE_CONF_LED_PATTERN_AW91XX_GROUP, led_pattern_name(self), NULL)) ) {
        // As a fallback: Generate bytecode on the fly for hybris breathing / panic patterns

        int led       = led_pattern_led(self);
        int ms_on     = led_pattern_on_period(self);
        int ms_off    = led_pattern_off_period(self);
        int ticks_on  = effect_ms_to_ticks(ms_on);
        int ticks_off = effect_ms_to_ticks(ms_off);

        if( ticks_on <= 0 || ticks_off <= 0 )
            goto EXIT;

        int ms_rise   = effect_ticks_to_ms(ticks_on);
        int ms_fall   = effect_ticks_to_ms(ticks_off);

        if( ms_on > 200 && ms_off > 200 )
            source = g_strdup_printf("L%dr%df%dFs", led, ms_rise, ms_fall);
        else
            source = g_strdup_printf("L%dh%dl%dFr", led, ms_rise, ms_fall);
    }

    mce_log(LL_DEBUG, "effect %s: %s", led_pattern_name(self), source);

    // Create effect controller object
    effect = effect_ctl_create(self);

    // Compile source to byte code
    if( !effect_ctl_compile(effect, source) )
        goto EXIT;

    // Attach effect to pattern
    led_pattern_set_effect(self, effect), effect = NULL;

EXIT:
    effect_ctl_delete(effect);
    g_free(source);
}
