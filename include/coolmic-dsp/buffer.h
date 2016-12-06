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
 * This file defines the basic memory buffer API as used by libcoolmic-dsp.
 * A buffer is generic object to store a portion of data with reference handling.
 */

#ifndef __COOLMIC_DSP_BUFFER_H__
#define __COOLMIC_DSP_BUFFER_H__

#include <unistd.h>

/* forward declare internally used structures */
typedef struct coolmic_buffer coolmic_buffer_t;

/* Management of the IO Handle object */
/* The constructor takes the length of the buffer length.
 * If take is NULL it allocates a new buffer. If take is not NULL take is used as buffer.
 * Aftr a buffer has been allocated if copy is not NULL copy is copied into the buffer.
 * When the object is destroied the buffer is freed with xfree if take was not NULL.
 * If xfree is NULL the operating system's free() is used.
 * userdata is passed to free.
 */
coolmic_buffer_t   *coolmic_buffer_new(size_t length, const void *copy, void *take, void (*xfree)(void *content, void *userdata), void *userdata);
/* The simple constructor takes the length for the buffer to be allocated.
 * If content is not NULL the addess of the buffer is written to *content.
 */
coolmic_buffer_t   *coolmic_buffer_new_simple(size_t length, void **content);

/* increase and decrease reference counter */
int                 coolmic_buffer_ref(coolmic_buffer_t *self);
int                 coolmic_buffer_unref(coolmic_buffer_t *self);

/* get content and size */
void               *coolmic_buffer_get_content(coolmic_buffer_t *self);
ssize_t             coolmic_buffer_get_length(coolmic_buffer_t *self);

/* sets a static offset to the data (shrinks the buffer) */
int                 coolmic_buffer_set_offset(coolmic_buffer_t *self, size_t offset);

/* get and set userdata */
void               *coolmic_buffer_get_userdata(coolmic_buffer_t *self);
int                 coolmic_buffer_set_userdata(coolmic_buffer_t *self, void *userdata);

/* get and set next buffer */
/* If a next buffer is set the object will hold a reference on the next buffer.
 * When the object's reference counter drops to zero the next buffer will be dereferenced.
 * This also happens when coolmic_buffer_set_next() is used to set next to NULL.
 */
coolmic_buffer_t   *coolmic_buffer_get_next(coolmic_buffer_t *self);
int                 coolmic_buffer_set_next(coolmic_buffer_t *self, coolmic_buffer_t *next);

/* adds a buffer at the end of the chain */
int                 coolmic_buffer_add_next(coolmic_buffer_t **self, coolmic_buffer_t *next);

#endif
