/** @file hal-aw91xxx.c
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include "hal-aw91xxx.h"

#include "../plugin/plugin-config.h"
#include "../plugin/plugin-logging.h"

#include <fcntl.h>
#include <stdio.h>

#define numof(array) (sizeof (array) / sizeof *(array))

/* ========================================================================= *
 * Types & Constants
 * ========================================================================= */

/* MCE patterns that need special treatment */
#define MCE_LED_PATTERN_POWER_OFF "PatternPowerOff"

/** MCE pattern configuration value indices */
enum {
    CONFVAL_PRIORITY,
    CONFVAL_SCREEN_ON,
    CONFVAL_TIMEOUT,
    CONFVAL_ON_PERIOD,
    CONFVAL_OFF_PERIOD,
    CONFVAL_RGB24,
    CONFVAL_COUNT
};

/** MCE Pattern state */
typedef struct {
    gchar    *ps_pattern;
    unsigned  ps_rgb;
    unsigned  ps_led;
    bool      ps_active;
} McePatternState;

/** Enumeration for the 5 aw91xxx leds */
typedef enum {
    AW91XXX_LED_RED,
    AW91XXX_LED_ORANGE,
    AW91XXX_LED_YELLOW,
    AW91XXX_LED_GREEN,
    AW91XXX_LED_BLUE,
    AW91XXX_LED_COUNT,
    AW91XXX_LED_INVALID = AW91XXX_LED_COUNT,
    AW91XXX_LED_DEFAULT = AW91XXX_LED_YELLOW,
} Aw91xxxLedIndex;

/** Output control state for aw91xxx leds */
typedef struct {
    bool ls_active[AW91XXX_LED_COUNT];
} Aw91xxxLedsState;

/** Value range for aw91xxx "dim" sysfs control */
enum {
    AW91XXX_DIM_MIN = 0x00,
    AW91XXX_DIM_MAX = 0xff,
};

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * AW91XXX_SYSFS
 * ------------------------------------------------------------------------- */

static void aw91xxx_sysfs_quit          (void);
static bool aw91xxx_sysfs_init          (void);
static void aw91xxx_sysfs_set_brightness(Aw91xxxLedIndex index, int val);

/* ------------------------------------------------------------------------- *
 * AW91XXX_LED
 * ------------------------------------------------------------------------- */

static const char      *aw91xxx_led_name     (Aw91xxxLedIndex index);
static Aw91xxxLedIndex  aw91xxx_led_for_color(unsigned rgb);

/* ------------------------------------------------------------------------- *
 * AW91XXX_LEDS
 * ------------------------------------------------------------------------- */

static void aw91xxx_leds_set_state  (const Aw91xxxLedsState *state);
static void aw91xxx_leds_reset_state(void);

/* ------------------------------------------------------------------------- *
 * MCE_PATTERN
 * ------------------------------------------------------------------------- */

static void mce_pattern_set_state(const char *pattern, bool active);

/* ------------------------------------------------------------------------- *
 * MCE_PATTERNS
 * ------------------------------------------------------------------------- */

static McePatternState *mce_patterns_lookup       (const char *pattern);
static void             mce_patterns_sync_to_leds (void);
static void             mce_patterns_unload_config(void);
static void             mce_patterns_load_config  (void);

/* ------------------------------------------------------------------------- *
 * HAL_AW91XXX
 * ------------------------------------------------------------------------- */

bool hal_aw91xxx_init                (void);
void hal_aw91xxx_quit                (void);
void hal_aw91xxx_indicator_set_active(const char *pattern, bool active);

/* ========================================================================= *
 * Data
 * ========================================================================= */

/** Approximate led colors for config matching */
static const unsigned aw91xxx_led_color[AW91XXX_LED_COUNT] = {
    [AW91XXX_LED_RED]    = 0xff0000,
    [AW91XXX_LED_ORANGE] = 0xff8000,
    [AW91XXX_LED_YELLOW] = 0xffff00,
    [AW91XXX_LED_GREEN]  = 0x00ff00,
    [AW91XXX_LED_BLUE]   = 0x0000ff,
};

/** Static led brightness calibration
 *
 * Brightness of the leds is out of balance: blue is way too bright and
 * green is too dim -> tune maximums for each led based on eyeballing.
 */
static int aw91xxx_led_calibration[AW91XXX_LED_COUNT] = {
    [AW91XXX_LED_RED]    =  72,
    [AW91XXX_LED_ORANGE] =  70,
    [AW91XXX_LED_YELLOW] =  78,
    [AW91XXX_LED_GREEN]  = 255,
    [AW91XXX_LED_BLUE]   =  10,
};

/** Human readable names for leds, use via #aw91xxx_led_name() */
static const char * const aw91xxx_led_name_lut[AW91XXX_LED_COUNT] = {
    [AW91XXX_LED_RED]     = "LED_RED",
    [AW91XXX_LED_ORANGE]  = "LED_ORANGE",
    [AW91XXX_LED_YELLOW]  = "LED_YELLOW",
    [AW91XXX_LED_GREEN]   = "LED_GREEN",
    [AW91XXX_LED_BLUE]    = "LED_BLUE",
};

/* ========================================================================= *
 * Inline helpers
 * ========================================================================= */

static inline const char *bool_repr(bool val)
{
    return val ? "true" : "false";
}

static inline int EXTRACT_R(unsigned rgb)
{
    return (rgb >> 16) & 255;
}

static inline int EXTRACT_G(unsigned rgb)
{
    return (rgb >> 8) & 255;
}

static inline int EXTRACT_B(unsigned rgb)
{
    return (rgb >> 0) & 255;
}

/* ========================================================================= *
 * Code
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * AW91XXX_SYSFS
 * ------------------------------------------------------------------------- */

static const char aw91xxx_sysfs_dim_path[] = "/sys/class/leds/aw91xxx_led/dim";
static int        aw91xxx_sysfs_dim_fd     = -1;

/** Open "dim" sysfs control file
 */
static void
aw91xxx_sysfs_quit(void)
{
    if( aw91xxx_sysfs_dim_fd != -1 )
        close(aw91xxx_sysfs_dim_fd), aw91xxx_sysfs_dim_fd = -1;
}

/** Close "dim" sysfs control file
 */
static bool
aw91xxx_sysfs_init(void)
{
    if( aw91xxx_sysfs_dim_fd == -1 ) {
        if( (aw91xxx_sysfs_dim_fd = open(aw91xxx_sysfs_dim_path, O_WRONLY)) == -1 )
            mce_log(LL_ERR, "%s: open: %m", aw91xxx_sysfs_dim_path);
    }

    return aw91xxx_sysfs_dim_fd != -1;
}

/** Write to "dim" sysfs control file
 */
static void
aw91xxx_sysfs_set_brightness(Aw91xxxLedIndex index, int val)
{
    if( aw91xxx_sysfs_dim_fd != -1 ) {
        if( val < AW91XXX_DIM_MIN )
            val = AW91XXX_DIM_MIN;
        else if( val > AW91XXX_DIM_MAX )
            val = AW91XXX_DIM_MAX;

        if( val > 0 )
            val = aw91xxx_led_calibration[index] * val / AW91XXX_DIM_MAX;

        mce_log(LL_DEBUG, "%s: brightness: %d", aw91xxx_led_name(index), val);

        // Note: dim_store() in kernel driver accepts only hexadecimal values!
        char txt[32];
        snprintf(txt, sizeof txt, "0x%02x 0x%02x", (int)index, val);
        if( write(aw91xxx_sysfs_dim_fd, txt, strlen(txt)) == -1 ) {
            // dontcare (and driver never returns error anyway)
        }
    }
}

/* ------------------------------------------------------------------------- *
 * AW91XXX_LED
 * ------------------------------------------------------------------------- */

/** Get human readable name for led index
 */
static const char *
aw91xxx_led_name(Aw91xxxLedIndex index)
{
    const char *name = index < numof(aw91xxx_led_name_lut) ? aw91xxx_led_name_lut[index] : NULL;
    return name ?: "LED_INVALID";
}

/** Find led that is the closest match to color
 */
static Aw91xxxLedIndex
aw91xxx_led_for_color(unsigned rgb)
{
    Aw91xxxLedIndex index = AW91XXX_LED_DEFAULT;
    int             error = INT_MAX;

    int r = EXTRACT_R(rgb);
    int g = EXTRACT_G(rgb);
    int b = EXTRACT_B(rgb);

    for( size_t i = 0; i < AW91XXX_LED_COUNT; ++i ) {
        unsigned led_rgb = aw91xxx_led_color[i];

        int e_r = EXTRACT_R(led_rgb) - r;
        int e_g = EXTRACT_G(led_rgb) - g;
        int e_b = EXTRACT_B(led_rgb) - b;

        int e2 = e_r * e_r + e_g * e_g + e_b * e_b;

        if( error > e2 )
            error = e2, index = i;
    }

    return index;
}

/* ------------------------------------------------------------------------- *
 * AW91XXX_LEDS
 * ------------------------------------------------------------------------- */

/** Cached led control state */
static Aw91xxxLedsState aw91xxx_leds_state = {};

/** Sync internal state to sysfs control
 */
static void
aw91xxx_leds_set_state(const Aw91xxxLedsState *state)
{
    Aw91xxxLedsState *cached = &aw91xxx_leds_state;

    for( size_t i = 0; i < AW91XXX_LED_COUNT; ++i ) {
        if( cached->ls_active[i] == state->ls_active[i] )
            continue;

        mce_log(LL_DEBUG, "%s: active: %s -> %s", aw91xxx_led_name(i),
                bool_repr(cached->ls_active[i]),
                bool_repr(state->ls_active[i]));

        if( (cached->ls_active[i] = state->ls_active[i]) ) {
            aw91xxx_sysfs_set_brightness(i, AW91XXX_DIM_MAX);
        }
        else {
            aw91xxx_sysfs_set_brightness(i, AW91XXX_DIM_MIN);
        }
    }
}

/** Reset both internal and sysfs control state
 */
static void
aw91xxx_leds_reset_state(void)
{
    Aw91xxxLedsState *cached = &aw91xxx_leds_state;

    for( size_t i = 0; i < AW91XXX_LED_COUNT; ++i ) {
        cached->ls_active[i] = false;
        aw91xxx_sysfs_set_brightness(i, 0);
    }
}

/* ------------------------------------------------------------------------- *
 * MCE_PATTERN
 * ------------------------------------------------------------------------- */

/** Mark pattern as active / inactive
 */
static void
mce_pattern_set_state(const char *pattern, bool active)
{
    McePatternState *state = mce_patterns_lookup(pattern);

    if( state && state->ps_active != active ) {
        mce_log(LL_DEBUG, "%s: active: %s -> %s", pattern, bool_repr(state->ps_active), bool_repr(active));

        state->ps_active = active;

        mce_patterns_sync_to_leds();
    }
}

/* ------------------------------------------------------------------------- *
 * MCE_PATTERNS
 * ------------------------------------------------------------------------- */

/** Pattern lookup table, derived from mce side pattern configuration */
static McePatternState *mce_mce_patterns_lut = NULL;
static gsize            mce_mce_patterns_cnt = 0;

/** Lookup pattern index
 */
static McePatternState *
mce_patterns_lookup(const char *pattern)
{
    for( size_t i = 0; i < mce_mce_patterns_cnt; ++i )
        if( !strcmp(mce_mce_patterns_lut[i].ps_pattern, pattern) )
            return &mce_mce_patterns_lut[i];

    mce_log(LL_WARN, "pattern %s is unknown", pattern);
    return NULL;
}

/** Map pattern state to physical leds */
static void
mce_patterns_sync_to_leds(void)
{
    /* Note: We have N:1 PATTERN -> LED mapping
     *
     * 1) Start from all leds "off" state,
     * 2) Mark leds for active patterns "on", and
     * 3) Sync to sysfs.
     */
    Aw91xxxLedsState state = {};

    for( size_t i = 0; i < mce_mce_patterns_cnt; ++i )
        if( mce_mce_patterns_lut[i].ps_active )
            state.ls_active[mce_mce_patterns_lut[i].ps_led] = true;

    aw91xxx_leds_set_state(&state);
}

static void
mce_patterns_unload_config(void)
{
    /* Disable all patterns except PatternPowerOff
     */
    for( size_t i = 0; i < mce_mce_patterns_cnt; ++i )
        if( strcmp(mce_mce_patterns_lut[i].ps_pattern, MCE_LED_PATTERN_POWER_OFF) )
            mce_mce_patterns_lut[i].ps_active = false;
    mce_patterns_sync_to_leds();

    /* Free memory
     */
    for( gsize i = 0; i < mce_mce_patterns_cnt; ++i )
        g_free(mce_mce_patterns_lut[i].ps_pattern);

    g_free(mce_mce_patterns_lut), mce_mce_patterns_lut = 0, mce_mce_patterns_cnt = 0;
}

static void
mce_patterns_load_config(void)
{
    /* Discarde any old data we might have
     */
    mce_patterns_unload_config();

    /* Parse Hybris led configuration and generate hybris color -> led mapping
     */
    gsize   key_cnt = 0;
    gchar **key_arr = mce_conf_get_keys(MCE_CONF_LED_PATTERN_HYBRIS_GROUP, &key_cnt);

    mce_mce_patterns_lut = g_malloc0_n(key_cnt, sizeof *mce_mce_patterns_lut);

    for( gsize i = 0; i < key_cnt; ++i ) {
        const char *pattern = key_arr[i];

        gsize   val_cnt = 0;
        gchar **val_arr = mce_conf_get_string_list(MCE_CONF_LED_PATTERN_HYBRIS_GROUP, pattern, &val_cnt);

        if( val_cnt < CONFVAL_COUNT ) {
            mce_log(LL_WARN, "pattern %s is malformed", pattern);
        }
        else {
            char     *rgb_txt = val_arr[CONFVAL_RGB24];
            char     *rgb_end = NULL;
            unsigned  rgb_val = strtoul(rgb_txt, &rgb_end, 16);

            if( rgb_end <= rgb_txt || *rgb_end != 0 ) {
                mce_log(LL_WARN, "pattern %s has invalid rgb value: '%s'", pattern, rgb_txt);
            }
            else {
                McePatternState *state = &mce_mce_patterns_lut[mce_mce_patterns_cnt++];
                Aw91xxxLedIndex  index = aw91xxx_led_for_color(rgb_val);

                mce_log(LL_DEBUG, "%s: rgb 0x%06x -> %s", pattern, rgb_val, aw91xxx_led_name(index));

                state->ps_pattern = g_strdup(pattern);
                state->ps_rgb     = rgb_val;
                state->ps_led     = index;
                state->ps_active  = false;
            }
        }
        g_strfreev(val_arr), val_arr = NULL, val_cnt = 0;
    }
    g_strfreev(key_arr), key_arr = NULL, key_cnt = 0;

    /* Reset sysfs bookkeeping and controls to a known state
     */
    aw91xxx_leds_reset_state();
}

/* ------------------------------------------------------------------------- *
 * HAL_AW91XXX
 * ------------------------------------------------------------------------- */

bool
hal_aw91xxx_init(void)
{
    bool ack = false;

    if( !aw91xxx_sysfs_init() )
        goto EXIT;

    mce_patterns_load_config();

    ack = true;

EXIT:
    return ack;
}

void
hal_aw91xxx_quit(void)
{
    mce_patterns_unload_config();
    aw91xxx_sysfs_quit();
}

void
hal_aw91xxx_indicator_set_active(const char *pattern, bool active)
{
    mce_pattern_set_state(pattern, active);
}
