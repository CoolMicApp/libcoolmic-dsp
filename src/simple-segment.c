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

#include <stdlib.h>
#include <string.h>
#include "types_private.h"
#include <coolmic-dsp/simple-segment.h>
#include <coolmic-dsp/coolmic-dsp.h>

struct coolmic_simple_segment {
    /* base type */
    igloo_ro_base_t __base;

    coolmic_simple_segment_pipeline_t pipeline;
    char *driver;
    char *device;
};

static void __free(igloo_ro_t self)
{
    coolmic_simple_segment_t *segment = igloo_RO_TO_TYPE(self, coolmic_simple_segment_t);

    free(segment->driver);
    free(segment->device);
}

igloo_RO_PUBLIC_TYPE(coolmic_simple_segment_t,
        igloo_RO_TYPEDECL_FREE(__free)
        );

coolmic_simple_segment_t *  coolmic_simple_segment_new(const char *name, igloo_ro_t associated, coolmic_simple_segment_pipeline_t pipeline, const char *driver, const char *device)
{
    coolmic_simple_segment_t *ret;
    char * n_driver;
    char * n_device;

    if (driver) {
        n_driver = strdup(driver);
        if (!n_driver)
            return NULL;
    } else {
        n_driver = NULL;
    }

    if (device) {
        n_device = strdup(device);
        if (!n_device)
            return NULL;
    } else {
        n_device = NULL;
    }

    ret = igloo_ro_new_raw(coolmic_simple_segment_t, name, associated);
    if (!ret)
        return NULL;

    ret->pipeline = pipeline;
    ret->driver = n_driver;
    ret->device = n_device;

    return ret;
}

int                         coolmic_simple_segment_get_pipeline(coolmic_simple_segment_t *segment, coolmic_simple_segment_pipeline_t *pipeline)
{
    if (!segment)
        return COOLMIC_ERROR_FAULT;

    if (pipeline)
        *pipeline = segment->pipeline;

    return 0;
}

int                         coolmic_simple_segment_get_driver_and_device(coolmic_simple_segment_t *segment, const char **driver, const char **device)
{
    if (!segment)
        return COOLMIC_ERROR_FAULT;

    if (driver)
        *driver = segment->driver;

    if (device)
        *device = segment->device;

    return 0;
}
