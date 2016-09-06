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
typedef struct coolmic_metadata_tag coolmic_metadata_tag_t;

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


/* This is the iterate API. It allows you to read the content of the metadata object.
 * Every time you want to iterate you need to start with coolmic_metadata_iter_start().
 * When you're done you need to call coolmic_metadata_iter_end().
 * Between your calls to coolmic_metadata_iter_start() and coolmic_metadata_iter_end()
 * you must not call any functions on the same object but coolmic_metadata_iter_*().
 */

/* Start iteration mode. */
int                      coolmic_metadata_iter_start(coolmic_metadata_t *self);
/* End iteration mode. */
int                      coolmic_metadata_iter_end(coolmic_metadata_t *self);
/* Get next tag from the object.
 * Will return NULL at end of list.
 */
coolmic_metadata_tag_t  *coolmic_metadata_iter_next_tag(coolmic_metadata_t *self);
/* Returns the key or name from the tag. */
const char              *coolmic_metadata_iter_tag_key(coolmic_metadata_tag_t *tag);
/* Returns the next value for the current tag.
 * Will return NULL at end of list.
 * Note: It's possible that this will return NULL at first call if there are no values for the given tag.
 */
const char              *coolmic_metadata_iter_tag_next_value(coolmic_metadata_tag_t *tag);

#endif
