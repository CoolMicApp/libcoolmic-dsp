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
 * This provides segment support for the very simple interface to the encoder.
 */

#ifndef __COOLMIC_DSP_SIMPLE_SEGMENT_H__
#define __COOLMIC_DSP_SIMPLE_SEGMENT_H__

#include <stdint.h>

/* forward declare internally used structures */
typedef struct coolmic_simple_segment coolmic_simple_segment_t;

typedef enum {
    COOLMIC_SIMPLE_SP_LIVE,
    COOLMIC_SIMPLE_SP_FILE_SIMPLE,
} coolmic_simple_segment_pipeline_t;

coolmic_simple_segment_t *  coolmic_simple_segment_new(const char *name, igloo_ro_t associated, coolmic_simple_segment_pipeline_t pipeline, const char *driver, const char *device);
int                         coolmic_simple_segment_get_pipeline(coolmic_simple_segment_t *segment, coolmic_simple_segment_pipeline_t *pipeline);
int                         coolmic_simple_segment_get_driver_and_device(coolmic_simple_segment_t *segment, const char **driver, const char **device);

#endif
