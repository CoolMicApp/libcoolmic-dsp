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
#include <string.h>
#include <coolmic-dsp/util.h>

static inline coolmic_argb_t __X_double2int(double X) {
    coolmic_argb_t ret;

    if (X >= 1.) {
        X = 1.;
    } else if (X <= 0.) {
        X = 0.;
    }

    ret = X * 255.;
    if (ret > 255)
        ret = 255;

    return ret;
}

static inline coolmic_argb_t __argb_double2int(double alpha, double red, double green, double blue) {
    coolmic_argb_t ret = 0;

    ret += __X_double2int(alpha)    << 24;
    ret += __X_double2int(red)      << 16;
    ret += __X_double2int(green)    <<  8;
    ret += __X_double2int(blue)     <<  0;

    return ret;
}

/* This converst Alpha-Hue-Saturation-Value colours to Alpha-Red-Green-Blue values.
 */
coolmic_argb_t coolmic_util_ahsv2argb(double alpha, double hue, double saturation, double value) {
    int    hue1     = (int)(double)(hue/(M_PI/3.));
    double f        = hue - (double)hue1;
    double p        = value * (1. - saturation);
    double q        = value * (1. - saturation * f);
    double t        = value * (1. - saturation * (1. - f));
    double red      = 0;
    double green    = 0;
    double blue     = 0;

    switch (hue1) {
        case 0:
        case 6:
            red     = value;
            green   = t;
            blue    = p;
        break;
        case 1:
            red     = q;
            green   = value;
            blue    = p;
        break;
        case 2:
            red     = p;
            green   = value;
            blue    = t;
        break;
        case 3:
            red     = p;
            green   = q;
            blue    = value;
        break;
        case 4:
            red     = t;
            green   = p;
            blue    = value;
        break;
        case 5:
            red     = value;
            green   = p;
            blue    = q;
        break;
    }

    return __argb_double2int(alpha, red, green, blue);
}

/* This converst a power value [dB] to a Hue value based on the given profile.
 */
double         coolmic_util_power2hue(double power, const char *profile) {
    if (!strcmp(profile, COOLMIC_UTIL_PROFILE_DEFAULT)) {
        if (power < -20.) {
            return M_PI*2./3.;
        } else if (power >= 0) {
            return 0;
        } else {
            return pow(sin(M_PI*power/40.), 2.)*M_PI*2./3.;
        }
    } else {
        return 0.; /* red */
    }
}

/* This converst a peak value [-32768:32767] to a Hue value based on the given profile.
 */
double         coolmic_util_peak2hue (int16_t peak, const char *profile) {
    if (!strcmp(profile, COOLMIC_UTIL_PROFILE_DEFAULT)) {
        if (peak == -32768 || peak == 32767) {
            return 0.; /* red */
        } else if (peak < -30000 || peak > 30000) {
            return 0.43; /* redish orange */
        } else if (peak < -28000 || peak > 28000) {
            return 1.; /* a yellow */
        } else {
            return M_PI*2./3.; /* green */
        }
    } else {
        return 0.; /* red */
    }
}
