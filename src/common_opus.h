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
 * This file defines the private common for Opus related parts.
 */

#ifndef __COOLMIC_DSP_COMMON_OPUS_H__
#define __COOLMIC_DSP_COMMON_OPUS_H__

#ifdef HAVE_ENC_OPUS
#ifdef HAVE_ENC_OPUS_BROKEN_INCLUDE_PATH
#include <opus/include/opus.h>
#else
#include <opus/opus.h>
#endif
#endif

#define COMMON_OPUS_RATE            48000U
#define COMMON_OPUS_MAGIC_HEAD      "OpusHead"
#define COMMON_OPUS_MAGIC_HEAD_LEN  8
#define COMMON_OPUS_MAGIC_TAGS      "OpusTags"
#define COMMON_OPUS_MAGIC_TAGS_LEN  8

#define COMMON_OPUS_MAGIC_LEN       8 /* MAX() of all COMMON_OPUS_MAGIC_*_LEN */

int coolmic_common_opus_libopuserror2error(const int err);

#endif
