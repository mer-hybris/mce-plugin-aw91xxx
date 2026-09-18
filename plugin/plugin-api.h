/** @file plugin-api.h
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2013 - 2017 Jolla Ltd.
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

/* FIXME: This header is included in sourcetrees of both mce and
 *        mce-plugin-aw91xxx. For now it must be kept in sync
 *        manually.
 */

#ifndef  MCE_HYBRIS_H_
# define MCE_HYBRIS_H_

# ifndef MCE_HYBRIS_INTERNAL
#  define MCE_HYBRIS_INTERNAL 0
# endif

# include <stdint.h>
# include <stdbool.h>

# ifdef __cplusplus
extern "C" {
# endif

# pragma GCC visibility push(default)

/* - - - - - - - - - - - - - - - - - - - *
 * frame buffer power state
 * - - - - - - - - - - - - - - - - - - - */

bool mce_hybris_framebuffer_init(void);
void mce_hybris_framebuffer_quit(void);
bool mce_hybris_framebuffer_set_power(bool on);

/* - - - - - - - - - - - - - - - - - - - *
 * display backlight brightness
 * - - - - - - - - - - - - - - - - - - - */

bool mce_hybris_backlight_init(void);
void mce_hybris_backlight_quit(void);
bool mce_hybris_backlight_set_brightness(int level);

/* - - - - - - - - - - - - - - - - - - - *
 * keypad backlight brightness
 * - - - - - - - - - - - - - - - - - - - */

bool mce_hybris_keypad_set_brightness(int level);
void mce_hybris_keypad_quit(void);
bool mce_hybris_keypad_init(void);

/* - - - - - - - - - - - - - - - - - - - *
 * indicator led pattern
 * - - - - - - - - - - - - - - - - - - - */

bool mce_hybris_indicator_init(void);
void mce_hybris_indicator_quit(void);
bool mce_hybris_indicator_set_pattern(int r, int g, int b, int ms_on, int ms_off);
void mce_hybris_indicator_enable_breathing(bool enable);
bool mce_hybris_indicator_set_brightness(int level);
bool mce_hybris_indicator_can_breathe(void);
void mce_hybris_indicator_set_active(const char *pattern, bool active);

/* - - - - - - - - - - - - - - - - - - - *
 * proximity sensor
 * - - - - - - - - - - - - - - - - - - - */

typedef void (*mce_hybris_ps_fn)(int64_t timestamp, float distance);

bool mce_hybris_ps_init(void);
void mce_hybris_ps_quit(void);
bool mce_hybris_ps_set_active(bool active);
bool mce_hybris_ps_set_callback(mce_hybris_ps_fn cb);

/* - - - - - - - - - - - - - - - - - - - *
 * ambient light sensor
 * - - - - - - - - - - - - - - - - - - - */

typedef void (*mce_hybris_als_fn)(int64_t timestamp, float light);

bool mce_hybris_als_init(void);
void mce_hybris_als_quit(void);
bool mce_hybris_als_set_active(bool active);
bool mce_hybris_als_set_callback(mce_hybris_als_fn cb);

/* - - - - - - - - - - - - - - - - - - - *
 * generic
 * - - - - - - - - - - - - - - - - - - - */

void mce_hybris_quit(void);

/* - - - - - - - - - - - - - - - - - - - *
 * internal to module <--> plugin
 * - - - - - - - - - - - - - - - - - - - */

# if MCE_HYBRIS_INTERNAL >= 1
typedef void (*mce_hybris_log_fn)(int lev, const char *file, const char *func,
                                  const char *text);
# endif

# if MCE_HYBRIS_INTERNAL >= 2
void mce_hybris_set_log_hook(mce_hybris_log_fn cb);
void mce_hybris_ps_set_hook(mce_hybris_ps_fn cb);
void mce_hybris_als_set_hook(mce_hybris_als_fn cb);
# endif

# pragma GCC visibility pop

# ifdef __cplusplus
};
# endif

#endif /* MCE_HYBRIS_H_ */
