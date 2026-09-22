/** @file effect_vcpu.c
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include "effect_vcpu.h"

#include "effect_ctl.h"
#include "hal-aw91xxx.h"
#include "led_pattern.h"

#include "../plugin/plugin-logging.h"

#include <math.h>

#include <glib.h>

#define numof(array) (sizeof (array) / sizeof *(array))

#define effect_vcpu_log(LEV, FMT, ARGS...)\
     mce_log(LEV, "%s: " FMT, effect_vcpu_identity(self), ##ARGS)

/* ========================================================================= *
 * Config
 * ========================================================================= */

#define EFFECT_DEFAULT_AMPLITUDE    AW91XXX_DIM_MAX
#define EFFECT_DEFAULT_COUNT        0 // = unlimited

/* ========================================================================= *
 * Types
 * ========================================================================= */

enum {
    COMMAND_LED         = 'L', // L<led>  ;led = 0-4, (r)ed, (o)range, (y)ellow, (g)reen, (b)lue
    COMMAND_NOP         = 'N', // N
    COMMAND_HALT        = 'H', // H
    COMMAND_RESTART     = 'R', // R
    COMMAND_POWEROFF    = 'X', // X
    COMMAND_EXPIRY      = 'e', // e<ms>
    COMMAND_DELAY       = 'd', // d<ms>   ; "d0" compiles to NOP -> same as "N"
    COMMAND_AMPLITUDE   = 'a', // a<val>  ; val = 0-255
    COMMAND_RISE_TIME   = 'r', // r<ms>
    COMMAND_HIGH_TIME   = 'h', // h<ms>
    COMMAND_FALL_TIME   = 'f', // f<ms>
    COMMAND_LOW_TIME    = 'l', // l<ms>
    COMMAND_COUNT       = 'c', // c<val>  ; val = 0-255, 0=infinite
    COMMAND_FUNCTION    = 'F', // F<func> ; func = (f)ixed (d)elay (r)ectangle (t)riangle (s)ine (f)lare
};

typedef enum {
    FUNCTION_DELAY,
    FUNCTION_FIXED,
    FUNCTION_RECTANGLE,
    FUNCTION_TRIANGLE,
    FUNCTION_SINE,
    FUNCTION_FLARE,
} EffectFunction;

enum {
    OPCODE_HAS_VALUE   = 0x40, // data byte follows
    OPCODE_IS_SETUP    = 0x80, // execute next opcode too

    OPCODE_NOP         = 0x00 | OPCODE_IS_SETUP,
    OPCODE_HALT        = 0x01,
    OPCODE_POWEROFF    = 0x02,
    OPCODE_RESTART     = 0x03 | OPCODE_IS_SETUP,

    OPCODE_EXPIRY      = 0x10 | OPCODE_HAS_VALUE | OPCODE_IS_SETUP,
    OPCODE_DELAY       = 0x11 | OPCODE_HAS_VALUE,

    OPCODE_AMPLITUDE   = 0x20 | OPCODE_HAS_VALUE | OPCODE_IS_SETUP,
    OPCODE_RISE_TIME   = 0x21 | OPCODE_HAS_VALUE | OPCODE_IS_SETUP,
    OPCODE_HIGH_TIME   = 0x22 | OPCODE_HAS_VALUE | OPCODE_IS_SETUP,
    OPCODE_FALL_TIME   = 0x23 | OPCODE_HAS_VALUE | OPCODE_IS_SETUP,
    OPCODE_LOW_TIME    = 0x24 | OPCODE_HAS_VALUE | OPCODE_IS_SETUP,
    OPCODE_COUNT       = 0x25 | OPCODE_HAS_VALUE | OPCODE_IS_SETUP,
    OPCODE_FUNCTION    = 0x26 | OPCODE_HAS_VALUE,
};

struct EffectVCPU {
    // linkage
    EffectCtl *vcp_effect_ctl;
    unsigned   vcp_led;
    gchar     *vcp_identity;

    // setup opcodes
    gint     vcp_setup_amplitude;
    uint32_t vcp_setup_rise_time;
    uint32_t vcp_setup_high_time;
    uint32_t vcp_setup_fall_time;
    uint32_t vcp_setup_low_time;
    gint     vcp_setup_count;
    uint32_t vcp_setup_deactivate;

    // function opcode
    gint     vcp_function_type;
    uint32_t vcp_function_start;
    uint32_t vcp_function_rise_ends;
    uint32_t vcp_function_high_ends;
    uint32_t vcp_function_fall_ends;
    uint32_t vcp_function_low_ends;
    uint32_t vcp_function_repeats;

    // bytecode
    gsize    vcp_codesize;
    uint8_t *vcp_bytecode;
    gsize    vcp_pc;
    bool     vcp_active;
    bool     vcp_halted;
    int      vcp_output;
};

struct EffectCompiler
{
    uint8_t efc_code[AW91XXX_LED_COUNT][256];
    size_t  efc_size[AW91XXX_LED_COUNT];
};

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

static const char *effect_function_name(EffectFunction value);
static const char *effect_opcode_name  (unsigned value);

/* ------------------------------------------------------------------------- *
 * EFFECT_VCPU
 * ------------------------------------------------------------------------- */

EffectCtl         *effect_vcpu_get_ctl                (const EffectVCPU *self);
static const char *effect_vcpu_identity               (const EffectVCPU *self);
int                effect_vcpu_get_output             (const EffectVCPU *self);
static void        effect_vcpu_set_output             (EffectVCPU *self, int output);
static bool        effect_vcpu_is_halted              (const EffectVCPU *self);
static void        effect_vcpu_set_halted             (EffectVCPU *self, bool halted);
static bool        effect_vcpu_is_active              (const EffectVCPU *self);
void               effect_vcpu_set_active             (EffectVCPU *self, bool active);
static void        effect_vcpu_poweron                (EffectVCPU *self);
static void        effect_vcpu_poweroff               (EffectVCPU *self);
static int         effect_vcpu_evaluate_output_rising (EffectVCPU *self, float input);
static int         effect_vcpu_evaluate_output_high   (EffectVCPU *self);
static int         effect_vcpu_evaluate_output_falling(EffectVCPU *self, float input);
static int         effect_vcpu_evaluate_output_low    (EffectVCPU *self);
static float       effect_vcpu_normalize_time         (int lo, int hi, int v);
static void        effect_vcpu_start_function         (EffectVCPU *self, unsigned tick);
static void        effect_vcpu_start_delay            (EffectVCPU *self, unsigned tick, unsigned value);
bool               effect_vcpu_advance_clock          (EffectVCPU *self, unsigned tick);
static uint8_t     effect_vcpu_fetch                  (EffectVCPU *self);
static bool        effect_vcpu_execute_opcode         (EffectVCPU *self, unsigned tick);
static void        effect_vcpu_execute_block          (EffectVCPU *self, unsigned tick);
static void        effect_vcpu_reset_config           (EffectVCPU *self);
static void        effect_vcpu_reset_function         (EffectVCPU *self);
EffectVCPU        *effect_vcpu_create                 (EffectCtl *control, unsigned led, const uint8_t *bytecode, size_t codesize);
void               effect_vcpu_delete                 (EffectVCPU *self);

/* ------------------------------------------------------------------------- *
 * EFFECT_COMPILER
 * ------------------------------------------------------------------------- */

static void     effect_compiler_reset       (EffectCompiler *self);
EffectCompiler *effect_compiler_create      (void);
void            effect_compiler_delete      (EffectCompiler *self);
static void     effect_compiler_add_byte    (EffectCompiler *self, unsigned led, uint8_t val);
static bool     effect_compiler_in_overflow (const EffectCompiler *self, unsigned led);
uint8_t        *effect_compiler_get_bytecode(EffectCompiler *self, unsigned led, size_t *psize);
size_t          effect_compiler_compile     (EffectCompiler *self, const char *srce);
static bool     effect_compiler_compile_sub (EffectCompiler *self, const char *srce);

/* ========================================================================= *
 * UTILITY
 * ========================================================================= */

static inline const char *bool_repr(bool val)
{
    return val ? "true" : "false";
}

static inline int get_chr(const char **ptxt)
{
    int chr = 0;

    const char *txt = *ptxt;

    if( *txt )
        chr = *txt++;

    *ptxt = txt;

    return chr;
}

static inline int get_num(const char **ptxt)
{
    int num = -1;

    char       *end = 0;
    const char *txt = *ptxt;
    int         val = strtol(txt, &end, 10);

    if( end > txt ) {
        num   = val;
        *ptxt = end;
    }

    return num;
}

static inline int get_ticks(const char **ptxt)
{
    return effect_ms_to_ticks(get_num(ptxt));
}

/* ========================================================================= *
 * EFFECT_VCPU
 * ========================================================================= */

static const char *effect_function_name(EffectFunction value)
{
    static const char * const lut[] = {
        [FUNCTION_DELAY]     = "FUNCTION_DELAY",
        [FUNCTION_FIXED]     = "FUNCTION_FIXED",
        [FUNCTION_RECTANGLE] = "FUNCTION_RECTANGLE",
        [FUNCTION_TRIANGLE]  = "FUNCTION_TRIANGLE",
        [FUNCTION_SINE]      = "FUNCTION_SINE",
        [FUNCTION_FLARE]     = "FUNCTION_FLARE",
    };

    const char *name = value < numof(lut) ? lut[value] : NULL;
    return name ?: "FUNCTION_UNKNOWN";
}

static const char *effect_opcode_name(unsigned value)
{
    switch( value ) {
#define HANDLE(opcode) case opcode: return #opcode
        HANDLE(OPCODE_NOP);
        HANDLE(OPCODE_HALT);
        HANDLE(OPCODE_POWEROFF);
        HANDLE(OPCODE_RESTART);
        HANDLE(OPCODE_EXPIRY);
        HANDLE(OPCODE_DELAY);
        HANDLE(OPCODE_AMPLITUDE);
        HANDLE(OPCODE_RISE_TIME);
        HANDLE(OPCODE_HIGH_TIME);
        HANDLE(OPCODE_FALL_TIME);
        HANDLE(OPCODE_LOW_TIME);
        HANDLE(OPCODE_COUNT);
        HANDLE(OPCODE_FUNCTION);
#undef HANDLE
    }
    return "OPCODE_UNKNOWN";
}

EffectCtl *
effect_vcpu_get_ctl(const EffectVCPU *self)
{
    return self ? self->vcp_effect_ctl : NULL;
}

static const char *
effect_vcpu_identity(const EffectVCPU *self)
{
    const char *identity = self ? self->vcp_identity : NULL;
    return identity ?: "PatternUnknown:LED_UNKNOWN";
}

int
effect_vcpu_get_output(const EffectVCPU *self)
{
    return self ? self->vcp_output : 0;
}

static void
effect_vcpu_set_output(EffectVCPU *self, int output)
{
    if( self && self->vcp_output != output ) {
        effect_vcpu_log(LL_DEBUG, "output: %d -> %d", self->vcp_output, output);
        self->vcp_output = output;
    }
}

static bool
effect_vcpu_is_halted(const EffectVCPU *self)
{
    return self ? self->vcp_halted : true;
}

static void
effect_vcpu_set_halted(EffectVCPU *self, bool halted)
{
    if( self && self->vcp_halted != halted ) {
        effect_vcpu_log(LL_DEBUG, "halted: %s -> %s", bool_repr(self->vcp_halted), bool_repr(halted));
        self->vcp_halted = halted;
    }
}

static bool
effect_vcpu_is_active(const EffectVCPU *self)
{
    return self ? self->vcp_active : false;
}

void
effect_vcpu_set_active(EffectVCPU *self, bool active)
{
    if( self && self->vcp_active != active ) {
        effect_vcpu_log(LL_DEBUG, "active: %s -> %s", bool_repr(self->vcp_active), bool_repr(active));
        if( (self->vcp_active = active) )
            effect_vcpu_poweron(self);
        else
            effect_vcpu_poweroff(self);
    }
}

static void
effect_vcpu_poweron(EffectVCPU *self)
{
    effect_vcpu_reset_config(self);
    effect_vcpu_reset_function(self);

    // rewind program counter and unfreeze
    self->vcp_pc = 0;
    effect_vcpu_set_halted(self, false);
    effect_vcpu_set_output(self, 0);
}

static void
effect_vcpu_poweroff(EffectVCPU *self)
{
    // freeze
    effect_vcpu_set_halted(self, true);
    effect_vcpu_set_output(self, 0);
}

static int
effect_vcpu_evaluate_output_rising(EffectVCPU *self, float input)
{
    // 'input' is fraction of rise time: 0.0f ... 1.0f
    int output = 0;

    switch( self->vcp_function_type ) {
    case FUNCTION_DELAY:
        output = 0;
        break;

    case FUNCTION_FIXED:
        output = self->vcp_setup_amplitude;
        break;

    case FUNCTION_RECTANGLE:
        output = self->vcp_setup_amplitude;
        break;

    case FUNCTION_SINE:
        input  = sinf(input * M_PI_2f);
        output = (int)(input * input * self->vcp_setup_amplitude + 0.5f);
        break;

    case FUNCTION_TRIANGLE:
        output = (int)((0.0f + input) * self->vcp_setup_amplitude + 0.5f);
        break;

    case FUNCTION_FLARE:
        output = random() & 0xff;
        break;
    }

    return output;
}

static int
effect_vcpu_evaluate_output_high(EffectVCPU *self)
{
    int output = 0;

    switch( self->vcp_function_type ) {
    case FUNCTION_DELAY:
        output = 0;
        break;

    case FUNCTION_FIXED:
    case FUNCTION_RECTANGLE:
    case FUNCTION_SINE:
    case FUNCTION_TRIANGLE:
    case FUNCTION_FLARE:
        output = self->vcp_setup_amplitude;
        break;
    }

    return output;
}

static int
effect_vcpu_evaluate_output_falling(EffectVCPU *self, float input)
{
    // 'input' is fraction of fall time: 0.0f ... 1.0f
    int output = 0;

    switch( self->vcp_function_type ) {
    case FUNCTION_DELAY:
        output = 0;
        break;

    case FUNCTION_FIXED:
        output = self->vcp_setup_amplitude;
        break;

    case FUNCTION_RECTANGLE:
        output = 0;
        break;

    case FUNCTION_SINE:
        input  = cosf(input * M_PI_2f);
        output = (int)(input * input * self->vcp_setup_amplitude + 0.5f);
        break;

    case FUNCTION_TRIANGLE:
        output = (int)((1.0f - input) * self->vcp_setup_amplitude + 0.5f);
        break;

    case FUNCTION_FLARE:
        output = random() & 0xff;
        break;
    }

    return output;
}

static int
effect_vcpu_evaluate_output_low(EffectVCPU *self)
{
    int output = 0;

    switch( self->vcp_function_type ) {
    case FUNCTION_DELAY:
        output = 0;
        break;

    case FUNCTION_FIXED:
        output = self->vcp_setup_amplitude;
        break;

    case FUNCTION_RECTANGLE:
    case FUNCTION_SINE:
    case FUNCTION_TRIANGLE:
    case FUNCTION_FLARE:
        output = 0;
        break;
    }

    return output;
}

static float
effect_vcpu_normalize_time(int lo, int hi, int v)
{
    return lo < hi ? (v - lo) / (float)(hi - lo) : 0.0f;
}

static void
effect_vcpu_start_function(EffectVCPU *self, unsigned tick)
{
    self->vcp_function_start     = tick;
    self->vcp_function_rise_ends = (tick += self->vcp_setup_rise_time);
    self->vcp_function_high_ends = (tick += self->vcp_setup_high_time);
    self->vcp_function_fall_ends = (tick += self->vcp_setup_fall_time);
    self->vcp_function_low_ends  = (tick += self->vcp_setup_low_time);

    // Halt on all-zero time values
    if( tick == self->vcp_function_start )
        effect_vcpu_set_halted(self, true);
}

static void
effect_vcpu_start_delay(EffectVCPU *self, unsigned tick, unsigned value)
{
    self->vcp_function_start     =
    self->vcp_function_rise_ends =
    self->vcp_function_high_ends =
    self->vcp_function_fall_ends = tick;
    self->vcp_function_low_ends  = tick + value;
}

bool
effect_vcpu_advance_clock(EffectVCPU *self, unsigned tick)
{
    bool clock_needed = false;

    if( self ) {
        effect_vcpu_log(LL_DEBUG, "clock:%u", tick);

        // Default to zero level output
        int output = 0;

        // Handle possible auto-power-off first
        if( tick >= self->vcp_setup_deactivate )
            effect_vcpu_set_active(self, false);

        if( !effect_vcpu_is_active(self) ) {
            // Power off implies: halted at zero output
            effect_vcpu_set_halted(self, true);
        }
        else if( effect_vcpu_is_halted(self) ) {
            // Halted: retain current level
            output = effect_vcpu_get_output(self);
        }
        else if( tick < self->vcp_function_rise_ends ) {
            // Rise time
            unsigned l = self->vcp_function_start;
            unsigned h = self->vcp_function_rise_ends;
            float    t = effect_vcpu_normalize_time(l, h, tick);

            output = effect_vcpu_evaluate_output_rising(self, t);
        }
        else if( tick <= self->vcp_function_high_ends ) {
            // High time
            output = effect_vcpu_evaluate_output_high(self);
        }
        else if( tick < self->vcp_function_fall_ends ) {
            // Fall time
            unsigned l = self->vcp_function_high_ends;
            unsigned h = self->vcp_function_fall_ends;
            float    t = effect_vcpu_normalize_time(l, h, tick);

            output = effect_vcpu_evaluate_output_falling(self, t);
        }
        else if( tick <= self->vcp_function_low_ends ) {
            // Low time
            output = effect_vcpu_evaluate_output_low(self);
        }
        else if( self->vcp_function_repeats > 0 ) {
            // Repeat configured for function
            self->vcp_function_repeats -= 1;
            effect_vcpu_start_function(self, tick);
        }
        else {
            // Execute until opcode with wait state hit
            effect_vcpu_reset_function(self);
            effect_vcpu_execute_block(self, tick);
            // Do not lose output in case of halt
            if( effect_vcpu_is_halted(self) )
                output = effect_vcpu_get_output(self);
        }

        effect_vcpu_set_output(self, output);

        // Clock is needed while not halted
        if( !effect_vcpu_is_halted(self) )
            clock_needed = true;
    }

    return clock_needed;
}

static uint8_t
effect_vcpu_fetch(EffectVCPU *self)
{
    uint8_t value = OPCODE_HALT;
    if( self && self->vcp_pc < self->vcp_codesize )
        value = self->vcp_bytecode[self->vcp_pc++];
    return value;
}

static bool
effect_vcpu_execute_opcode(EffectVCPU *self, unsigned tick)
{
    effect_vcpu_log(LL_DEBUG, "clock:%u", tick);

    uint8_t opcode = self->vcp_halted ? OPCODE_HALT : effect_vcpu_fetch(self);
    uint8_t value  = (opcode & OPCODE_HAS_VALUE) ? effect_vcpu_fetch(self) : 0;

    if( !(opcode & OPCODE_HAS_VALUE) )
        effect_vcpu_log(LL_DEBUG, "EXEC %s", effect_opcode_name(opcode));
    else if( opcode == OPCODE_FUNCTION )
        effect_vcpu_log(LL_DEBUG, "EXEC %s %s", effect_opcode_name(opcode), effect_function_name(value));
    else
        effect_vcpu_log(LL_DEBUG, "EXEC %s %d", effect_opcode_name(opcode), value);

    switch( opcode ) {
    case OPCODE_NOP:
        break;

    default:
    case OPCODE_HALT:
        effect_vcpu_set_halted(self, true);
        break;

    case OPCODE_POWEROFF:
        effect_vcpu_set_active(self, false);
        break;

    case OPCODE_RESTART:
        effect_vcpu_poweron(self);
        break;

    case OPCODE_EXPIRY:
        self->vcp_setup_deactivate = tick + value;
        break;

    case OPCODE_DELAY:
        self->vcp_function_type    = FUNCTION_DELAY;
        self->vcp_function_repeats = 0;

        effect_vcpu_start_delay(self, tick, value);
        break;

    case OPCODE_AMPLITUDE:
        self->vcp_setup_amplitude = value;
        break;

    case OPCODE_RISE_TIME:
        self->vcp_setup_rise_time = value;
        break;

    case OPCODE_HIGH_TIME:
        self->vcp_setup_high_time = value;
        break;

    case OPCODE_FALL_TIME:
        self->vcp_setup_fall_time = value;
        break;

    case OPCODE_LOW_TIME:
        self->vcp_setup_low_time = value;
        break;

    case OPCODE_COUNT:
        self->vcp_setup_count = value;
        break;

    case OPCODE_FUNCTION:
        self->vcp_function_type    = value;
        self->vcp_function_repeats = self->vcp_setup_count - 1u;

        effect_vcpu_start_function(self, tick);
        effect_vcpu_advance_clock(self, tick);

        if( value == FUNCTION_FIXED )
            effect_vcpu_set_halted(self, true);
        break;
    }

    return opcode & OPCODE_IS_SETUP;
}

static void
effect_vcpu_execute_block(EffectVCPU *self, unsigned tick)
{
    // Opcode sequence that does not lead to wait-state -> halt
    for( int allowance = 16; ; ) {
        if( !effect_vcpu_execute_opcode(self, tick) )
            break;
        if( --allowance <= 0 )
            effect_vcpu_set_halted(self, true);
    }
}

static void
effect_vcpu_reset_config(EffectVCPU *self)
{
    // reset configuration state
    self->vcp_setup_amplitude  = EFFECT_DEFAULT_AMPLITUDE;
    self->vcp_setup_rise_time  = 0;
    self->vcp_setup_high_time  = 0;
    self->vcp_setup_fall_time  = 0;
    self->vcp_setup_low_time   = 0;
    self->vcp_setup_count      = EFFECT_DEFAULT_COUNT;
    self->vcp_setup_deactivate = UINT_MAX;
}

static void
effect_vcpu_reset_function(EffectVCPU *self)
{
    // reset execution state
    self->vcp_function_type      = FUNCTION_DELAY;
    self->vcp_function_start     = 0;
    self->vcp_function_rise_ends = 0;
    self->vcp_function_high_ends = 0;
    self->vcp_function_fall_ends = 0;
    self->vcp_function_low_ends  = 0;
    self->vcp_function_repeats   = 0;
}

EffectVCPU *
effect_vcpu_create(EffectCtl *control, unsigned led, const uint8_t *bytecode, size_t codesize)
{
    EffectVCPU *self = g_malloc0(sizeof *self);

    // uplink
    self->vcp_effect_ctl = control;
    self->vcp_led        = led;

    // bytecode
    self->vcp_bytecode = g_memdup2(bytecode, codesize);
    self->vcp_codesize = codesize;

    // externally visible state
    self->vcp_active = false;
    self->vcp_halted = true;
    self->vcp_output = 0;

    // identity for diagnostic logging
    LedPattern *pattern = effect_ctl_get_pattern(control);

    self->vcp_identity = g_strdup_printf("%s:%s", led_pattern_name(pattern), aw91xxx_led_name(led));

    // the rest is dontcare until poweron

    return self;
}

void
effect_vcpu_delete(EffectVCPU *self)
{
    if( self ) {
        g_free(self->vcp_bytecode), self->vcp_bytecode = NULL, self->vcp_codesize = 0;

        self->vcp_effect_ctl = NULL;

        g_free(self->vcp_identity), self->vcp_identity = NULL;

        g_free(self);
    }
}

/* ------------------------------------------------------------------------- *
 * EFFECT_COMPILER
 * ------------------------------------------------------------------------- */

static void
effect_compiler_reset(EffectCompiler *self)
{
    for( unsigned led = 0; led < AW91XXX_LED_COUNT; ++led )
        self->efc_size[led] = 0;
}

EffectCompiler *
effect_compiler_create(void)
{
    EffectCompiler *self = g_malloc0(sizeof *self);

    effect_compiler_reset(self);

    return self;
}

void
effect_compiler_delete(EffectCompiler *self)
{
    if( self ) {
        g_free(self);
    }
}

static void
effect_compiler_add_byte(EffectCompiler *self, unsigned led, uint8_t val)
{
    if( self && led < AW91XXX_LED_COUNT ) {
        size_t offs = self->efc_size[led];
        if( offs < sizeof *self->efc_code )
            self->efc_code[led][offs] = val;
        self->efc_size[led] = offs + 1;
    }
}

static bool
effect_compiler_in_overflow(const EffectCompiler *self, unsigned led)
{
    bool in_overflow = true;

    if( self && led < AW91XXX_LED_COUNT )
        in_overflow = self->efc_size[led] >= sizeof *self->efc_code;

    return in_overflow;
}

uint8_t *
effect_compiler_get_bytecode(EffectCompiler *self, unsigned led, size_t *psize)
{
    uint8_t *code = NULL;
    size_t   size = 0;

    if( self && led < AW91XXX_LED_COUNT ) {
        if( (size = self->efc_size[led]) > 0 )
            code = self->efc_code[led];
    }

    return *psize = size, code;
}

size_t
effect_compiler_compile(EffectCompiler *self, const char *srce)
{
    size_t count = 0;

    if( self ) {
        effect_compiler_reset(self);
        if( srce && !effect_compiler_compile_sub(self, srce) )
            effect_compiler_reset(self);

        for( unsigned led = 0; led < AW91XXX_LED_COUNT; ++led ) {
            mce_log(LL_DEBUG, "LED(%d) %s: %zu bytes", led, aw91xxx_led_name(led), self->efc_size[led]);
            count += self->efc_size[led] > 0;
        }
    }

    return count;
}

static bool
effect_compiler_compile_sub(EffectCompiler *self, const char *srce)
{
    bool ack = false;
    int  led = -1;

    while( *srce ) {
        const char *pos = srce;

        int cmd = get_chr(&srce);

        if( cmd == ' ' )
            continue;

        if( cmd == COMMAND_LED ) {
            switch( get_chr(&srce) ) {
            case 'r': case '0': led = 0; break;
            case 'o': case '1': led = 1; break;
            case 'y': case '2': led = 2; break;
            case 'g': case '3': led = 3; break;
            case 'b': case '4': led = 4; break;
            default:
                mce_log(LL_ERR, "invalid led at: '%s'", pos);
                goto EXIT;
            }
            mce_log(LL_DEBUG, "LED(%d) %s", led, aw91xxx_led_name(led));
            continue;
        }

        if( led == -1 ) {
            mce_log(LL_ERR, "led not set at: '%s'", pos);
            goto EXIT;
        }

        int opc = OPCODE_HALT;
        int arg = -1;

        switch( cmd ) {
        case COMMAND_LED:
            break;
        case COMMAND_NOP:
            opc = OPCODE_NOP;
            break;
        case COMMAND_HALT:
            opc = OPCODE_HALT;
            break;
        case COMMAND_POWEROFF:
            opc = OPCODE_POWEROFF;
            break;
        case COMMAND_RESTART:
            opc = OPCODE_RESTART;
            break;
        case COMMAND_EXPIRY:
            opc = OPCODE_EXPIRY;
            arg = get_ticks(&srce);
            break;
        case COMMAND_DELAY:
            opc = OPCODE_DELAY;
            arg = get_ticks(&srce);
            if( arg > 0 )
                arg -= 1;
            else
                opc = OPCODE_NOP;
            break;
        case COMMAND_AMPLITUDE:
            opc = OPCODE_AMPLITUDE;
            arg = get_num(&srce);
            break;
        case COMMAND_RISE_TIME:
            opc = OPCODE_RISE_TIME;
            arg = get_ticks(&srce);
            break;
        case COMMAND_HIGH_TIME:
            opc = OPCODE_HIGH_TIME;
            arg = get_ticks(&srce);
            break;
        case COMMAND_FALL_TIME:
            opc = OPCODE_FALL_TIME;
            arg = get_ticks(&srce);
            break;
        case COMMAND_LOW_TIME:
            opc = OPCODE_LOW_TIME;
            arg = get_ticks(&srce);
            break;
        case COMMAND_COUNT:
            opc = OPCODE_COUNT;
            arg = get_num(&srce);
            break;
        case COMMAND_FUNCTION:
            opc = OPCODE_FUNCTION;
            switch( get_chr(&srce) ) {
            case 'd': arg = FUNCTION_DELAY;     break;
            case 'f': arg = FUNCTION_FIXED;     break;
            case 'r': arg = FUNCTION_RECTANGLE; break;
            case 't': arg = FUNCTION_TRIANGLE;  break;
            case 's': arg = FUNCTION_SINE;      break;
            case 'F': arg = FUNCTION_FLARE;     break;
            }
            break;
        default:
            mce_log(LL_ERR, "unkown command at: '%s'", pos);
            goto EXIT;
        }

        if( (opc & OPCODE_HAS_VALUE) && (arg < 0 || arg > UINT8_MAX) ) {
            mce_log(LL_ERR, "invalid command arg at: '%s'", pos);
            goto EXIT;
        }

        if( !(opc & OPCODE_HAS_VALUE) )
            mce_log(LL_DEBUG, "ADD %s", effect_opcode_name(opc));
        else if( opc == OPCODE_FUNCTION )
            mce_log(LL_DEBUG, "ADD %s %s", effect_opcode_name(opc), effect_function_name(arg));
        else
            mce_log(LL_DEBUG, "ADD %s %d", effect_opcode_name(opc), arg);

        effect_compiler_add_byte(self, led, opc);

        if( opc & OPCODE_HAS_VALUE )
            effect_compiler_add_byte(self, led, arg);

        if( effect_compiler_in_overflow(self, led) ) {
            mce_log(LL_ERR, "program too large at: '%s'", pos);
            goto EXIT;
        }

        if( opc == OPCODE_HALT || opc == COMMAND_RESTART )
            led = -1;
    }

    ack = true;

EXIT:
    return ack;
}
