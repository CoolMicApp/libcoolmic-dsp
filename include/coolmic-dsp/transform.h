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

/*
 * This file defines the API for simple signal transformation.
 */

#ifndef __COOLMIC_DSP_TRANSFORM_H__
#define __COOLMIC_DSP_TRANSFORM_H__

#include <stdint.h>
#include "iohandle.h"

#define COOLMIC_DSP_TRANSFORM_MAX_CHANNELS  16

/* forward declare internally used structures */
typedef struct coolmic_transform coolmic_transform_t;

/* Management of the encoder object */
coolmic_transform_t   *coolmic_transform_new(const char *name, igloo_ro_t associated, uint_least32_t rate, unsigned int channels);

/* This is to attach the IO Handle to the ring buffer */
int                    coolmic_transform_attach_iohandle(coolmic_transform_t *self, coolmic_iohandle_t *handle);

/* This function is to get the IO Handle to read data from the ring buffer */
coolmic_iohandle_t    *coolmic_transform_get_iohandle(coolmic_transform_t *self);

/* This sets the master gain.
 * The setting is for the given amount of channels. If the amount of channels differs from the signal a best match is tried.
 * All values are relative to the given scale. So the gain is channel_gain/scale.
 */
int                    coolmic_transform_set_master_gain(coolmic_transform_t *self, unsigned int channels, uint16_t scale, const uint16_t *gain);

#endif
