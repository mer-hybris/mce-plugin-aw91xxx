/** @file hal-aw91xxx.h
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef  HAL_AW91XXX_H_
# define HAL_AW91XXX_H_

# include <stdbool.h>
# include <stdint.h>

/* ========================================================================= *
 * Configuration
 * ========================================================================= */

#define EFFECT_TICK_MS 50

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct EffectMgr  EffectMgr;
typedef struct LedPattern LedPattern;
typedef struct EffectCtl  EffectCtl;

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
typedef struct Aw91xxxLedsState {
    uint8_t ls_active[AW91XXX_LED_COUNT];
} Aw91xxxLedsState;

/** Value range for aw91xxx "dim" sysfs control */
enum {
    AW91XXX_DIM_MIN = 0x00,
    AW91XXX_DIM_MAX = 0xff,
};

/* ========================================================================= *
 * Inline helpers
 * ========================================================================= */

static inline int effect_ticks_to_ms(int ticks)
{
    // Negative values are preserved as -1
    return ticks < 0 ? -1 : ticks <= 0 ? 0 : ticks * EFFECT_TICK_MS;
}

static inline unsigned effect_ms_to_ticks(int ms)
{
    // Negative values are preserved as -1
    return ms < 0 ? -1 : ms <= 0 ? 0 : (ms + EFFECT_TICK_MS - 1) / EFFECT_TICK_MS;
}

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * AW91XXX_SYSFS
 * ------------------------------------------------------------------------- */

void aw91xxx_sysfs_quit(void);
bool aw91xxx_sysfs_init(void);

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

#endif /* HAL_AW91XXX_H_ */
