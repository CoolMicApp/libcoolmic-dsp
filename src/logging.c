/*
 *      Copyright (C) Jordan Erickson                     - 2014-2016,
 *      Copyright (C) Löwenfelsen UG (haftungsbeschränkt) - 2015-2016
 *       on behalf of Jordan Erickson.
 */

/*
 * This file is part of Cool Mic.
 * 
 * Cool Mic is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Cool Mic is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Cool Mic.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Please see the corresponding header file for details of this API. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <coolmic-dsp/coolmic-dsp.h>
#include <coolmic-dsp/logging.h>

static pthread_mutex_t __logging_lock = PTHREAD_MUTEX_INITIALIZER;
static int (*__logging_cb_simple)(coolmic_logging_level_t level, const char *msg) = NULL;

const char *coolmic_logging_level2string(coolmic_logging_level_t level)
{
    switch (level) {
        case COOLMIC_LOGGING_LEVEL_FATAL:
            return "FATAL";
        break;
        case COOLMIC_LOGGING_LEVEL_ERROR:
            return "ERROR";
        break;
        case COOLMIC_LOGGING_LEVEL_WARNING:
            return "WARNING";
        break;
        case COOLMIC_LOGGING_LEVEL_INFO:
            return "INFO";
        break;
        case COOLMIC_LOGGING_LEVEL_DEBUG:
            return "DEBUG";
        break;
    }
    return "(unknown)";
}

int coolmic_logging_log_real(const char *file, unsigned long int line, const char *component, coolmic_logging_level_t level, int error, const char *format, ...)
{
    va_list ap;
    char *usermsg;
    char *msg;
    int ret;

    if (!format)
        return COOLMIC_ERROR_FAULT;

    if (!__logging_cb_simple)
        return COOLMIC_ERROR_NONE;

    va_start(ap, format);
    ret = vasprintf(&usermsg, format, ap);
    va_end(ap);

    if (ret < 0)
        return COOLMIC_ERROR_NOMEM;

    if (__logging_cb_simple) {
        if (error == COOLMIC_ERROR_NONE) {
            ret = asprintf(&msg, "%s in %s:%lu: %s: %s", component, file, line, coolmic_logging_level2string(level), usermsg);
        } else {
            ret = asprintf(&msg, "%s in %s:%lu: %s: %s: %s", component, file, line, coolmic_logging_level2string(level), usermsg, coolmic_error2string(error));
        }
        if (ret < 0)
            return COOLMIC_ERROR_NOMEM;
        __logging_cb_simple(level, msg);
        free(msg);
    }

    free(usermsg);

    return COOLMIC_ERROR_NONE;
}

int coolmic_logging_set_cb_simple(int (*cb)(coolmic_logging_level_t level, const char *msg))
{
    pthread_mutex_lock(&__logging_lock);
    __logging_cb_simple = cb;
    pthread_mutex_unlock(&__logging_lock);
    return COOLMIC_ERROR_NONE;
}
