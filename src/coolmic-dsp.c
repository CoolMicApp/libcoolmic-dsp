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

#include <sys/types.h>
#include <string.h>
#include <coolmic-dsp/coolmic-dsp.h>

static const struct {
    const int       error;
    const char *    string;
} __coolmic_errors[] = {
/* grep '^#define COOLMIC_ERROR_' coolmic-dsp.h | sed 's/^#define \([^ ]*\) .*\/\* \(.*\) \*\/$/\1 \2/' | while read code msg; do printf "    {%-32s \"%s\"},\n" "$code", "$msg"; done */
    {COOLMIC_ERROR_NONE,              "No error"},
    {COOLMIC_ERROR_GENERIC,           "Generic, unknown error"},
    {COOLMIC_ERROR_NOSYS,             "Function not implemented"},
    {COOLMIC_ERROR_FAULT,             "Bad address"},
    {COOLMIC_ERROR_INVAL,             "Invalid argument"},
    {COOLMIC_ERROR_NOMEM,             "Not enough space"},
    {COOLMIC_ERROR_BUSY,              "Device or resource busy"},
    {COOLMIC_ERROR_PERM,              "Operation not permitted"},
    {COOLMIC_ERROR_CONNREFUSED,       "Connection refused"},
    {COOLMIC_ERROR_CONNECTED,         "Connected."},
    {COOLMIC_ERROR_UNCONNECTED,       "Unconnected."},
    {COOLMIC_ERROR_NOTLS,             "TLS requested but not supported by peer"},
    {COOLMIC_ERROR_TLSBADCERT,        "TLS connection can not be established because of bad certificate"}
};

const char *coolmic_error2string(const int error) {
    size_t i;

    for (i = 0; i < (sizeof(__coolmic_errors)/sizeof(*__coolmic_errors)); i++) {
        if (__coolmic_errors[i].error == error) {
            return __coolmic_errors[i].string;
        }
    }

    return "(unknown)";
}

const char *coolmic_features(void)
{
    static const char *features = "features"
        " " COOLMIC_FEATURE_ENCODE_OGG_VORBIS
        " " COOLMIC_FEATURE_DRIVER_NULL
#ifdef HAVE_SNDDRV_DRIVER_OSS
        " " COOLMIC_FEATURE_DRIVER_OSS
#endif
#ifdef HAVE_SNDDRV_DRIVER_OPENSL
        " " COOLMIC_FEATURE_DRIVER_OPENSL
#endif
#ifdef HAVE_SNDDRV_DRIVER_STDIO
        " " COOLMIC_FEATURE_DRIVER_STDIO
#endif
    ;
    return features;
}

int coolmic_feature_check(const char *feature)
{
    const char *p;
    size_t len;

    if (!feature)
        return COOLMIC_ERROR_FAULT;
    if (!*feature)
        return COOLMIC_ERROR_INVAL;

    len = strlen(feature);
    p = coolmic_features();

    while (*p) {
        if (strncmp(feature, p, len) == 0) {
            p += len;
            if (*p == 0 || *p == ' ')
                return 1;
        }

        p = strstr(p, " ");
        if (!p)
            return 0;
        p++;
    }

    return 0;
}
