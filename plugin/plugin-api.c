/** @file plugin-api.c
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2013 - 2017 Jolla Ltd.
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

/* ========================================================================= *
 * MCE aw91xxx LED plugin
 *
 * "mce-plugin-aw91xxx" implements hybris-plugin compatible API for use
 * in devices that have aw91xxx based set of 5 indicator LEDs.
 *
 * The idea of a "hybris-plugin" (this package) is shortly:
 *
 * - it uses no mce functions or data types
 * - it can be compiled independently from mce
 *
 * And the idea of "hybris-module" (part of mce) is:
 *
 * - it contains functions with the same names as "hybris-plugin"
 * - if called, the functions will load & call "hybris-plugin" code
 * - if "hybris-plugin" is not present "hybris-module" functions still
 *   work, but return failures for everything
 *
 * Put together:
 *
 * - mce code can assume that hybris-plugin code is always available and
 *   callable during hw probing activity
 * - if hybris plugin is not installed / not compatible with underlying
 *   hw, failures will be reported and mce can try other existing ways to
 *   proble for available hw controls
 * ========================================================================= */

#include "plugin-api.h"

#include "../hal/hal-aw91xxx.h"

/* ========================================================================= *
 * PLUGIN_LEDS_API
 * ========================================================================= */

/** Initialize indicator led functionality
 *
 * @return true on success, false on failure
 */
bool
mce_hybris_indicator_init(void)
{
    return hal_aw91xxx_init();
}

/** Release indicator led functionality
 */
void
mce_hybris_indicator_quit(void)
{
    hal_aw91xxx_quit();
}

/** Set indicator led pattern
 *
 * @param r     red intensity 0 ... 255
 * @param g     green intensity 0 ... 255
 * @param b     blue intensity 0 ... 255
 * @param ms_on milliseconds to keep the led on, or 0 for no flashing
 * @param ms_on milliseconds to keep the led off, or 0 for no flashing
 *
 * @return true on success, false on failure
 */
bool
mce_hybris_indicator_set_pattern(int r, int g, int b, int ms_on, int ms_off)
{
    (void)r;
    (void)g;
    (void)b;
    (void)ms_on;
    (void)ms_off;

    return true;
}

/** Query if currently active led backend can support breathing
 *
 * @return true if breathing can be requested, false otherwise
 */
bool
mce_hybris_indicator_can_breathe(void)
{
    return false;
}

/** Enable/disable sw breathing
 *
 * @param enable true to enable sw breathing, false to disable
 */
void
mce_hybris_indicator_enable_breathing(bool enable)
{
    (void)enable;
}

/** Set indicator led brightness
 *
 * @param level 1=minimum, 255=maximum
 *
 * @return true on success, or false on failure
 */
bool
mce_hybris_indicator_set_brightness(int level)
{
    (void)level;

    return true;
}

void
mce_hybris_indicator_set_active(const char *pattern, bool active)
{
    hal_aw91xxx_indicator_set_active(pattern, active);
}

/* ========================================================================= *
 * PLUGIN_API
 * ========================================================================= */

/** Release all resources allocated by this module
 */
void
mce_hybris_quit(void)
{
    mce_hybris_indicator_quit();
}
