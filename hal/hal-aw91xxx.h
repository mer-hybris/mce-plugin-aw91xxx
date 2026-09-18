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

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * HAL_AW91XXX
 * ------------------------------------------------------------------------- */

bool hal_aw91xxx_init                (void);
void hal_aw91xxx_quit                (void);
void hal_aw91xxx_indicator_set_active(const char *pattern, bool active);

#endif /* HAL_AW91XXX_H_ */
