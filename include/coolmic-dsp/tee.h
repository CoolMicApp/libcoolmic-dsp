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
 * This file defines the API for the tee module. The tee module allows
 * multiple sinks to be connected to a single source.
 */

#ifndef __COOLMIC_DSP_TEE_H__
#define __COOLMIC_DSP_TEE_H__

#include "iohandle.h"

typedef struct coolmic_tee coolmic_tee_t;

/* Management of the tee object */
coolmic_tee_t      *coolmic_tee_new(const char *name, igloo_ro_t associated, size_t readers);

/* This is to attach the IO Handle the tee module should read from */
int                 coolmic_tee_attach_iohandle(coolmic_tee_t *self, coolmic_iohandle_t *handle);

/* This function is to get the IO Handles users can read from */
coolmic_iohandle_t *coolmic_tee_get_iohandle(coolmic_tee_t *self, ssize_t index);

#endif
