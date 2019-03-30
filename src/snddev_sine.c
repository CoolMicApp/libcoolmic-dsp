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

/* This is a dummy sound driver. It supports record and playback.
 * In record mode it will read as zeros (silence).
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "types_private.h"
#include <coolmic-dsp/snddev.h>
#include <coolmic-dsp/coolmic-dsp.h>

/* Sine fullwave lookup table for 1kHz at f_s=8000Hz(8kHz) */
static const int16_t table_sine_8[] = {
         0,  23169,  32766,  23169,      0, -23169, -32766, -23169
};
/* Sine fullwave lookup table for 1kHz at f_s=16000Hz(16kHz) */
static const int16_t table_sine_16[] = {
         0,  12539,  23169,  30272,  32766,  30272,  23169,  12539,      0, -12539, 
    -23169, -30272, -32766, -30272, -23169, -12539
};
/* Sine fullwave lookup table for 1kHz at f_s=24000Hz(24kHz) */
static const int16_t table_sine_24[] = {
         0,   8480,  16383,  23169,  28377,  31650,  32766,  31650,  28377,  23169, 
     16383,   8480,      0,  -8480, -16383, -23169, -28377, -31650, -32766, -31650, 
    -28377, -23169, -16383,  -8480
};
/* Sine fullwave lookup table for 1kHz at f_s=32000Hz(32kHz) */
static const int16_t table_sine_32[] = {
         0,   6392,  12539,  18204,  23169,  27244,  30272,  32137,  32766,  32137, 
     30272,  27244,  23169,  18204,  12539,   6392,      0,  -6392, -12539, -18204, 
    -23169, -27244, -30272, -32137, -32766, -32137, -30272, -27244, -23169, -18204, 
    -12539,  -6392
};
/* Sine fullwave lookup table for 1kHz at f_s=44100Hz(44kHz) */
static const int16_t table_sine_44[] = {
         0,   4663,   9231,  13611,  17715,  21457,  24763,  27565,  29805,  31439, 
     32433,  32766,  32433,  31439,  29805,  27565,  24763,  21457,  17715,  13611, 
      9231,   4663,      0,  -4663,  -9231, -13611, -17715, -21457, -24763, -27565, 
    -29805, -31439, -32433, -32766, -32433, -31439, -29805, -27565, -24763, -21457, 
    -17715, -13611,  -9231,  -4663
};
/* Sine fullwave lookup table for 1kHz at f_s=48000Hz(48kHz) */
static const int16_t table_sine_48[] = {
         0,   4276,   8480,  12539,  16383,  19947,  23169,  25995,  28377,  30272, 
     31650,  32486,  32766,  32486,  31650,  30272,  28377,  25995,  23169,  19947, 
     16383,  12539,   8480,   4276,      0,  -4276,  -8480, -12539, -16383, -19947, 
    -23169, -25995, -28377, -30272, -31650, -32486, -32766, -32486, -31650, -30272, 
    -28377, -25995, -23169, -19947, -16383, -12539,  -8480,  -4276
};
/* Sine fullwave lookup table for 1kHz at f_s=96000Hz(96kHz) */
static const int16_t table_sine_96[] = {
         0,   2143,   4276,   6392,   8480,  10532,  12539,  14492,  16383,  18204, 
     19947,  21604,  23169,  24635,  25995,  27244,  28377,  29387,  30272,  31028, 
     31650,  32137,  32486,  32696,  32766,  32696,  32486,  32137,  31650,  31028, 
     30272,  29387,  28377,  27244,  25995,  24635,  23169,  21604,  19947,  18204, 
     16383,  14492,  12539,  10532,   8480,   6392,   4276,   2143,      0,  -2143, 
     -4276,  -6392,  -8480, -10532, -12539, -14492, -16383, -18204, -19947, -21604, 
    -23169, -24635, -25995, -27244, -28377, -29387, -30272, -31028, -31650, -32137, 
    -32486, -32696, -32766, -32696, -32486, -32137, -31650, -31028, -30272, -29387, 
    -28377, -27244, -25995, -24635, -23169, -21604, -19947, -18204, -16383, -14492, 
    -12539, -10532,  -8480,  -6392,  -4276,  -2143
};


/* Sine table lookup table */
static const struct { uint32_t freq; const int16_t *table; } table_sine[] = {
    {  8000, table_sine_8},
    { 16000, table_sine_16},
    { 24000, table_sine_24},
    { 32000, table_sine_32},
    { 44000, table_sine_44},
    { 44100, table_sine_44},
    { 48000, table_sine_48},
    { 96000, table_sine_96},
    {0, NULL}
};

typedef struct snddev_sine {
    size_t len;
    size_t pos;
    const void *table;
} snddev_sine_t;

static const int16_t *find_table(uint_least32_t freq)
{
    size_t i;
    for (i = 0; table_sine[i].freq != 0; i++) {
        if (table_sine[i].freq == freq) {
            return table_sine[i].table;
        }
    }
    return NULL;
}

static ssize_t __read(coolmic_snddev_driver_t *dev, void *buffer, size_t len)
{
    snddev_sine_t *self = dev->userdata_vp;
    size_t todo = len;
    size_t do_next;

    if (self->pos) {
        if ((self->len - self->pos) > todo) {
            do_next = todo;
        } else {
            do_next = self->len - self->pos;
        }
        memcpy(buffer, self->table+self->pos, do_next);
        self->pos += do_next;
        buffer    += do_next;
        todo      -= do_next;
        if (self->pos == self->len)
            self->pos = 0;
    }

    while (todo > self->len) {
        memcpy(buffer, self->table, self->len);
        buffer += self->len;
        todo   -= self->len;
    }

    if (todo) {
        memcpy(buffer, self->table, todo);
        self->pos = todo;
    }

    return len;
}

static ssize_t __write(coolmic_snddev_driver_t *dev, const void *buffer, size_t len)
{
    /* write works as null driver works */
    (void)dev, (void)buffer;
    return len;
}

static int __free(coolmic_snddev_driver_t *dev)
{
    free(dev->userdata_vp);
    memset(dev, 0, sizeof(*dev));
    return COOLMIC_ERROR_NONE;
}

int coolmic_snddev_driver_sine_open(coolmic_snddev_driver_t *dev, const char *driver, void *device, uint_least32_t rate, unsigned int channels, int flags, ssize_t buffer)
{
    (void)driver, (void)device, (void)rate, (void)channels, (void)flags, (void)buffer;
    snddev_sine_t *self;
    const int16_t *table;

    if (channels != 1)
        return COOLMIC_ERROR_NOSYS;

    table = find_table(rate);
    if (!table)
        return COOLMIC_ERROR_NOSYS;

    dev->userdata_vp = self = malloc(sizeof(snddev_sine_t));
    if (!dev->userdata_vp)
        return COOLMIC_ERROR_NOMEM;

    self->table = table;
    self->len   = rate/1000;
    self->len  *= 2;
    self->pos = 0;

    dev->read = __read;
    dev->write = __write;
    dev->free = __free;

    return COOLMIC_ERROR_NONE;
}
