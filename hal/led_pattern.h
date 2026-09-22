/** @file led_pattern.h
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef  LED_PATTERN_H_
# define LED_PATTERN_H_

# include <stdbool.h>

/* ========================================================================= *
 * Types & Constants
 * ========================================================================= */

#define EFFECT_TICK_MS              50

/* MCE patterns that need special treatment */
#define MCE_LED_PATTERN_POWER_OFF "PatternPowerOff"

/** MCE pattern configuration value indices */
enum {
    HYBRIS_PRIORITY,
    HYBRIS_SCREEN_ON,
    HYBRIS_TIMEOUT,
    HYBRIS_ON_PERIOD,
    HYBRIS_OFF_PERIOD,
    HYBRIS_RGB24,
    HYBRIS_COUNT
};

typedef struct EffectMgr  EffectMgr;
typedef struct LedPattern LedPattern;
typedef struct EffectCtl  EffectCtl;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * LED_PATTERN
 * ------------------------------------------------------------------------- */

LedPattern *led_pattern_create    (EffectMgr *manager, const char *pattern);
void        led_pattern_delete    (LedPattern *self);
EffectCtl  *led_pattern_get_effect(const LedPattern *self);
void        led_pattern_set_effect(LedPattern *self, EffectCtl *effect);
bool        led_pattern_is_valid  (const LedPattern *self);
bool        led_pattern_is_active (const LedPattern *self);
void        led_pattern_set_active(LedPattern *self, bool active);
const char *led_pattern_name      (const LedPattern *self);
int         led_pattern_priority  (const LedPattern *self);
int         led_pattern_screen_on (const LedPattern *self);
int         led_pattern_timeout   (const LedPattern *self);
int         led_pattern_on_period (const LedPattern *self);
int         led_pattern_off_period(const LedPattern *self);
unsigned    led_pattern_rgb       (const LedPattern *self);
unsigned    led_pattern_led       (const LedPattern *self);

#endif /* LED_PATTERN_H_ */
