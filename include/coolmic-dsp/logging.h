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

/*
 * This file defines the API for logging of library events.
 */

#ifndef __COOLMIC_DSP_LOGGING_H__
#define __COOLMIC_DSP_LOGGING_H__

typedef enum coolmic_logging_level {
    COOLMIC_LOGGING_LEVEL_FATAL,
    COOLMIC_LOGGING_LEVEL_ERROR,
    COOLMIC_LOGGING_LEVEL_WARNING,
    COOLMIC_LOGGING_LEVEL_INFO,
    COOLMIC_LOGGING_LEVEL_DEBUG
} coolmic_logging_level_t;


const char *coolmic_logging_level2string(coolmic_logging_level_t level);

int coolmic_logging_log_real(const char *file, unsigned long int line, const char *component, coolmic_logging_level_t level, int error, const char *format, ...);
#define coolmic_logging_log(level,error,format,args...) coolmic_logging_log_real(__FILE__, __LINE__, COOLMIC_COMPONENT, (level), (error), (format), ## args)

int coolmic_logging_set_cb_simple(int (*cb)(coolmic_logging_level_t level, const char *msg));

#endif
