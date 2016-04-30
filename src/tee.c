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
#include <string.h>
#include <coolmic-dsp/tee.h>
#include <coolmic-dsp/iohandle.h>
#include <coolmic-dsp/coolmic-dsp.h>

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

    /* IO buffer fill */
    size_t buffer_fill;

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

static void __readjust_buffer(coolmic_tee_t *self, size_t len_request)
{
    void *buffer_new;
    size_t i;
    size_t min_offset;

    /* request a new buffer if needed
     * We limit this to range of 1024 to 8192 to avoid both short buffers (performance)
     * as well as long buffers (likely invalid reads).
     */
    if (len_request < 1024) {
        len_request = 1024;
    } else if (len_request > 8192) {
        len_request = 8192;
    }

    if (len_request > self->buffer_len) {
        buffer_new = realloc(self->buffer, len_request);
        if (buffer_new) {
            self->buffer = buffer_new;
            self->buffer_len = len_request;
        }
    }

    if (!self->buffer)
        return;

    /* look up the common (minimum) offset of all reads into the buffer */
    min_offset = self->buffer_fill;
    for (i = 0; i < self->readers; i++)
        if (self->offset[i] < min_offset)
            min_offset = self->offset[i];

    /* if we got a minimum offset > 0 we can move what we have to the begin of the buffer */
    if (min_offset > 0) {
        memmove(self->buffer, self->buffer + min_offset, self->buffer_fill - min_offset);
        self->buffer_fill -= min_offset;

        for (i = 0; i < self->readers; i++)
            self->offset[i] -= min_offset;
    }
}

static ssize_t __read_phy(coolmic_tee_t *self, size_t len_request)
{
    size_t iter;
    size_t ret;

    __readjust_buffer(self, len_request);

    iter = self->buffer_len - self->buffer_fill;

    /* check if there is some kind of problem with the buffer */
    if (!self->buffer || !iter)
        return COOLMIC_ERROR_NOMEM;

    if (iter > len_request)
        iter = len_request;

    ret = coolmic_iohandle_read(self->in, self->buffer + self->buffer_fill, iter);
    if (ret < 1)
        return ret;

    self->buffer_fill += ret;

    return ret;
}

static ssize_t __read(void *userdata, void *buffer, size_t len)
{
    backpointer_t *backpointer = userdata;
    coolmic_tee_t *self = backpointer->parent;
    ssize_t ret = 0;
    size_t iter;

    do {
        iter = self->buffer_fill - self->offset[backpointer->index];
        if (!iter) {
            if (__read_phy(self, len) < 1)
                return ret;
            iter = self->buffer_fill - self->offset[backpointer->index];
        }

        if (iter > len)
            iter = len;

        memcpy(buffer, self->buffer + self->offset[backpointer->index], iter);

        ret += iter;
        self->offset[backpointer->index] += iter;

        if (iter == len)
            return ret;

        buffer += iter;
        len -= iter;
    } while (iter);

    return ret;
}

static int __eof(void *userdata)
{
    backpointer_t *backpointer = userdata;
    coolmic_tee_t *self = backpointer->parent;

    if (self->offset[backpointer->index] < self->buffer_fill)
        return 0; /* bool */

    return coolmic_iohandle_eof(self->in);
}

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
        return COOLMIC_ERROR_FAULT;
    self->refc++;
    return COOLMIC_ERROR_NONE;
}

int                 coolmic_tee_unref(coolmic_tee_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    self->refc--;
    if (self->refc != 0) /* TODO: update this */
        return COOLMIC_ERROR_NONE;

    coolmic_iohandle_unref(self->in);
    if (self->buffer)
        free(self->buffer);
    free(self);

    return COOLMIC_ERROR_NONE;
}

/* This is to attach the IO Handle the tee module should read from */
int                 coolmic_tee_attach_iohandle(coolmic_tee_t *self, coolmic_iohandle_t *handle)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    if (self->in)
        coolmic_iohandle_unref(self->in);
    /* ignore errors here as handle is allowed to be NULL */
    coolmic_iohandle_ref(self->in = handle);
    return COOLMIC_ERROR_NONE;
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
