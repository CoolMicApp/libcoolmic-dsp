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

/* This is a generic buffer management. See header for details. */

#include <stdlib.h>
#include <string.h>
#include <coolmic-dsp/buffer.h>
#include <coolmic-dsp/coolmic-dsp.h>

struct coolmic_buffer {
    /* reference counter */
    size_t refc;

    /* buffer length */
    size_t length;

    /* buffer offset */
    size_t offset;

    /* buffer content */
    void *content;

    /* free handler */
    void (*free)(void *buffer, void *userdata);

    /* userdata */
    void *userdata;

    /* next buffer pointer */
    coolmic_buffer_t *next;
};

static void __default_free(void *content, void *userdata)
{
    (void)userdata;
    free(content);
}

coolmic_buffer_t   *coolmic_buffer_new(size_t length, const void *copy, void *take, void (*xfree)(void *content, void *userdata), void *userdata)
{
    coolmic_buffer_t *self = calloc(1, sizeof(*self));

    if (!self)
        return NULL;

    self->refc      = 1;
    self->length    = length;
    self->userdata  = userdata;

    if (take) {
        self->content = take;
        self->free = xfree;
    } else {
        self->content = calloc(1, length);
        if (!self->content) {
            free(self);
            return NULL;
        }
    }

    if (!self->free)
        self->free = __default_free;

    if (copy)
        memcpy(self->content, copy, length);

    return self;
}

coolmic_buffer_t   *coolmic_buffer_new_simple(size_t length, void **content)
{
    coolmic_buffer_t *self = coolmic_buffer_new(length, NULL, NULL, NULL, NULL);

    if (!self)
        return NULL;

    if (content)
        *content = coolmic_buffer_get_content(self);

    return self;
}

int                 coolmic_buffer_ref(coolmic_buffer_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    self->refc++;
    return COOLMIC_ERROR_NONE;
}

int                 coolmic_buffer_unref(coolmic_buffer_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;

    self->refc--;

    if (self->refc) {
        return COOLMIC_ERROR_NONE;
    }

    coolmic_buffer_set_next(self, NULL);

    self->free(self->content, self->userdata);
    free(self);

    return COOLMIC_ERROR_NONE;
}
            
void               *coolmic_buffer_get_content(coolmic_buffer_t *self)
{
    if (!self)
        return NULL;
    return self->content + self->offset;
}

ssize_t             coolmic_buffer_get_length(coolmic_buffer_t *self)
{
    if (!self)
        return -1;
    return self->length - self->offset;
}

int                 coolmic_buffer_set_offset(coolmic_buffer_t *self, size_t offset)
{
    size_t new_offset;

    if (!self)
        return COOLMIC_ERROR_FAULT;

    new_offset = self->offset + offset;

    if (self->length < new_offset)
        return COOLMIC_ERROR_INVAL;

    self->offset = new_offset;

    return COOLMIC_ERROR_NONE;
}

void               *coolmic_buffer_get_userdata(coolmic_buffer_t *self)
{
    if (!self)
        return NULL;
    return self->userdata;
}

int                 coolmic_buffer_set_userdata(coolmic_buffer_t *self, void *userdata)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    self->userdata = userdata;
    return COOLMIC_ERROR_NONE;
}

coolmic_buffer_t   *coolmic_buffer_get_next(coolmic_buffer_t *self)
{
    if (!self)
        return NULL;

    if (!self->next)
        return NULL;

    if (coolmic_buffer_ref(self->next) != COOLMIC_ERROR_NONE)
        return NULL;

    return self->next;
}

int                 coolmic_buffer_set_next(coolmic_buffer_t *self, coolmic_buffer_t *next)
{
    int err;

    if (!self)
        return COOLMIC_ERROR_FAULT;

    if (next) {
        err = coolmic_buffer_ref(next);
        if (err != COOLMIC_ERROR_NONE)
            return err;
    }

    if (self->next)
        coolmic_buffer_unref(self->next);

    self->next = next;

    return COOLMIC_ERROR_NONE;
}
