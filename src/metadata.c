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
#include <coolmic-dsp/metadata.h>
#include <coolmic-dsp/coolmic-dsp.h>


struct coolmic_metadata {
    /* reference counter */
    size_t refc;
};

/* Management of the metadata object */
coolmic_metadata_t      *coolmic_metadata_new(void)
{
    coolmic_metadata_t *ret;

    ret = calloc(1, sizeof(coolmic_metadata_t));
    if (!ret)
        return NULL;

    ret->refc = 1;

    return ret;
}

int                 coolmic_metadata_ref(coolmic_metadata_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    self->refc++;
    return COOLMIC_ERROR_NONE;
}

int                 coolmic_metadata_unref(coolmic_metadata_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    self->refc--;

    if (self->refc)
        return COOLMIC_ERROR_NONE;

    free(self);

    return COOLMIC_ERROR_NONE;
}
