/*
 *      Copyright (C) Jordan Erickson                     - 2014-2020,
 *      Copyright (C) Löwenfelsen UG (haftungsbeschränkt) - 2015-2020
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
 * This file defines the API for the tee module. The tee module allows
 * multiple sinks to be connected to a single source.
 */

#ifndef __COOLMIC_DSP_UTIL_H__
#define __COOLMIC_DSP_UTIL_H__

#include <stdint.h>

#define COOLMIC_UTIL_PROFILE_DEFAULT      "default"

typedef uint32_t coolmic_argb_t;

/* This converst Alpha-Hue-Saturation-Value colours to Alpha-Red-Green-Blue values.
 */
coolmic_argb_t coolmic_util_ahsv2argb(double alpha, double hue, double saturation, double value);

/* This converst a power value [dB] to a Hue value based on the given profile.
 */
double         coolmic_util_power2hue(double power, const char *profile);
/* This converst a peak value [-32768:32767] to a Hue value based on the given profile.
 */
double         coolmic_util_peak2hue (int16_t peak, const char *profile);


#endif
