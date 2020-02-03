/*
 *      Copyright (C) Jordan Erickson                     - 2014-2020,
 *      Copyright (C) Löwenfelsen UG (haftungsbeschränkt) - 2015-2020
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

#define COOLMIC_COMPONENT "libcoolmic-dsp/tee"
#include <stdlib.h>
#include <string.h>
#include "types_private.h"
#include <coolmic-dsp/tee.h>
#include <coolmic-dsp/iohandle.h>
#include <coolmic-dsp/coolmic-dsp.h>
#include <coolmic-dsp/logging.h>

#define MAX_READERS 4

typedef struct {
    /* reference back to the parent object */
    coolmic_tee_t *parent;

    /* index of the current handle */
    size_t index;
} backpointer_t;

struct coolmic_tee {
    /* base type */
    igloo_ro_base_t __base;

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

    /* offsets of IO handles into buffer */
    size_t offset[MAX_READERS];
};

static void __free(igloo_ro_t self)
{
    coolmic_tee_t *tee = igloo_RO_TO_TYPE(self, coolmic_tee_t);

    igloo_ro_unref(tee->in);
    free(tee->buffer);
}

igloo_RO_PUBLIC_TYPE(coolmic_tee_t,
        igloo_RO_TYPEDECL_FREE(__free)
        );

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

    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Buffer adjustment for %zu bytes", len_request);

    if (len_request > self->buffer_len) {
        buffer_new = realloc(self->buffer, len_request);
        if (buffer_new) {
            self->buffer = buffer_new;
            self->buffer_len = len_request;
        } else {
            coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, COOLMIC_ERROR_NONE, "Can not allocate new buffer");
        }
    }

    if (!self->buffer) {
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, COOLMIC_ERROR_NONE, "Unabled to allocate any buffer");
        return;
    }

    /* look up the common (minimum) offset of all reads into the buffer */
    min_offset = self->buffer_fill;
    for (i = 0; i < self->readers; i++) {
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Reader %zu's offset is %zu byte", i, self->offset[i]);
        if (self->offset[i] < min_offset) {
            min_offset = self->offset[i];
        }
    }

    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Buffer's minimum offset is %zu byte", min_offset);

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
    ssize_t ret;

    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Physical read request, len_request=%zu", len_request);
    __readjust_buffer(self, len_request);

    iter = self->buffer_len - self->buffer_fill;

    /* check if there is some kind of problem with the buffer */
    if (!self->buffer || !iter) {
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, COOLMIC_ERROR_NOMEM, "Physical read failed, self->buffer=%p, iter=%zu", self->buffer, iter);
        return -1;
    }

    if (iter > len_request)
        iter = len_request;

    ret = coolmic_iohandle_read(self->in, self->buffer + self->buffer_fill, iter);
    if (ret < 1) {
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, COOLMIC_ERROR_NONE, "Physical read on backend failed");
        return ret;
    }

    self->buffer_fill += ret;

    return ret;
}

static ssize_t __read(void *userdata, void *buffer, size_t len)
{
    backpointer_t *backpointer = userdata;
    coolmic_tee_t *self = backpointer->parent;
    ssize_t ret = 0;
    size_t iter;

    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Read request, buffer=%p, len=%zu", buffer, len);

    do {
        iter = self->buffer_fill - self->offset[backpointer->index];
        if (!iter) {
            if (__read_phy(self, len) < 1) {
                coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Read request satisfied, ret=%zu", ret);
                return ret;
            }
            iter = self->buffer_fill - self->offset[backpointer->index];
        }

        if (iter > len)
            iter = len;

        memcpy(buffer, self->buffer + self->offset[backpointer->index], iter);

        ret += iter;
        self->offset[backpointer->index] += iter;

        if (iter == len) {
            coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Read request satisfied, ret=%zu", ret);
            return ret;
        }

        buffer += iter;
        len -= iter;
    } while (iter);

    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Read request satisfied, ret=%zu", ret);

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
coolmic_tee_t      *coolmic_tee_new(const char *name, igloo_ro_t associated, size_t readers)
{
    coolmic_tee_t *ret;

    if (readers < 1 || readers > MAX_READERS)
        return NULL;

    ret = igloo_ro_new_raw(coolmic_tee_t, name, associated);
    if (!ret)
        return NULL;

    ret->readers = readers;

    return ret;
}

/* This is to attach the IO Handle the tee module should read from */
int                 coolmic_tee_attach_iohandle(coolmic_tee_t *self, coolmic_iohandle_t *handle)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    if (self->in)
        igloo_ro_unref(self->in);
    /* ignore errors here as handle is allowed to be NULL */
    igloo_ro_ref(self->in = handle);
    return COOLMIC_ERROR_NONE;
}

static int __free_backpointer(void *arg)
{
    backpointer_t *backpointer = arg;

    igloo_ro_unref(backpointer->parent);

    free(backpointer);

    return 0;
}

/* This function is to get the IO Handles users can read from */
coolmic_iohandle_t *coolmic_tee_get_iohandle(coolmic_tee_t *self, ssize_t index)
{
    backpointer_t *backpointer;
    coolmic_iohandle_t *ret;

    if (!self)
        return NULL;
    if (index == -1)
        index = self->next_reader;
    if (index < 0 || index >= MAX_READERS)
        return NULL;

    self->next_reader = index + 1;

    backpointer = calloc(1, sizeof(backpointer_t));
    if (!backpointer)
        return NULL;


    igloo_ro_ref(self);
    backpointer->parent = self;
    backpointer->index = index;

    ret = coolmic_iohandle_new(NULL, igloo_RO_NULL, backpointer, __free_backpointer, __read, __eof);

    if (!ret)
        __free_backpointer(backpointer);

    return ret;
}
