/** @file plugin-config.c
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2017 Jolla Ltd.
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include "plugin-config.h"

#include "plugin-logging.h"

#include <stdbool.h>
#include <dlfcn.h>

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * UTILITY
 * ------------------------------------------------------------------------- */

static void *lookup_function(const char *name);

/* ------------------------------------------------------------------------- *
 * MCE_CONFIG_API
 * ------------------------------------------------------------------------- */

gchar **mce_conf_get_string_list(const gchar *group, const gchar *key, gsize *length);
gchar **mce_conf_get_keys       (const gchar *group, gsize *length);
gint    mce_conf_get_int        (const gchar *group, const gchar *key, const gint defaultval);

/* ========================================================================= *
 * UTILITY
 * ========================================================================= */

static void *
lookup_function(const char *name)
{
    void *addr = dlsym(RTLD_DEFAULT, name);

    if( !addr )
        mce_log(LL_ERR, "mce does not export: %s(): %s", name, dlerror());

    return addr;
}

#define RESOLVE_FROM_MCE\
     do {\
         static bool done = false;\
         if( !done ) {\
             done = true;\
             real = lookup_function(__FUNCTION__);\
         }\
     } while( false );

/* ========================================================================= *
 * MCE_CONFIG_API
 * ========================================================================= */

gchar **
mce_conf_get_string_list(const gchar *group, const gchar *key, gsize *length)
{
    static gchar **(*real)(const gchar *, const gchar *, gsize *) = NULL;

    RESOLVE_FROM_MCE;

    if( length )
        *length = 0;

    return real ? real(group, key, length) : NULL;
}

gchar **
mce_conf_get_keys(const gchar *group, gsize *length)
{
    static gchar **(*real)(const gchar *, gsize *) = NULL;

    RESOLVE_FROM_MCE;

    if( length )
        *length = 0;

    return real ? real(group, length) : NULL;
}

gint
mce_conf_get_int(const gchar *group, const gchar *key, const gint defaultval)
{
    static gint (*real)(const gchar *, const gchar *, const gint) = NULL;

    RESOLVE_FROM_MCE;

    return real ? real(group, key, defaultval) : defaultval;
}
