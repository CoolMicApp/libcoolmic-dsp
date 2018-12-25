/*
 *      Copyright (C) Jordan Erickson                     - 2014-2018,
 *      Copyright (C) Löwenfelsen UG (haftungsbeschränkt) - 2015-2018
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

/* This is the implementation of simple signal transformations.
 */

#define COOLMIC_COMPONENT "libcoolmic-dsp/transform"
#include <stdlib.h>
#include <string.h>
#include <coolmic-dsp/transform.h>
#include <coolmic-dsp/coolmic-dsp.h>
#include <coolmic-dsp/logging.h>

/* forward declare internally used structures */
struct coolmic_transform {
    /* reference counter */
    size_t refc;
    /* IO Handle */
    coolmic_iohandle_t *io;
    /* iobuffer */
    char iobuffer[2*COOLMIC_DSP_TRANSFORM_MAX_CHANNELS-1];
    size_t iobuffer_fill;
    /* signal sample rate */
    uint_least32_t rate;
    /* signal number of channels */
    unsigned int channels;
    /* Master gain */
    uint16_t master_gain_scale;
    uint16_t master_gain_gain[COOLMIC_DSP_TRANSFORM_MAX_CHANNELS];
};

/* Management of the encoder object */
coolmic_transform_t   *coolmic_transform_new(uint_least32_t rate, unsigned int channels)
{
    coolmic_transform_t *self;

    if (!rate || !channels)
        return NULL;

    self = calloc(1, sizeof(*self));

    if (!self)
        return NULL;

    self->refc      = 1;
    self->rate      = rate;
    self->channels  = channels;

    return self;
}

int                 coolmic_transform_ref(coolmic_transform_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    self->refc++;
    return COOLMIC_ERROR_NONE;
}

int                 coolmic_transform_unref(coolmic_transform_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;

    self->refc--;

    if (self->refc) {
        return COOLMIC_ERROR_NONE;
    }

    coolmic_iohandle_unref(self->io);
    free(self);

    return COOLMIC_ERROR_NONE;
}

int                 coolmic_transform_attach_iohandle(coolmic_transform_t *self, coolmic_iohandle_t *handle)
{   
    if (!self) 
        return COOLMIC_ERROR_FAULT;
    if (self->io)
        coolmic_iohandle_unref(self->io);
    /* ignore errors here as handle is allowed to be NULL */
    coolmic_iohandle_ref(self->io = handle);
    return COOLMIC_ERROR_NONE;
}

static int __free(void *userdata)
{
    coolmic_transform_t *self = userdata;

    return coolmic_transform_unref(self);
}

static void __process(coolmic_transform_t *self, int16_t *samples, size_t frames)
{
    size_t frame;
    size_t channel;
    int64_t tmp;

    if (!self->master_gain_scale)
        return;

    for (frame = 0; frame < frames; frame++) {
        for (channel = 0; channel < self->channels; channel++) {
            tmp = *samples;
            tmp *= self->master_gain_gain[channel];
            tmp /= self->master_gain_scale;
            if (tmp >= 32767) {
                tmp = 32767;
            } else if (tmp <= -32768) {
                tmp = -32768;
            }
            *samples = tmp;
            samples++;
        }
    }
}

static ssize_t __read(void *userdata, void *buffer, size_t len)
{
    coolmic_transform_t *self = userdata;
    const size_t framesize = 2 * self->channels;
    size_t done, tmp;
    ssize_t ret;

    tmp = len % framesize;
    len -= tmp;

    if (!len)
        return 0;

    done = 0;

    if (self->iobuffer_fill) {
        /* As the iobuffer only holds unaligned data and the target buffer has at least space for one slot of aligned data
         * there is no need to check it's size at this point.
         */
        memcpy(buffer, self->iobuffer, self->iobuffer_fill);
        done = self->iobuffer_fill;
        self->iobuffer_fill = 0;
    }

    ret = coolmic_iohandle_read(self->io, buffer + done, len - done);
    if (ret > 0) {
        done += ret;
    }

    tmp = done % framesize;
    if (tmp) {
        memcpy(self->iobuffer, buffer + done - tmp, tmp);
        self->iobuffer_fill = tmp;
        done -= tmp;
    }

    __process(self, buffer, done/framesize);

    return done;
}

static int __eof(void *userdata)
{
    coolmic_transform_t *self = userdata;

    /* we do not need to check iobuffer here because it only holds unaligned data and requires more backend reads anyway. */

    /* There is no data in the buffer. If we do not have an IO handle this is EOF. */
    if (!self->io)
        return 1;

    /* We have no data in the buffer but an IO handle. Just forward the question to the next layer. */
    return coolmic_iohandle_eof(self->io);
}

coolmic_iohandle_t *coolmic_transform_get_iohandle(coolmic_transform_t *self)
{
    coolmic_iohandle_t *ret;

    if (coolmic_transform_ref(self) != COOLMIC_ERROR_NONE)
        return NULL;

    ret = coolmic_iohandle_new(self, __free, __read, __eof);
    if (!ret)
        coolmic_transform_unref(self);

    return ret;
}

int                    coolmic_transform_set_master_gain(coolmic_transform_t *self, unsigned int channels, uint16_t scale, const uint16_t *gain)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;

    if (!channels || !scale || !gain) {
        self->master_gain_scale = 0;
        return COOLMIC_ERROR_NONE;
    }

    if (channels == self->channels) {
        self->master_gain_scale = scale;
        memcpy(self->master_gain_gain, gain, sizeof(*gain)*channels);
        return COOLMIC_ERROR_NONE;
    } else if (channels == 1) {
        self->master_gain_scale = scale;
        for (channels = 0; channels < self->channels; channels++)
            self->master_gain_gain[channels] = *gain;
        return COOLMIC_ERROR_NONE;
    } else if (channels == 2 && self->channels == 1) {
        self->master_gain_scale = scale;
        self->master_gain_gain[0] = ((uint32_t)gain[0] + (uint32_t)gain[1]) / (uint32_t)2;
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "gain: scale=%u, gain[0]=%u (in: %u, %u)", (unsigned int)self->master_gain_scale, (unsigned int)self->master_gain_gain[0], (unsigned int)gain[0], (unsigned int)gain[1]);
        return COOLMIC_ERROR_NONE;
    } else {
        return COOLMIC_ERROR_INVAL;
    }
}
