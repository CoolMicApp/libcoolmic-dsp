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

/* Please see the corresponding header file for details of this API. */

#include <stdlib.h>
#include <coolmic-dsp/tee.h>

#define MAX_READERS 4

typedef struct {
    /* reference back to the parent object */
    coolmic_tee_t *parent;

    /* index of the current handle */
    size_t index;
} backpointer_t;

struct coolmic_tee {
    /* reference counter */
    size_t refc;

    /* number of readers */
    size_t readers;

    /* index of next reader for requests using coolmic_tee_get_iohandle(self, -1) */
    ssize_t next_reader;

    /* IO buffer length */
    size_t buffer_len;

    /* IO buffer */
    void *buffer;

    /* input IO handle */
    coolmic_iohandle_t *in;

    /* output IO handles */
    coolmic_iohandle_t *out[MAX_READERS];

    /* offsets of IO handles into buffer */
    size_t offset[MAX_READERS];

    /* backpointers */
    backpointer_t backpointer[MAX_READERS];
};

static ssize_t __read(void *userdata, void *buffer, size_t len);
static int __eof(void *userdata);

/* Management of the tee object */
coolmic_tee_t      *coolmic_tee_new(size_t readers)
{
    coolmic_tee_t *ret;
    size_t i;

    if (readers < 1 || readers > MAX_READERS)
        return NULL;

    ret = calloc(1, sizeof(coolmic_tee_t));
    if (!ret)
        return NULL;

    ret->refc = 1;
    ret->readers = readers;

    for (i = 0; i < readers; i++) {
        ret->backpointer[i].parent = ret;
        ret->backpointer[i].index = i;
        ret->out[i] = coolmic_iohandle_new(&(ret->backpointer[i]), (int (*)(void*))coolmic_tee_unref, __read, __eof);
    }

    return ret;
}

int                 coolmic_tee_ref(coolmic_tee_t *self)
{
    if (!self)
        return -1;
    self->refc++;
    return 0;
}

int                 coolmic_tee_unref(coolmic_tee_t *self)
{
    if (!self)
        return -1;
    self->refc--;
    if (self->refc != 0) /* TODO: update this */
        return 0;

    coolmic_iohandle_unref(self->in);
    if (self->buffer)
        free(self->buffer);
    free(self);

    return 0;
}

/* This is to attach the IO Handle the tee module should read from */
int                 coolmic_tee_attach_iohandle(coolmic_tee_t *self, coolmic_iohandle_t *handle)
{
    if (!self)
        return -1;
    if (self->in)
        coolmic_iohandle_unref(self->in);
    /* ignore errors here as handle is allowed to be NULL */
    coolmic_iohandle_ref(self->in = handle);
    return 0;
}

/* This function is to get the IO Handles users can read from */
coolmic_iohandle_t *coolmic_tee_get_iohandle(coolmic_tee_t *self, ssize_t index)
{
    if (!self)
        return NULL;
    if (index == -1)
        index = self->next_reader;
    if (index < 0 || index >= MAX_READERS)
        return NULL;

    self->next_reader = index + 1;

    coolmic_iohandle_ref(self->out[index]);
    return self->out[index];
}
