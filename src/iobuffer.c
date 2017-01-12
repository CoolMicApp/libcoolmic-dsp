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

/* This is a dummy sound driver. It supports record and playback.
 * In record mode it will read as zeros (silence).
 */

#include <stdlib.h>
#include <string.h>
#include <coolmic-dsp/iobuffer.h>
#include <coolmic-dsp/coolmic-dsp.h>

/* forward declare internally used structures */
struct coolmic_iobuffer {
    /* reference counter */
    size_t refc;
    /* buffer size */
    size_t size;
    /* buffer content */
    void *content;
    /* read pointer */
    size_t reader;
    /* write pointer */
    size_t writer;
    /* IO Handle */
    coolmic_iohandle_t *io;
};

/* Management of the encoder object */
coolmic_iobuffer_t   *coolmic_iobuffer_new(size_t size)
{
    coolmic_iobuffer_t *self;

    if (size < 4)
        return NULL;

    self = calloc(1, sizeof(*self));

    if (!self)
        return NULL;

    self->refc      = 1;
    self->size      = size;
    self->reader    = 0;
    self->writer    = 0;

    self->content   = calloc(1, size);
    if (!self->content) {
        free(self);
        return NULL;
    }

    return self;
}

int                 coolmic_iobuffer_ref(coolmic_iobuffer_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    self->refc++;
    return COOLMIC_ERROR_NONE;
}

int                 coolmic_iobuffer_unref(coolmic_iobuffer_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;

    self->refc--;

    if (self->refc) {
        return COOLMIC_ERROR_NONE;
    }

    coolmic_iohandle_unref(self->io);
    free(self->content);
    free(self);

    return COOLMIC_ERROR_NONE;
}

int                 coolmic_iobuffer_attach_iohandle(coolmic_iobuffer_t *self, coolmic_iohandle_t *handle)
{   
    if (!self) 
        return COOLMIC_ERROR_FAULT;
    if (self->io)
        coolmic_iohandle_unref(self->io);
    /* ignore errors here as handle is allowed to be NULL */
    coolmic_iohandle_ref(self->io = handle);
    return COOLMIC_ERROR_NONE;
}

static int __free(void *userdata)
{
    coolmic_iobuffer_t *self = userdata;

    return coolmic_iobuffer_unref(self);
}

#if 0
/* Needed headers: <stdio.h> and <ctype.h> */
static void __dump(coolmic_iobuffer_t *self, const char *call)
{
    unsigned char p;
    size_t datalen;
    size_t i;

    if (!call)
        call = "<<<?>>>";

    if (self->reader > self->writer) {
        datalen = self->size - self->reader + self->writer;
    } else {
        datalen = self->writer - self->reader;
    }

    fprintf(stderr, "%s: self=%p{.refc=%zu, .size=%zu, .reader=%zu, .writer=%zu, .content[] = {", call, self, self->refc, self->size, self->reader, self->writer);
    for (i = 0; i < self->size; i++) {
        if (self->reader == i)
            fprintf(stderr, "reader: ");
        if (self->writer == i)
            fprintf(stderr, "writer: ");
        p = ((unsigned char*)self->content)[i];
        fprintf(stderr, "0x%.2x \"%c\", ", p, isprint(p) ? p : '.');
    }
    fprintf(stderr, "}, .io=%p...} /* data in buffer: %zu */\n", self->io, datalen);
}
#else
#define __dump(x,y)
#endif

static ssize_t __read(void *userdata, void *buffer, size_t len)
{
    coolmic_iobuffer_t *self = userdata;
    size_t end;

    /* If reader > writer: we can read up to size-1.
     * Else: we can read up till writer-1
     */

    if (self->reader > self->writer) {
        end = self->size;
    } else {
        end = self->writer;
    }

    if (len > (end - self->reader)) {
        len = end - self->reader;
    }

    __dump(self, "__read");

    memcpy(buffer, self->content + self->reader, len);

    self->reader += len;
    if (self->reader == self->size)
        self->reader = 0;

    return len;
}

static int __eof(void *userdata)
{
    coolmic_iobuffer_t *self = userdata;

    /* Check if there is still data in the buffer.
     * If so it can not be eof.
     */

    __dump(self, "__eof");

    if (self->reader != self->writer)
        return COOLMIC_ERROR_NONE;

    /* There is no data in the buffer. If we do not have an IO handle this is EOF. */
    if (!self->io)
        return 1;

    /* We have no data in the buffer but an IO handle. Just forward the question to the next layer. */
    return coolmic_iohandle_eof(self->io);
}

coolmic_iohandle_t *coolmic_iobuffer_get_iohandle(coolmic_iobuffer_t *self)
{
    coolmic_iohandle_t *ret;

    if (coolmic_iobuffer_ref(self) != COOLMIC_ERROR_NONE)
        return NULL;

    ret = coolmic_iohandle_new(self, __free, __read, __eof);
    if (!ret)
        coolmic_iobuffer_unref(self);

    return ret;
}

/* This function is to iterate. It will try to fill the ring buffer.
 * If the input is non-blocking this will also be non-blocking.
 */
int                 coolmic_iobuffer_iter(coolmic_iobuffer_t *self)
{
    ssize_t ret;
    size_t end;
    size_t len;

    /* check if we have a valid object */
    if (!self)
        return COOLMIC_ERROR_FAULT;
    if (!self->io)
        return COOLMIC_ERROR_INVAL;

    /* Now calculate the amount of data to read and the used address space. */
    if (self->reader > self->writer) {
        end = self->reader - 1;
    } else if (self->reader) {
        end = self->size;
    } else {
        end = self->size - 1;
    }

    len = end - self->writer;

    if (!len)
        return COOLMIC_ERROR_BUSY;

    /* avoid huge reads */
    if (len > 8192)
        len = 8192;

    ret = coolmic_iohandle_read(self->io, self->content + self->writer, len);

    __dump(self, "iter");
    if (ret < 0) {
        return COOLMIC_ERROR_GENERIC;
    } else if (ret == 0) {
        return COOLMIC_ERROR_NONE;
    }

    self->writer += ret;
    if (self->writer == self->size)
        self->writer = 0;

    return COOLMIC_ERROR_NONE;
}
