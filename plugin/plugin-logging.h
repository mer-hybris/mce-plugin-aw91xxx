/** @file plugin-logging.h
 *
 * mce-plugin-aw91xxx - AW91XXX LED plugin for Mode Control Entity
 *
 * SPDX-FileCopyrightText: (c) 2013 - 2017 Jolla Ltd.
 * SPDX-FileCopyrightText: (c) 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#ifndef  PLUGIN_LOGGING_H_
# define PLUGIN_LOGGING_H_

# include <syslog.h>

/* ========================================================================= *
 * Constants
 * ========================================================================= */

/** MCE logging priorities
 */
enum
{
  LL_CRIT    = LOG_CRIT,          /**< Critical error */
  LL_ERR     = LOG_ERR,           /**< Error */
  LL_WARN    = LOG_WARNING,       /**< Warning */
  LL_NOTICE  = LOG_NOTICE,        /**< Normal but noteworthy */
  LL_INFO    = LOG_INFO,          /**< Informational message */
  LL_DEBUG   = LOG_DEBUG,         /**< Useful when debugging */
};

/* ========================================================================= *
 * Macros
 * ========================================================================= */

/** Logging from hybris plugin mimics mce-log.h API */
# define mce_log(LEV,FMT,ARGS...) \
   mce_hybris_log(LEV, __FILE__, __FUNCTION__ ,FMT, ## ARGS)

/* ========================================================================= *
 * Prototypes
 * ========================================================================= */

/* ------------------------------------------------------------------------- *
 * MCE_LOGGING_API
 * ------------------------------------------------------------------------- */

void mce_hybris_log(int lev, const char *file, const char *func, const char *fmt, ...) __attribute__ ((format (printf, 4, 5)));

#endif /* PLUGIN_LOGGING_H_ */
