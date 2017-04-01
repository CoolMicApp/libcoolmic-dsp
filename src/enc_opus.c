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

#define COOLMIC_COMPONENT "libcoolmic-dsp/enc-opus"
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <coolmic-dsp/coolmic-dsp.h>
#include <coolmic-dsp/enc.h>
#include <coolmic-dsp/metadata.h>
#include "enc_private.h"

static void __opus_write_uint32(unsigned char buf[4], uint32_t val)
{
    buf[0] = (val & 0x000000FF) >>  0;
    buf[1] = (val & 0x0000FF00) >>  8;
    buf[2] = (val & 0x00FF0000) >> 16;
    buf[3] = (val & 0xFF000000) >> 24;
}

static int __opus_build_header(unsigned char header[19], coolmic_enc_t *self)
{
    memcpy(header, COMMON_OPUS_MAGIC_HEAD, COMMON_OPUS_MAGIC_HEAD_LEN); /* magic */
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
    coolmic_metadata_t *metadata = self->metadata;
    coolmic_metadata_tag_t *tag;
    const char *key, *value;
    size_t retlen = 12;
    size_t tags = 0;
    void *buf;
    int ret;

    retlen += vendor_len + 4;

    if (metadata) {
        ret = coolmic_metadata_iter_start(metadata);
        if (ret != COOLMIC_ERROR_NONE)
            return ret;
    }

    _add_length("ENCODER", "libcoolmic-dsp");

    while ((tag = coolmic_metadata_iter_next_tag(metadata))) {
        key = coolmic_metadata_iter_tag_key(tag);
        while ((value = coolmic_metadata_iter_tag_next_value(tag))) {
            _add_length(key, value);
        }
    }
    coolmic_metadata_iter_rewind(metadata);

    buf = malloc(retlen);
    if (!buf) {
        if (metadata)
            coolmic_metadata_iter_end(metadata);
        return COOLMIC_ERROR_NOMEM;
    }
    *buffer = buf;
    *len = retlen;

    memcpy(buf, COMMON_OPUS_MAGIC_TAGS, COMMON_OPUS_MAGIC_TAGS_LEN);
    buf += 8;

    __opus_write_uint32(buf, vendor_len);
    buf += 4;
    memcpy(buf, vendor, vendor_len);
    buf += vendor_len;

    __opus_write_uint32(buf, tags);
    buf += 4;

    buf = __opus_write_tag(buf, "ENCODER", "libcoolmic-dsp");

    while ((tag = coolmic_metadata_iter_next_tag(metadata))) {
        key = coolmic_metadata_iter_tag_key(tag);
        while ((value = coolmic_metadata_iter_tag_next_value(tag))) {
            buf = __opus_write_tag(buf, key, value);
        }
    }

    coolmic_metadata_iter_end(metadata);
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

    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "New data requested, %zu frames (%zu bytes)", frames, len);

    if (len > sizeof(self->codec.opus.buffer)) {
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, COOLMIC_ERROR_INVAL, "Request bigger than buffer");
        return NULL;
    }

    if (self->codec.opus.buffer_fill == len) {
        self->codec.opus.buffer_fill = 0;
        return self->codec.opus.buffer;
    } else if (self->codec.opus.buffer_fill < len) {
        todo = len - self->codec.opus.buffer_fill;
        ret = coolmic_iohandle_read(self->in, self->codec.opus.buffer + self->codec.opus.buffer_fill, todo);
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Requested data: requested %zu bytes, got %zi bytes", todo, ret);
        if (ret == (ssize_t)todo) {
            self->codec.opus.buffer_fill = 0;
            return self->codec.opus.buffer;
        } else if (ret < 1) {
            if (coolmic_iohandle_eof(self->in) == 1) {
                self->state = STATE_EOF;
            }
            coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, COOLMIC_ERROR_NONE, "Can not read more data from input");
            return NULL;
        } else {
            self->codec.opus.buffer_fill += ret;
            coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, COOLMIC_ERROR_NONE, "Got more data but not enough to satisfy request");
            return NULL;
        }
    } else {
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, COOLMIC_ERROR_INVAL, "Bad state, fill > len");
        return NULL;
    }
}

static int __opus_packetin_data(coolmic_enc_t *self)
{
    size_t frames = 2880;
    void *data = __opus_read_data(self, frames);
    opus_int32 len;
    unsigned char buffer[4096];
    int err;

    if (!data) {
        err = COOLMIC_ERROR_RETRY;
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, err, "No input data");
        return err;
    }

    len = opus_encode(self->codec.opus.enc, data, frames, buffer, sizeof(buffer));

    if (len < 0) {
        err = coolmic_common_opus_libopuserror2error(len);
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, err, "Encoder error");
        return err;
    }

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

static long int __opus_get_bitrate(coolmic_enc_t *self)
{
    register float q = self->quality;

    if (q < -.15) {
        return 32000;
    } else if (q < -.05) {
        return 45000;
    } else if (q < .05) {
        return 64000;
    } else if (q < .15) {
        return 80000;
    } else if (q < .25) {
        return 96000;
    } else if (q < .35) {
        return 112000;
    } else if (q < .45) {
        return 128000;
    } else if (q < .55) {
        return 160000;
    } else if (q < .65) {
        return 192000;
    } else if (q < .75) {
        return 224000;
    } else if (q < .85) {
        return 256000;
    } else if (q < .95) {
        return 320000;
    } else if (q < 1.05) {
        return 500000;
    } else {
        return 512000;
    }
}

static int __opus_stop_encoder(coolmic_enc_t *self)
{
    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_INFO, COOLMIC_ERROR_NONE, "Stop callback called");

    if (self->codec.opus.enc) {
        opus_encoder_destroy(self->codec.opus.enc);
        self->codec.opus.enc = NULL;
    }

    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_INFO, COOLMIC_ERROR_NONE, "Stop successful");
    return COOLMIC_ERROR_NONE;
}

static int __opus_start_encoder(coolmic_enc_t *self)
{
    int error;
    int ret;

    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_INFO, COOLMIC_ERROR_NONE, "Start callback called");

    if (self->channels < 1 || self->channels > 2) {
        ret = COOLMIC_ERROR_INVAL;
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, ret, "Start failed: bad number of channels (supported: 1, 2): %u", self->channels);
        return ret;
    }

    if (self->rate != COMMON_OPUS_RATE) {
        ret = COOLMIC_ERROR_INVAL;
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, ret, "Start failed: bad sampling rate (supported: %u): %u", COMMON_OPUS_RATE, self->channels);
        return ret;
    }

    self->codec.opus.enc = opus_encoder_create(self->rate, self->channels, OPUS_APPLICATION_AUDIO, &error);
    if (!self->codec.opus.enc) {
        ret = coolmic_common_opus_libopuserror2error(error);
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, ret, "Start failed: can not create encoder");
        return ret;
    }

    error = opus_encoder_ctl(self->codec.opus.enc, OPUS_SET_BITRATE(__opus_get_bitrate(self)));
    if (error != OPUS_OK) {
        __opus_stop_encoder(self);
        ret = coolmic_common_opus_libopuserror2error(error);
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, ret, "Start failed: can not set bitrate");
        return ret;
    }

    self->codec.opus.state = COOLMIC_ENC_OPUS_STATE_HEAD;
    self->codec.opus.granulepos = 0;
    self->codec.opus.packetno = 0;

    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_INFO, COOLMIC_ERROR_NONE, "Start successful");
    return COOLMIC_ERROR_NONE;
}

static int __opus_process(coolmic_enc_t *self)
{
    int err;

    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Process callback called");

    switch (self->codec.opus.state) {
        case COOLMIC_ENC_OPUS_STATE_HEAD:
            if ((err = __opus_packetin_header(self)) != COOLMIC_ERROR_NONE) {
                coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, err, "Process failed: can not create header");
                return -1;
            }
            self->codec.opus.state = COOLMIC_ENC_OPUS_STATE_TAGS;
            coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Process successful");
            return 0;
        break;
        case COOLMIC_ENC_OPUS_STATE_TAGS:
            if ((err = __opus_packetin_tags(self)) != COOLMIC_ERROR_NONE) {
                coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, err, "Process failed: can not create tags");
                return -1;
            }
            self->codec.opus.state = COOLMIC_ENC_OPUS_STATE_DATA;
            coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Process successful");
            return 0;
        break;
        case COOLMIC_ENC_OPUS_STATE_DATA:
            if ((err = __opus_packetin_data(self)) != COOLMIC_ERROR_NONE) {
                coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, err, "Process failed: can not process data");
                if (err == COOLMIC_ERROR_RETRY) {
                    return -2;
                } else {
                    return -1;
                }
            }
            coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Process successful");
            return 0;
        break;
        case COOLMIC_ENC_OPUS_STATE_EOF:
            coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Process successful");
            return 0;
        break;
    }

    err = COOLMIC_ERROR_INVAL;
    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_ERROR, err, "Process failed: invalid state");
    return -1;
}

const coolmic_enc_cb_t __coolmic_enc_cb_opus = {
    .start = __opus_start_encoder,
    .stop = __opus_stop_encoder,
    .process = __opus_process
};
