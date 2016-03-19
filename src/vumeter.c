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
#include <coolmic-dsp/vumeter.h>

struct coolmic_vumeter {
    /* reference counter */
    size_t refc;

    /* input IO handle */
    coolmic_iohandle_t *in;

    /* sample rate in [Hz] */
    uint_least32_t rate;
    /* number of channels */
    unsigned int channels;

    /* result */
    coolmic_vumeter_result_t result;
};

coolmic_vumeter_t  *coolmic_vumeter_new(uint_least32_t rate, unsigned int channels)
{
    coolmic_vumeter_t *ret;

    if (!rate || !channels)
        return NULL;

    ret = calloc(1, sizeof(coolmic_vumeter_t));
    if (!ret)
        return NULL;

    ret->refc = 1;
    ret->rate = rate;
    ret->channels = channels;

    coolmic_vumeter_reset(ret);

    return ret;
}

int                 coolmic_vumeter_ref(coolmic_vumeter_t *self)
{
    if (!self)
        return -1;
    self->refc++;
    return 0;
}

int                 coolmic_vumeter_unref(coolmic_vumeter_t *self)
{
    if (!self)
        return -1;
    self->refc--;
    if (self->refc != 0)
        return 0;

    coolmic_iohandle_unref(self->in);
    free(self);

    return 0;
}

int                 coolmic_vumeter_reset(coolmic_vumeter_t *self)
{
    if (!self)
        return -1;

    memset(&(self->result), 0, sizeof(self->result));
    self->result.rate = self->rate;
    self->result.channels = self->channels;

    return 0;
}

int                 coolmic_vumeter_attach_iohandle(coolmic_vumeter_t *self, coolmic_iohandle_t *handle)
{
    if (!self)
        return -1;
    if (self->in)
        coolmic_iohandle_unref(self->in);
    /* ignore errors here as handle is allowed to be NULL */
    coolmic_iohandle_ref(self->in = handle);
    return 0;
}

ssize_t             coolmic_vumeter_read(coolmic_vumeter_t *self, ssize_t maxlen)
{
    if (!self)
        return -1;
    return -1; /* TODO: implement this */
}

int                 coolmic_vumeter_result(coolmic_vumeter_t *self, coolmic_vumeter_result_t *result)
{
    if (!self || !result)
        return -1;

    memcpy(result, &(self->result), sizeof(self->result));
    coolmic_vumeter_reset(self);

    return 0;
}
