/** @file effect_vcpu.h
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef  EFFECT_VCPU_H_
# define EFFECT_VCPU_H_

# include <stdbool.h>
# include <stdint.h>
# include <stddef.h>

/* ========================================================================= *
 * Types
 * ========================================================================= */

typedef struct EffectCtl      EffectCtl;
typedef struct EffectVCPU     EffectVCPU;
typedef struct EffectCompiler EffectCompiler;

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * EFFECT_VCPU
 * ------------------------------------------------------------------------- */

EffectCtl  *effect_vcpu_get_ctl      (const EffectVCPU *self);
int         effect_vcpu_get_output   (const EffectVCPU *self);
void        effect_vcpu_set_active   (EffectVCPU *self, bool active);
bool        effect_vcpu_advance_clock(EffectVCPU *self, unsigned tick);
EffectVCPU *effect_vcpu_create       (EffectCtl *control, unsigned led, const uint8_t *bytecode, size_t codesize);
void        effect_vcpu_delete       (EffectVCPU *self);

/* ------------------------------------------------------------------------- *
 * EFFECT_COMPILER
 * ------------------------------------------------------------------------- */

EffectCompiler *effect_compiler_create      (void);
void            effect_compiler_delete      (EffectCompiler *self);
uint8_t        *effect_compiler_get_bytecode(EffectCompiler *self, unsigned led, size_t *psize);
size_t          effect_compiler_compile     (EffectCompiler *self, const char *srce);

#endif /* EFFECT_VCPU_H_ */
