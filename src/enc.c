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

#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <coolmic-dsp/coolmic-dsp.h>
#include <coolmic-dsp/enc.h>
#include <vorbis/vorbisenc.h>

typedef enum coolmic_enc_state {
    STATE_NEED_INIT = 0,
    STATE_RUNNING,
    STATE_EOF,
    STATE_NEED_RESET
} coolmic_enc_state_t;

struct coolmic_enc {
    size_t refc;

    /* overall state */
    coolmic_enc_state_t state;

    /* Audio */
    uint_least32_t rate;
    unsigned int channels;

    /* IO Handles */
    coolmic_iohandle_t *in;
    coolmic_iohandle_t *out;

    /* Ogg: */
    ogg_stream_state os; /* take physical pages, weld into a logical
                            stream of packets */
    ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
    ogg_packet       op; /* one raw packet of data for decode */

    ssize_t offset_in_page;

    int use_page_flush;  /* if set the next requests for pages will use flush not normal pageout.
                          * This is reset when the buffer is empty again.
                          */

    /* Vorbis: */
    vorbis_info      vi; /* struct that stores all the static vorbis bitstream
                            settings */
    vorbis_comment   vc; /* struct that stores all the user comments */

    vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
    vorbis_block     vb; /* local working space for packet->PCM decode */

    float quality;       /* quality level, -0.1 to 1.0 */
};

static int __vorbis_start_encoder(coolmic_enc_t *self)
{
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    if (self->state != STATE_NEED_INIT)
        return -1;

    vorbis_info_init(&(self->vi));
    if (vorbis_encode_init_vbr(&(self->vi), self->channels, self->rate, self->quality) != 0)
        return -1;

    vorbis_comment_init(&(self->vc));
    vorbis_comment_add_tag(&(self->vc), "ENCODER", "libcoolmic-dsp");

    vorbis_analysis_init(&(self->vd), &(self->vi));
    vorbis_block_init(&(self->vd), &(self->vb));

    srand(time(NULL)); /* TODO FIXME: move this out */
    ogg_stream_init(&(self->os), rand());

    vorbis_analysis_headerout(&(self->vd), &(self->vc), &header, &header_comm, &header_code);
    ogg_stream_packetin(&(self->os), &header); /* automatically placed in its own page */
    ogg_stream_packetin(&(self->os), &header_comm);
    ogg_stream_packetin(&(self->os), &header_code);

    self->use_page_flush = 1;
    self->state = STATE_RUNNING;

    return 0;
}

static int __vorbis_stop_encoder(coolmic_enc_t *self)
{
    ogg_stream_clear(&(self->os));
    vorbis_block_clear(&(self->vb));
    vorbis_dsp_clear(&(self->vd));
    vorbis_comment_clear(&(self->vc));
    vorbis_info_clear(&(self->vi));

    memset(&(self->os), 0, sizeof(self->os));
    memset(&(self->vb), 0, sizeof(self->vb));
    memset(&(self->vd), 0, sizeof(self->vd));
    memset(&(self->vc), 0, sizeof(self->vc));
    memset(&(self->vi), 0, sizeof(self->vi));
    self->state = STATE_NEED_INIT;
    return 0;
}

static int __vorbis_read_data(coolmic_enc_t *self)
{
    char buffer[1024];
    ssize_t ret;
    float **vbuffer;
    const int16_t *in = (int16_t*)buffer;
    unsigned int c;
    size_t i = 0;

    if (self->state == STATE_EOF || self->state == STATE_NEED_RESET) {
        vorbis_analysis_wrote(&(self->vd), 0);
        return 0;
    }

    ret = coolmic_iohandle_read(self->in, buffer, sizeof(buffer));

    if (ret < 1) {
        if (coolmic_iohandle_eof(self->in) == 1) {
            vorbis_analysis_wrote(&(self->vd), 0);
            self->state = STATE_EOF;
            return -1;
        }
        return -2;
    }

    /* Have we got a strange nummber of bytes? */
    if (ret % (2 * self->channels)) {
        self->offset_in_page = -1;
        return -1;
    }

    vbuffer = vorbis_analysis_buffer(&(self->vd), ret / (2 * self->channels));

    while (ret) {
        for (c = 0; c < self->channels; c++)
            vbuffer[c][i] = *(in++) / 32768.f;
        i++;
        ret -= 2 * self->channels;
    }

    vorbis_analysis_wrote(&(self->vd), i);

    return 0;
}

static int __vorbis_process_flush(coolmic_enc_t *self)
{
    int ret = 0;

    while (vorbis_bitrate_flushpacket(&(self->vd), &(self->op))){
        ogg_stream_packetin(&(self->os), &(self->op));
        ret = 1;
    }

    return ret;
}

static int __vorbis_process(coolmic_enc_t *self)
{
    int err;

    if (__vorbis_process_flush(self))
        return 0;

    while (vorbis_analysis_blockout(&(self->vd), &(self->vb)) != 1) {
        if ((err = __vorbis_read_data(self)) != 0) {
            return err;
        }
    }

    vorbis_analysis(&(self->vb), NULL);
    vorbis_bitrate_addblock(&(self->vb));

    __vorbis_process_flush(self);
    return 0;
}

static int __need_new_page(coolmic_enc_t *self)
{
    int ret;
    int (*pageout)(ogg_stream_state *, ogg_page *) = ogg_stream_pageout;

    if (self->use_page_flush)
        pageout = ogg_stream_flush;

    if (self->state == STATE_NEED_INIT)
        __vorbis_start_encoder(self);

    if (self->state == STATE_EOF && ogg_page_eos(&(self->og)))
        return -2; /* EOF */

    while (pageout(&(self->os), &(self->og)) == 0) {
        /* we reached end of buffer */
        self->use_page_flush = 0; 
        pageout = ogg_stream_pageout;

        if (self->state == STATE_NEED_RESET) {
            __vorbis_stop_encoder(self);
            __vorbis_start_encoder(self);
        }

        ret = __vorbis_process(self);
        if (ret == -1) {
            self->offset_in_page = -1;
            return -1;
        } else if (ret == -2) {
            return -1;
        }
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

    if (!rate || !channels)
        return NULL;

    /* for now we only support Ogg/Vorbis */
    if (strcasecmp(codec, COOLMIC_DSP_CODEC_VORBIS) != 0)
        return NULL;

    ret = calloc(1, sizeof(coolmic_enc_t));
    if (!ret)
        return NULL;

    ret->refc = 1;
    ret->state = STATE_NEED_INIT;
    ret->rate = rate;
    ret->channels = channels;
    ret->quality  = 0.1;

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

    __vorbis_stop_encoder(self);

    coolmic_iohandle_unref(self->in);
    coolmic_iohandle_unref(self->out);
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

int                 coolmic_enc_ctl(coolmic_enc_t *self, coolmic_enc_op_t op, ...)
{
    va_list ap;
    int ret = COOLMIC_ERROR_BADRQC;
    union {
        double *fp;
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
        case COOLMIC_ENC_OP_GET_QUALITY:
            tmp.fp = va_arg(ap, double*);
            *(tmp.fp) = self->quality;
            ret = COOLMIC_ERROR_NONE;
        break;
        case COOLMIC_ENC_OP_SET_QUALITY:
            self->quality = va_arg(ap, double);
            ret = COOLMIC_ERROR_NONE;
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
