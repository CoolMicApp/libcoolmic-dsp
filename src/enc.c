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


#define COOLMIC_COMPONENT "libcoolmic-dsp/enc"
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <coolmic-dsp/coolmic-dsp.h>
#include <coolmic-dsp/enc.h>
#include "enc_private.h"

static int __start(coolmic_enc_t *self)
{
    static int count;
    int ret;

    if (self->state != STATE_NEED_INIT)
        return -1;

    srand(time(NULL) + count++); /* TODO FIXME: move this out */
    ogg_stream_init(&(self->os), rand());

    ret = self->cb.start(self);
    if (ret != COOLMIC_ERROR_NONE)
        return ret;

    self->state = STATE_RUNNING;

    return COOLMIC_ERROR_NONE;
}

static int __stop(coolmic_enc_t *self)
{
    int ret;

    ret = self->cb.stop(self);
    if (ret != COOLMIC_ERROR_NONE)
        return ret;

    ogg_stream_clear(&(self->os));
    memset(&(self->os), 0, sizeof(self->os));

    self->state = STATE_NEED_INIT;

    return COOLMIC_ERROR_NONE;
}

static int __need_new_page(coolmic_enc_t *self)
{
    int ret;
    int (*pageout)(ogg_stream_state *, ogg_page *) = ogg_stream_pageout;

    if (self->use_page_flush)
        pageout = ogg_stream_flush;

    if (self->state == STATE_NEED_INIT)
        __start(self);

    if (self->state == STATE_EOF && ogg_page_eos(&(self->og)))
        return -2; /* EOF */

    if (self->state == STATE_NEED_RESTART && ogg_page_eos(&(self->og)))
        self->state = STATE_NEED_RESET;

    while (pageout(&(self->os), &(self->og)) == 0) {
        /* we reached end of buffer */
        self->use_page_flush = 0; 
        pageout = ogg_stream_pageout;

        if (self->state == STATE_NEED_RESET) {
            __stop(self);
            __start(self);
            return -2;
        }

        ret = self->cb.process(self);
        if (ret == -1) {
            self->offset_in_page = -1;
            return -1;
        } else if (ret == -2) {
            return -1;
        }
        if (self->use_page_flush)
            pageout = ogg_stream_flush;
    }

    self->offset_in_page = 0;
    return 0;
}

static ssize_t __read(void *userdata, void *buffer, size_t len)
{
    coolmic_enc_t *self = userdata;
    size_t offset;
    size_t max_len;
    int ret;

    if (self->offset_in_page == -1)
        return COOLMIC_ERROR_GENERIC;

    if (self->state == STATE_NEED_INIT || self->offset_in_page == (self->og.header_len + self->og.body_len)) {
        ret = __need_new_page(self);
        if (ret == -2) {
            return 0;
        } else if (ret == -1) {
            return COOLMIC_ERROR_GENERIC;
        }
    }

    if (self->offset_in_page < self->og.header_len) {
        max_len = self->og.header_len - self->offset_in_page;
        len = (len > max_len) ? max_len : len;
        memcpy(buffer, self->og.header + self->offset_in_page, len);
        self->offset_in_page += len;
        return len;
    }

    offset = self->offset_in_page - self->og.header_len;
    max_len = self->og.body_len - offset;
    len = (len > max_len) ? max_len : len;
    memcpy(buffer, self->og.body + offset, len);
    self->offset_in_page += len;
    return len;
}

static int __eof(void *userdata)
{
    coolmic_enc_t *self = userdata;

    if (self->offset_in_page == (self->og.header_len + self->og.body_len) && self->state == STATE_EOF)
        return 1; /* bool */

    return 0; /* bool */
}

coolmic_enc_t      *coolmic_enc_new(const char *codec, uint_least32_t rate, unsigned int channels)
{
    coolmic_enc_t *ret;
    coolmic_enc_cb_t cb;

    if (!rate || !channels)
        return NULL;

    if (strcasecmp(codec, COOLMIC_DSP_CODEC_VORBIS) == 0) {
        cb = __coolmic_enc_cb_vorbis;
#ifdef HAVE_ENC_OPUS
    } else if (strcasecmp(codec, COOLMIC_DSP_CODEC_OPUS) == 0) {
        cb = __coolmic_enc_cb_opus;
#endif
    } else {
        /* unknown codec */
        return NULL;
    }

    ret = calloc(1, sizeof(coolmic_enc_t));
    if (!ret)
        return NULL;

    ret->refc = 1;
    ret->state = STATE_NEED_INIT;
    ret->rate = rate;
    ret->channels = channels;
    ret->quality  = 0.1;
    ret->cb = cb;

    coolmic_enc_ref(ret);
    ret->out = coolmic_iohandle_new(ret, (int (*)(void*))coolmic_enc_unref, __read, __eof);

    return ret;
}

int                 coolmic_enc_ref(coolmic_enc_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    self->refc++;
    return COOLMIC_ERROR_NONE;
}

int                 coolmic_enc_unref(coolmic_enc_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    self->refc--;
    if (self->refc != 1) /* 1 = reference in self->out */
        return COOLMIC_ERROR_NONE;

    __stop(self);

    coolmic_iohandle_unref(self->in);
    coolmic_iohandle_unref(self->out);
    coolmic_metadata_unref(self->metadata);
    free(self);

    return COOLMIC_ERROR_NONE;
}

int                 coolmic_enc_reset(coolmic_enc_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    if (self->state != STATE_RUNNING && self->state != STATE_EOF)
        return COOLMIC_ERROR_GENERIC;

    /* send EOF event */
    self->state = STATE_EOF;

    /* no process to EOS page and then reset the encoder */
    while (__need_new_page(self) == 0)
        if (ogg_page_eos(&(self->og)))
            break;

    self->state = STATE_NEED_RESET;
    __need_new_page(self); /* buffer the first page of the new segment. */

    return COOLMIC_ERROR_NONE;
}

static inline int __restart(coolmic_enc_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    if (self->state != STATE_RUNNING && self->state != STATE_EOF)
        return COOLMIC_ERROR_GENERIC;
    self->state = STATE_NEED_RESTART;
    return COOLMIC_ERROR_NONE;
}

int                 coolmic_enc_ctl(coolmic_enc_t *self, coolmic_enc_op_t op, ...)
{
    va_list ap;
    int ret = COOLMIC_ERROR_BADRQC;
    union {
        double *fp;
        coolmic_metadata_t *md;
        coolmic_metadata_t **mdp;
    } tmp;

    if (!self)
        return COOLMIC_ERROR_FAULT;

    va_start(ap, op);

    switch (op) {
        case COOLMIC_ENC_OP_INVALID:
            ret = COOLMIC_ERROR_INVAL;
        break;
        case COOLMIC_ENC_OP_NONE:
            ret = COOLMIC_ERROR_NONE;
        break;
        case COOLMIC_ENC_OP_RESET:
            ret = coolmic_enc_reset(self);
        break;
        case COOLMIC_ENC_OP_RESTART:
            ret = __restart(self);
        break;
        case COOLMIC_ENC_OP_GET_QUALITY:
            tmp.fp = va_arg(ap, double*);
            *(tmp.fp) = self->quality;
            ret = COOLMIC_ERROR_NONE;
        break;
        case COOLMIC_ENC_OP_SET_QUALITY:
            self->quality = va_arg(ap, double);
            ret = COOLMIC_ERROR_NONE;
        break;
        case COOLMIC_ENC_OP_GET_METADATA:
            tmp.mdp = va_arg(ap, coolmic_metadata_t**);
            ret = coolmic_metadata_ref(*(tmp.mdp) = self->metadata);
        break;
        case COOLMIC_ENC_OP_SET_METADATA:
            tmp.md = va_arg(ap, coolmic_metadata_t*);
            if (tmp.md) {
                ret = coolmic_metadata_ref(tmp.md);
                if (ret == COOLMIC_ERROR_NONE) {
                    coolmic_metadata_unref(self->metadata);
                    self->metadata = tmp.md;
                }
            } else {
                coolmic_metadata_unref(self->metadata);
                self->metadata = NULL;
                ret = COOLMIC_ERROR_NONE;
            }
        break;
    }

    va_end(ap);

    return ret;
}

int                 coolmic_enc_attach_iohandle(coolmic_enc_t *self, coolmic_iohandle_t *handle)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    if (self->in)
        coolmic_iohandle_unref(self->in);
    /* ignore errors here as handle is allowed to be NULL */
    coolmic_iohandle_ref(self->in = handle);
    return COOLMIC_ERROR_NONE;
}

coolmic_iohandle_t *coolmic_enc_get_iohandle(coolmic_enc_t *self)
{
    if (!self)
        return NULL;
    coolmic_iohandle_ref(self->out);
    return self->out;
}
