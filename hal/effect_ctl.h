/** @file effect_ctl.h
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef  EFFECT_CTL_H_
# define EFFECT_CTL_H_

# include <stdbool.h>

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct LedPattern LedPattern;
typedef struct EffectCtl  EffectCtl;
typedef struct EffectVCPU EffectVCPU;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * EFFECT_CTL
 * ------------------------------------------------------------------------- */

EffectVCPU *effect_ctl_get_vcpu     (const EffectCtl *self, unsigned led);
LedPattern *effect_ctl_get_pattern  (const EffectCtl *self);
bool        effect_ctl_get_active   (const EffectCtl *self);
void        effect_ctl_set_active   (EffectCtl *self, bool active);
bool        effect_ctl_advance_clock(EffectCtl *self, unsigned tick);
bool        effect_ctl_compile      (EffectCtl *self, const char *srce);
EffectCtl  *effect_ctl_create       (LedPattern *pattern);
void        effect_ctl_delete       (EffectCtl *self);

#endif // EFFECT_CTL_H_
