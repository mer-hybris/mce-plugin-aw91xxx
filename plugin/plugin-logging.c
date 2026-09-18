/** @file plugin-logging.c
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2013 - 2017 Jolla Ltd.
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include "plugin-logging.h"

#include "plugin-api.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * MCE_LOGGING_API
 * ------------------------------------------------------------------------- */

void mce_hybris_log(int lev, const char *file, const char *func, const char *fmt, ...) __attribute__ ((format (printf, 4, 5)));

/* ========================================================================= *
 * Data
 * ========================================================================= */

/** Callback function for diagnostic output, or NULL for stderr output */
static mce_hybris_log_fn mce_hybris_log_cb = 0;

/* ========================================================================= *
 * MCE_LOGGING_API
 * ========================================================================= */

/** Set diagnostic output forwarding callback
 *
 * @param cb  The callback function to use, or NULL for stderr output
 */
void
mce_hybris_set_log_hook(mce_hybris_log_fn cb)
{
  mce_hybris_log_cb = cb;
}

/** Wrapper for diagnostic logging
 *
 * @param lev  syslog priority (=mce_log level) i.e. LL_ERR etc
 * @param file source code path
 * @param func name of function within file
 * @param fmt  printf compatible format string
 * @param ...  parameters required by the format string
 */
void
mce_hybris_log(int lev, const char *file, const char *func, const char *fmt, ...)
{
  char *msg = NULL;
  va_list va;

  va_start(va, fmt);
  if( vasprintf(&msg, fmt, va) < 0 )
        msg = NULL;
  va_end(va);

  if( msg ) {
    if( mce_hybris_log_cb )
      mce_hybris_log_cb(lev, file, func, msg);
    else
      fprintf(stderr, "%s: %s: %s\n", file, func, msg);
    free(msg);
  }
}
