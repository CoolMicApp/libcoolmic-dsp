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
 * This file defines the types for usage with libigloo.
 */

#ifndef __COOLMIC_DSP_TYPES_H__
#define __COOLMIC_DSP_TYPES_H__

#include <igloo/typedef.h>

igloo_RO_FORWARD_TYPE(coolmic_iohandle_t);
igloo_RO_FORWARD_TYPE(coolmic_shout_t);
igloo_RO_FORWARD_TYPE(coolmic_simple_t);
igloo_RO_FORWARD_TYPE(coolmic_tee_t);
igloo_RO_FORWARD_TYPE(coolmic_vumeter_t);
igloo_RO_FORWARD_TYPE(coolmic_enc_t);
igloo_RO_FORWARD_TYPE(coolmic_metadata_t);
igloo_RO_FORWARD_TYPE(coolmic_simple_segment_t);

#define COOLMIC_DSP_TYPES \
    igloo_RO_TYPE(coolmic_iohandle_t) \
    igloo_RO_TYPE(coolmic_shout_t) \
    igloo_RO_TYPE(coolmic_simple_t) \
    igloo_RO_TYPE(coolmic_tee_t) \
    igloo_RO_TYPE(coolmic_vumeter_t) \
    igloo_RO_TYPE(coolmic_enc_t) \
    igloo_RO_TYPE(coolmic_metadata_t) \
    igloo_RO_TYPE(coolmic_simple_segment_t)

#endif
