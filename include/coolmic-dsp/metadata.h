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

#include <vorbis/codec.h>

typedef struct coolmic_metadata coolmic_metadata_t;

/* Management of the metadata object */
coolmic_metadata_t      *coolmic_metadata_new(void);
int                      coolmic_metadata_ref(coolmic_metadata_t *self);
int                      coolmic_metadata_unref(coolmic_metadata_t *self);

/* Altering tags */
/* Those functions cache a lot memory objects.
 * If you completly rewrite and not just do updates you should create a new metadata object each time you start.
 * Otherwise you may end up with this eating up all your memory.
 * This is very important when you use a huge set of key values.
 */
int                      coolmic_metadata_tag_add(coolmic_metadata_t *self, const char *key, const char *value);
int                      coolmic_metadata_tag_set(coolmic_metadata_t *self, const char *key, const char *value);
int                      coolmic_metadata_tag_remove(coolmic_metadata_t *self, const char *key);

/* This adds the metadata in the metadata object to the passed vorbis comment structure.
 * The structure must be vorbis_comment_init()ed. Calling this multiple times on the same
 * instance of vc will add the tags multiple times.
 */
int                      coolmic_metadata_add_to_vorbis_comment(coolmic_metadata_t *self, vorbis_comment *vc);

#endif
