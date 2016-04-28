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

#include <math.h>
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

    /* buffer for calculation */
    char buffer[2*COOLMIC_DSP_VUMETER_MAX_CHANNELS*32];
    /* how much data we have in the buffer in [Byte] */
    size_t buffer_fill;

    /* Storage for per channel power values */
    int64_t power[COOLMIC_DSP_VUMETER_MAX_CHANNELS];

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

    memset(&(self->power), 0, sizeof(self->power));
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

static ssize_t      coolmic_vumeter_read_phy(coolmic_vumeter_t *self, ssize_t maxlen)
{
    size_t len;
    ssize_t ret;

    len = sizeof(self->buffer) - self->buffer_fill;

    if (maxlen >= 0 && len > (size_t)maxlen)
        len = maxlen;

    ret = coolmic_iohandle_read(self->in, self->buffer + self->buffer_fill, len);

    if (ret == -1 && !self->buffer_fill) {
        return -1;
    } else if (ret == -1) {
        return 0;
    }

    self->buffer_fill += ret;

    return ret;
}

ssize_t             coolmic_vumeter_read(coolmic_vumeter_t *self, ssize_t maxlen)
{
    ssize_t ret;
    size_t framesize;
    size_t frames;
    size_t f, c;
    int16_t *in;

    if (!self)
        return -1;

    ret = coolmic_vumeter_read_phy(self, maxlen);

    in = (int16_t*)(self->buffer);

    framesize = self->channels * 2;
    frames = self->buffer_fill / framesize;

    for (f = 0; f < frames; f++) {
        for (c = 0; c < self->channels; c++) {
            if (abs(*in) > abs(self->result.channel_peak[c])) {
                self->result.channel_peak[c] = *in;
                if (abs(*in) > abs(self->result.global_peak)) {
                    self->result.global_peak = *in;
                }
            }

            self->power[c] += ((int64_t)*in) * ((int64_t)*in);

            /* go to next value */
            in++;
        }
    }

    self->result.frames += frames;

    if ((frames * framesize) < self->buffer_fill) {
        memmove(self->buffer, self->buffer + (frames * framesize), self->buffer_fill - (frames * framesize));
        self->buffer_fill -= frames * framesize;
    } else {
        self->buffer_fill = 0;
    }

    return ret;
}

int                 coolmic_vumeter_result(coolmic_vumeter_t *self, coolmic_vumeter_result_t *result)
{
    unsigned int c;
    int64_t p_all = 0;
    double p;

    if (!self || !result)
        return -1;

    if (!self->result.frames)
        return -1;

    for (c = 0; c < self->channels; c++) {
        p_all += self->power[c];
        p = (double)(self->power[c] / (int64_t)self->result.frames);
        p = 20.*log10(sqrt(p) / 32768.);
        p = fmin(p, 0.);
        self->result.channel_power[c] = p;
    }

    p = (double)(p_all / (uint64_t)(self->result.frames * (size_t)self->channels));
    p = 20.*log10(sqrt(p) / 32768.);
    p = fmin(p, 0.);
    self->result.global_power = p;

    memcpy(result, &(self->result), sizeof(self->result));
    coolmic_vumeter_reset(self);

    return 0;
}
