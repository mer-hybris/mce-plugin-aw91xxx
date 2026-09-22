/** @file plugin-config.h
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2017 Jolla Ltd.
 * SPDX-FileCopyrightText: (c) 2024 Jollyboys Ltd.
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef  PLUGIN_CONFIG_H_
# define PLUGIN_CONFIG_H_

# include <glib.h>

/* ========================================================================= *
 * Constants
 * ========================================================================= */

#define MCE_CONF_LED_PATTERN_HYBRIS_GROUP "LEDPatternHybris"

#define MCE_CONF_AW91XX_LED_BRIGHTNESS_GROUP  "LEDBrightnesAW91XXX"
#define MCE_CONF_AW91XX_LED_BRIGHTNESS_RED    "Red"
#define MCE_CONF_AW91XX_LED_BRIGHTNESS_ORANGE "Orange"
#define MCE_CONF_AW91XX_LED_BRIGHTNESS_YELLOW "Yellow"
#define MCE_CONF_AW91XX_LED_BRIGHTNESS_GREEN  "Green"
#define MCE_CONF_AW91XX_LED_BRIGHTNESS_BLUE   "Blue"

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * MCE_CONFIG_API
 * ------------------------------------------------------------------------- */

gchar **mce_conf_get_string_list(const gchar *group, const gchar *key, gsize *length);
gchar **mce_conf_get_keys       (const gchar *group, gsize *length);
gint    mce_conf_get_int        (const gchar *group, const gchar *key, const gint defaultval);

#endif /* PLUGIN_CONFIG_H_ */
