/** @file effect_mgr.h
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef  EFFECT_MGR_H_
# define EFFECT_MGR_H_

# include <stdbool.h>

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct LedPattern LedPattern;
typedef struct EffectMgr  EffectMgr;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

void        effect_mgr_set_breathing_allowed(EffectMgr *self, bool breathing_allowed);
void        effect_mgr_rethink_state        (EffectMgr *self);
EffectMgr  *effect_mgr_create               (void);
void        effect_mgr_delete               (EffectMgr *self);
LedPattern *effect_mgr_lookup_pattern       (EffectMgr *self, const char *name);
void        effect_mgr_unload_patterns      (EffectMgr *self);

#endif /* EFFECT_MGR_H_ */
