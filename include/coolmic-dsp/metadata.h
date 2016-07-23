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
 * This file defines the API for the metadata module.
 * The metadata API is used to store and manipulate stream metadata.
 */

#ifndef __COOLMIC_DSP_METADATA_H__
#define __COOLMIC_DSP_METADATA_H__

typedef struct coolmic_metadata coolmic_metadata_t;

/* Management of the metadata object */
coolmic_metadata_t      *coolmic_metadata_new(void);
int                      coolmic_metadata_ref(coolmic_metadata_t *self);
int                      coolmic_metadata_unref(coolmic_metadata_t *self);

#endif
