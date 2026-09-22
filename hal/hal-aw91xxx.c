/** @file hal-aw91xxx.c
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include "hal-aw91xxx.h"

#include "effect_mgr.h"
#include "led_pattern.h"

#include "../plugin/plugin-config.h"
#include "../plugin/plugin-logging.h"

#include <fcntl.h>
#include <stdio.h>

#define numof(array) (sizeof (array) / sizeof *(array))

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

static uint64_t boottime(void);

/* ------------------------------------------------------------------------- *
 * AW91XXX_SYSFS
 * ------------------------------------------------------------------------- */

void        aw91xxx_sysfs_quit          (void);
bool        aw91xxx_sysfs_init          (void);
static void aw91xxx_sysfs_set_brightness(Aw91xxxLedIndex index, int val);

/* ------------------------------------------------------------------------- *
 * AW91XXX_LED
 * ------------------------------------------------------------------------- */

bool             aw91xxx_led_is_valid (Aw91xxxLedIndex index);
const char      *aw91xxx_led_name     (Aw91xxxLedIndex index);
Aw91xxxLedIndex  aw91xxx_led_for_color(unsigned rgb);

/* ------------------------------------------------------------------------- *
 * AW91XXX_LEDS
 * ------------------------------------------------------------------------- */

void aw91xxx_leds_set_state  (const Aw91xxxLedsState *state);
void aw91xxx_leds_reset_state(void);

/* ------------------------------------------------------------------------- *
 * HAL_AW91XXX
 * ------------------------------------------------------------------------- */

bool hal_aw91xxx_init                      (void);
void hal_aw91xxx_quit                      (void);
void hal_aw91xxx_indicator_set_active      (const char *name, bool active);
void hal_aw91xxx_indicator_enable_breathing(bool enable);
void hal_aw91xxx_indicator_set_brightness  (int level);

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

/** Dynamic led brightness adjustment
 *
 * Defaults to using full hw brightness.
 */
static int aw91xxx_led_brightness = AW91XXX_DIM_MAX;

/** Static led brightness calibration
 *
 * Defaults to using full hw brightness range.
 */
static int aw91xxx_led_calibration[AW91XXX_LED_COUNT] = {
    [AW91XXX_LED_RED]    = AW91XXX_DIM_MAX,
    [AW91XXX_LED_ORANGE] = AW91XXX_DIM_MAX,
    [AW91XXX_LED_YELLOW] = AW91XXX_DIM_MAX,
    [AW91XXX_LED_GREEN]  = AW91XXX_DIM_MAX,
    [AW91XXX_LED_BLUE]   = AW91XXX_DIM_MAX,
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
 * Code
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

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

static uint64_t boottime(void)
{
    struct timespec ts = {};
    clock_gettime(CLOCK_BOOTTIME, &ts);
    uint64_t res = 0;
    res += ts.tv_sec;
    res *= 1000u;
    res += ts.tv_nsec / 1000000u;
    return res;
}

/* ------------------------------------------------------------------------- *
 * AW91XXX_SYSFS
 * ------------------------------------------------------------------------- */

static const char aw91xxx_sysfs_dim_path[] = "/sys/class/leds/aw91xxx_led/dim";
static int        aw91xxx_sysfs_dim_fd     = -1;
static int        aw91xxx_sysfs_dim_state[AW91XXX_LED_COUNT];

/** Open "dim" sysfs control file
 */
void
aw91xxx_sysfs_quit(void)
{
    if( aw91xxx_sysfs_dim_fd != -1 )
        close(aw91xxx_sysfs_dim_fd), aw91xxx_sysfs_dim_fd = -1;
}

/** Close "dim" sysfs control file
 */
bool
aw91xxx_sysfs_init(void)
{
    /* Parse aw91xxx LED brightness configuration
     *
     * For more info see example at: inifiles/70-led-brightness-aw91xxx.ini
     */
    for( size_t i = 0; i < AW91XXX_LED_COUNT; ++i ) {
        static const char group[] = MCE_CONF_AW91XX_LED_BRIGHTNESS_GROUP;

        static const char * const keys[AW91XXX_LED_COUNT] = {
            [AW91XXX_LED_RED]     = MCE_CONF_AW91XX_LED_BRIGHTNESS_RED,
            [AW91XXX_LED_ORANGE]  = MCE_CONF_AW91XX_LED_BRIGHTNESS_ORANGE,
            [AW91XXX_LED_YELLOW]  = MCE_CONF_AW91XX_LED_BRIGHTNESS_YELLOW,
            [AW91XXX_LED_GREEN]   = MCE_CONF_AW91XX_LED_BRIGHTNESS_GREEN,
            [AW91XXX_LED_BLUE]    = MCE_CONF_AW91XX_LED_BRIGHTNESS_BLUE,
        };

        gint val = mce_conf_get_int(group, keys[i], AW91XXX_DIM_MAX);

        if( val < AW91XXX_DIM_MIN )
            val = AW91XXX_DIM_MIN;
        else if( val > AW91XXX_DIM_MAX )
            val = AW91XXX_DIM_MAX;

        aw91xxx_led_calibration[i] = val;

        mce_log(LL_DEBUG, "[%s] %s = %d", group, keys[i], val);
    }

    /* Clear sysfs level cached brightness values */
    for( unsigned led = 0; led < AW91XXX_LED_COUNT; ++led )
        aw91xxx_sysfs_dim_state[led] = -1;

    if( aw91xxx_sysfs_dim_fd == -1 ) {
        if( (aw91xxx_sysfs_dim_fd = open(aw91xxx_sysfs_dim_path, O_WRONLY)) == -1 )
            mce_log(LL_ERR, "%s: open: %m", aw91xxx_sysfs_dim_path);
    }

    return aw91xxx_sysfs_dim_fd != -1;
}

static inline int
aw91xxx_sysfs_scale_brightness(int val, int new_max)
{
    return  (val * new_max + AW91XXX_DIM_MAX / 2) / AW91XXX_DIM_MAX;
}

/** Write to "dim" sysfs control file
 */
static void
aw91xxx_sysfs_set_brightness(Aw91xxxLedIndex index, int val)
{
    if( aw91xxx_sysfs_dim_fd != -1 && aw91xxx_led_is_valid(index) ) {
        if( val < AW91XXX_DIM_MIN )
            val = AW91XXX_DIM_MIN;
        else if( val > AW91XXX_DIM_MAX )
            val = AW91XXX_DIM_MAX;

        if( val > 0 ) {
            val = aw91xxx_sysfs_scale_brightness(val, aw91xxx_led_calibration[index]);
            val = aw91xxx_sysfs_scale_brightness(val, aw91xxx_led_brightness);
        }

        if( aw91xxx_sysfs_dim_state[index] != val ) {
            aw91xxx_sysfs_dim_state[index] = val;

            // Note: dim_store() in kernel driver accepts only hexadecimal values!
            char txt[32];
            snprintf(txt, sizeof txt, "0x%02x 0x%02x", (int)index, val);
            uint64_t t = boottime();
            if( write(aw91xxx_sysfs_dim_fd, txt, strlen(txt)) == -1 ) {
                // dontcare (and driver never returns error anyway)
            }
            t = boottime() - t;

            mce_log(LL_DEBUG, "%s: brightness: %d (T+%u)", aw91xxx_led_name(index), val, (unsigned)t);
        }
    }
}

/* ------------------------------------------------------------------------- *
 * AW91XXX_LED
 * ------------------------------------------------------------------------- */

bool
aw91xxx_led_is_valid(Aw91xxxLedIndex index)
{
    return (unsigned)index < (unsigned)AW91XXX_LED_COUNT;
}

/** Get human readable name for led index
 */
const char *
aw91xxx_led_name(Aw91xxxLedIndex index)
{
    const char *name = index < numof(aw91xxx_led_name_lut) ? aw91xxx_led_name_lut[index] : NULL;
    return name ?: "LED_INVALID";
}

/** Find led that is the closest match to color
 */
Aw91xxxLedIndex
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
void
aw91xxx_leds_set_state(const Aw91xxxLedsState *state)
{
    Aw91xxxLedsState *cached = &aw91xxx_leds_state;

    for( size_t i = 0; i < AW91XXX_LED_COUNT; ++i ) {
        if( cached->ls_active[i] == state->ls_active[i] )
            continue;

        mce_log(LL_DEBUG, "%s: active: %d -> %d", aw91xxx_led_name(i), cached->ls_active[i], state->ls_active[i]);
        aw91xxx_sysfs_set_brightness(i, (cached->ls_active[i] = state->ls_active[i]));
    }
}

/** Reset both internal and sysfs control state
 */
void
aw91xxx_leds_reset_state(void)
{
    Aw91xxxLedsState *cached = &aw91xxx_leds_state;

    for( size_t i = 0; i < AW91XXX_LED_COUNT; ++i ) {
        cached->ls_active[i] = AW91XXX_DIM_MIN;
        aw91xxx_sysfs_set_brightness(i, AW91XXX_DIM_MIN);
    }
}

/* ------------------------------------------------------------------------- *
 * HAL_AW91XXX
 * ------------------------------------------------------------------------- */

static EffectMgr *effect_manager = NULL;

bool
hal_aw91xxx_init(void)
{
    bool ack = false;

    if( !aw91xxx_sysfs_init() )
        goto EXIT;

    if( !effect_manager )
        effect_manager = effect_mgr_create();

    ack = true;

EXIT:
    return ack;
}

void
hal_aw91xxx_quit(void)
{
    effect_mgr_delete(effect_manager), effect_manager = NULL;

    aw91xxx_sysfs_quit();
}

void
hal_aw91xxx_indicator_set_active(const char *name, bool active)
{
    LedPattern *pattern = effect_mgr_lookup_pattern(effect_manager, name);
    if( pattern ) {
        led_pattern_set_active(pattern, active);
    }
}

void
hal_aw91xxx_indicator_enable_breathing(bool enable)
{
    effect_mgr_set_breathing_allowed(effect_manager, enable);
}

void
hal_aw91xxx_indicator_set_brightness(int level)
{
    if( level < AW91XXX_DIM_MIN )
        level = AW91XXX_DIM_MAX;
    else if( level > AW91XXX_DIM_MAX )
        level = AW91XXX_DIM_MAX;

    if( aw91xxx_led_brightness != level ) {
        mce_log(LL_DEBUG, "brightness: %d -> %d", aw91xxx_led_brightness, level);
        aw91xxx_led_brightness = level;

        /* Re-apply current (non-zero) logical brightness values */
        Aw91xxxLedsState current = aw91xxx_leds_state;
        memset(&aw91xxx_leds_state, 0, sizeof aw91xxx_leds_state);
        aw91xxx_leds_set_state(&current);
    }
}
