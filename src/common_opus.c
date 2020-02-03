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

/* Please see the corresponding header file for details of this API. */

#define COOLMIC_COMPONENT "libcoolmic-dsp/common-opus"
#include <coolmic-dsp/coolmic-dsp.h>
#include "common_opus.h"

int coolmic_common_opus_libopuserror2error(const int err)
{
    switch (err) {
        case OPUS_OK:
            return COOLMIC_ERROR_NONE;
        break;
        case OPUS_BAD_ARG:
            return COOLMIC_ERROR_INVAL;
        break;
        case OPUS_BUFFER_TOO_SMALL:
            return COOLMIC_ERROR_FAULT;
        break;
        case OPUS_INVALID_PACKET:
            return COOLMIC_ERROR_INVAL;
        break;
        case OPUS_UNIMPLEMENTED:
            return COOLMIC_ERROR_NOSYS;
        break;
        case OPUS_INVALID_STATE:
            return COOLMIC_ERROR_INVAL;
        break;
        case OPUS_ALLOC_FAIL:
            return COOLMIC_ERROR_NOMEM;
        break;

        case OPUS_INTERNAL_ERROR:
        default:
            return COOLMIC_ERROR_GENERIC;
        break;
    }
}
