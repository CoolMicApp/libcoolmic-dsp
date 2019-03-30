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
 * This file defines the API for the VU-Meter part of this library.
 *
 * The VU-Meter works by setting it up, attaching a IO handle
 * for the backend and then calling the result function whenever you
 * want to have the current information.
 */

#ifndef __COOLMIC_DSP_VUMETER_H__
#define __COOLMIC_DSP_VUMETER_H__

#include <stdint.h>
#include "iohandle.h"

/* maximum number of channels */
#define COOLMIC_DSP_VUMETER_MAX_CHANNELS 16

/* forward declare internally used structures */
typedef struct coolmic_vumeter coolmic_vumeter_t;

/* Delcare type for result */
typedef struct {
    /* General information about the stream */
    /* Sample rate in [Hz] */
    uint_least32_t rate;
    /* Number of channels */
    unsigned int channels;

    /* Information about the result */
    /* Number of frames taken into account.
     * Time for this block is: time = frames / rate.
     */
    size_t frames;

    /* Signal parameters */
    /* Global peak (peak value of all channels).
     * This is the absolute biggest sample value seen in the block.
     * If this is -32768 or 32767 you reach hard clipping.
     */
    int16_t global_peak;
    /* Global power (power level of all channels).
     * This is the power of all channels summed up.
     * This value is in [dB] and normalized so that 0dB
     * represent maximum value on all channel all the time.
     */
    double global_power;
    /* Per channel peak.
     * This is like the global peak but for each channel.
     */
    int16_t channel_peak[COOLMIC_DSP_VUMETER_MAX_CHANNELS];
    /* Per channel power.
     * This is like the global power but for each channel.
     * The value is normalized so that 0dB represents the
     * maximum sample value on this channel all the time.
     */
    double channel_power[COOLMIC_DSP_VUMETER_MAX_CHANNELS];
} coolmic_vumeter_result_t;

/* Management of the VU-Meter object */
coolmic_vumeter_t  *coolmic_vumeter_new(const char *name, igloo_ro_t associated, uint_least32_t rate, unsigned int channels);

/* Reset the VU-Meter state. This discards all the allready collected data */
int                 coolmic_vumeter_reset(coolmic_vumeter_t *self);

/* This is to attach the IO Handle of the PCM data stream that is to be passed to the analyzer */
int                 coolmic_vumeter_attach_iohandle(coolmic_vumeter_t *self, coolmic_iohandle_t *handle);

/* Read data from the IO Handle.
 * This reads until reaching any error or maxlen bytes.
 * If maxbytes is -1 a unspecified internal default is used.
 * The data is directly processed and the internal state is updated.
 * If you want to read a specific time use maxlen = time * rate * channels * sizeof(int16_t).
 * Returns the number of bytes actually read.
 */
ssize_t             coolmic_vumeter_read(coolmic_vumeter_t *self, ssize_t maxlen);

/* Read the result into the structure pointed to by *result.
 * On successful call the internal state is reset after the result is read
 * such as by calling coolmic_vumeter_reset().
 */
int                 coolmic_vumeter_result(coolmic_vumeter_t *self, coolmic_vumeter_result_t *result);

#endif
