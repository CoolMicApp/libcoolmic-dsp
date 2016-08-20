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
#include <string.h>
#include <stdlib.h>
#include <coolmic-dsp/coolmic-dsp.h>
#include <coolmic-dsp/enc.h>
#include "enc_private.h"

static int libopuserror2error(const int err) {
    switch (err) {
        case OPUS_OK:
            return COOLMIC_ERROR_NONE;
        break;
        case OPUS_BAD_ARG:
            return COOLMIC_ERROR_INVAL;
        break;
        case OPUS_BUFFER_TOO_SMALL:
            return COOLMIC_ERROR_FAULT;
        break;
        case OPUS_INVALID_PACKET:
            return COOLMIC_ERROR_INVAL;
        break;
        case OPUS_UNIMPLEMENTED:
            return COOLMIC_ERROR_NOSYS;
        break;
        case OPUS_INVALID_STATE:
            return COOLMIC_ERROR_INVAL;
        break;
        case OPUS_ALLOC_FAIL:
            return COOLMIC_ERROR_NOMEM;
        break;

        case OPUS_INTERNAL_ERROR:
        default:
            return COOLMIC_ERROR_GENERIC;
        break;
    }
}


static void __opus_write_uint32(unsigned char buf[4], uint32_t val)
{
    buf[0] = (val & 0x000000FF) >>  0;
    buf[1] = (val & 0x0000FF00) >>  8;
    buf[2] = (val & 0x00FF0000) >> 18;
    buf[3] = (val & 0xFF000000) >> 24;
}

static int __opus_build_header(unsigned char header[19], coolmic_enc_t *self)
{
    memcpy(header, "OpusHead", 8); /* magic */
    header[8] = 1; /* version */
    header[9] = self->channels; /* channel count */
    header[10] = 0; /* pre-skip LSB */
    header[11] = 0; /* pre-skip MSB */
    __opus_write_uint32(header+12, self->rate);
    header[16] = 0; /* Output Gain LSB */
    header[17] = 0; /* Output Gain MSB */
    header[18] = 0; /* Channel Mapping Family */
    return COOLMIC_ERROR_NONE;
}

static int __opus_packetin_header(coolmic_enc_t *self)
{
    unsigned char header[19];
    int err;

    err = __opus_build_header(header, self);
    if (err != COOLMIC_ERROR_NONE)
        return err;

    memset(&(self->op), 0, sizeof(self->op));
    self->op.packet = header;
    self->op.bytes = 19;
    self->op.b_o_s = 1;
    self->op.e_o_s = 0;
    self->op.granulepos = self->codec.opus.granulepos;
    self->op.packetno = self->codec.opus.packetno++;


    ogg_stream_packetin(&(self->os), &(self->op));
    self->use_page_flush = 1;
    return COOLMIC_ERROR_NONE;
}

#define _add_length(key,value) retlen += strlen((key)) + strlen((value)) + 1 + 4; tags++
#define _add_tag(key,value) __opus_write_uint32(p, strlen((key)) + strlen((value)) + 1); 
static void *__opus_write_tag(void *buf, const char *key, const char *value)
{
    size_t key_len = strlen(key);
    size_t value_len = (value) ? strlen(value) : 0;

    __opus_write_uint32(buf, (value_len) ? key_len + value_len + 1 : key_len);
    buf += 4;
    memcpy(buf, key, key_len);
    buf += key_len;
    if (!value_len)
        return buf;

    memcpy(buf, "=", 1);
    buf++;

    memcpy(buf, value, value_len);
    buf += value_len;

    return buf;
}

static int __opus_build_tags(coolmic_enc_t *self, void **buffer, size_t *len)
{
    static const char vendor[] = "libcoolmic-dsp";
    const size_t vendor_len = strlen(vendor);
    size_t retlen = 12;
    size_t tags = 0;
    void *buf;

    retlen += vendor_len + 4;

    _add_length("ENCODER", "libcoolmic-dsp");

    buf = malloc(retlen);
    if (!buf)
        return COOLMIC_ERROR_NOMEM;
    *buffer = buf;
    *len = retlen;

    memcpy(buf, "OpusTags", 8);
    buf += 8;

    __opus_write_uint32(buf, vendor_len);
    buf += 4;
    memcpy(buf, vendor, vendor_len);
    buf += vendor_len;

    __opus_write_uint32(buf, tags);
    buf += 4;

    buf = __opus_write_tag(buf, "ENCODER", "libcoolmic-dsp");
    return COOLMIC_ERROR_NONE;
}

static int __opus_packetin_tags(coolmic_enc_t *self)
{   
    void *buf;
    size_t len;
    int err;

    err = __opus_build_tags(self, &buf, &len);
    if (err != COOLMIC_ERROR_NONE)
        return err;

    memset(&(self->op), 0, sizeof(self->op));
    self->op.packet = buf;
    self->op.bytes = len;
    self->op.b_o_s = 0;
    self->op.e_o_s = 0; 
    self->op.granulepos = self->codec.opus.granulepos;
    self->op.packetno = self->codec.opus.packetno++;


    err = ogg_stream_packetin(&(self->os), &(self->op));

    self->use_page_flush = 1;
    free(buf);
    return COOLMIC_ERROR_NONE;
}

static void* __opus_read_data(coolmic_enc_t *self, size_t frames)
{
    size_t len = frames * self->channels * 2;
    size_t todo;
    ssize_t ret;

    if (len > sizeof(self->codec.opus.buffer))
        return NULL;

    if (self->codec.opus.buffer_fill == len) {
        self->codec.opus.buffer_fill = 0;
        return self->codec.opus.buffer;
    } else if (self->codec.opus.buffer_fill < len) {
        todo = len - self->codec.opus.buffer_fill;
        ret = coolmic_iohandle_read(self->in, self->codec.opus.buffer + self->codec.opus.buffer_fill, todo);
        if (ret == (ssize_t)todo) {
            self->codec.opus.buffer_fill = 0;
            return self->codec.opus.buffer;
        } else if (ret < 1) {
            if (coolmic_iohandle_eof(self->in) == 1) {
                self->state = STATE_EOF;
            }
            return NULL;
        } else {
            self->codec.opus.buffer_fill += ret;
            return NULL;
        }
    } else {
        return NULL;
    }
}

static int __opus_packetin_data(coolmic_enc_t *self)
{
    size_t frames = 2880;
    void *data = __opus_read_data(self, frames);
    opus_int32 len;
    unsigned char buffer[4096];

    if (!data)
        return COOLMIC_ERROR_GENERIC;

    len = opus_encode(self->codec.opus.enc, data, frames, buffer, sizeof(buffer));

    if (len < 0)
        return libopuserror2error(len);

    self->codec.opus.granulepos += frames;

    memset(&(self->op), 0, sizeof(self->op));
    self->op.packet = buffer;
    self->op.bytes = len;
    self->op.b_o_s = 0;
    self->op.e_o_s = 0;
    self->op.granulepos = self->codec.opus.granulepos;
    self->op.packetno = self->codec.opus.packetno++;

    if (self->state == STATE_EOF || self->state == STATE_NEED_RESET || self->state == STATE_NEED_RESTART) {
        self->op.e_o_s = 1;
        self->codec.opus.state = COOLMIC_ENC_OPUS_STATE_EOF;
        self->use_page_flush = 1;
    }

    ogg_stream_packetin(&(self->os), &(self->op));

    return COOLMIC_ERROR_NONE;
}

static int __opus_start_encoder(coolmic_enc_t *self)
{
    int error;

    if (self->channels < 1 || self->channels > 2)
        return COOLMIC_ERROR_INVAL;

    if (self->rate != 48000)
        return COOLMIC_ERROR_INVAL;

    self->codec.opus.enc = opus_encoder_create(self->rate, self->channels, OPUS_APPLICATION_AUDIO, &error);
    if (!self->codec.opus.enc) {
        return libopuserror2error(error);
    }

    self->codec.opus.state = COOLMIC_ENC_OPUS_STATE_HEAD;
    self->codec.opus.granulepos = 0;
    self->codec.opus.packetno = 0;

    return COOLMIC_ERROR_NONE;
}

static int __opus_stop_encoder(coolmic_enc_t *self)
{
    if (self->codec.opus.enc) {
        opus_encoder_destroy(self->codec.opus.enc);
        self->codec.opus.enc = NULL;
    }
    return COOLMIC_ERROR_NONE;
}

static int __opus_process(coolmic_enc_t *self)
{
    int err;

    switch (self->codec.opus.state) {
        case COOLMIC_ENC_OPUS_STATE_HEAD:
            if ((err =__opus_packetin_header(self)) != COOLMIC_ERROR_NONE)
                return err;
            self->codec.opus.state = COOLMIC_ENC_OPUS_STATE_TAGS;
        break;
        case COOLMIC_ENC_OPUS_STATE_TAGS:
            if ((err =__opus_packetin_tags(self)) != COOLMIC_ERROR_NONE)
                return err;
            self->codec.opus.state = COOLMIC_ENC_OPUS_STATE_DATA;
        break;
        case COOLMIC_ENC_OPUS_STATE_DATA:
            if ((err =__opus_packetin_data(self)) != COOLMIC_ERROR_NONE)
                return err;
        break;
        case COOLMIC_ENC_OPUS_STATE_EOF:
            return 0;
        break;
    }
    return COOLMIC_ERROR_INVAL;
}

const coolmic_enc_cb_t __coolmic_enc_cb_opus = {
    .start = __opus_start_encoder,
    .stop = __opus_stop_encoder,
    .process = __opus_process
};
