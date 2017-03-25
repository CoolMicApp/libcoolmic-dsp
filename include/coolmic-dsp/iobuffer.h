/*
 *      Copyright (C) Jordan Erickson                     - 2014-2017,
 *      Copyright (C) Löwenfelsen UG (haftungsbeschränkt) - 2015-2017
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
 * This file defines the API for IO ring buffers.
 */

#ifndef __COOLMIC_DSP_IOBUFFER_H__
#define __COOLMIC_DSP_IOBUFFER_H__

#include "iohandle.h"

/* forward declare internally used structures */
typedef struct coolmic_iobuffer coolmic_iobuffer_t;

/* Management of the encoder object */
coolmic_iobuffer_t   *coolmic_iobuffer_new(size_t size);
int                 coolmic_iobuffer_ref(coolmic_iobuffer_t *self);
int                 coolmic_iobuffer_unref(coolmic_iobuffer_t *self);

/* This is to attach the IO Handle to the ring buffer */
int                 coolmic_iobuffer_attach_iohandle(coolmic_iobuffer_t *self, coolmic_iohandle_t *handle);

/* This function is to get the IO Handle to read data from the ring buffer */
coolmic_iohandle_t *coolmic_iobuffer_get_iohandle(coolmic_iobuffer_t *self);

/* This function is to iterate. It will try to fill the ring buffer.
 * If the input is non-blocking this will also be non-blocking.
 */
int                 coolmic_iobuffer_iter(coolmic_iobuffer_t *self);

#endif
