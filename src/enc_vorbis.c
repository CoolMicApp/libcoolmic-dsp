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
#include <coolmic-dsp/coolmic-dsp.h>
#include <coolmic-dsp/enc.h>
#include "enc_private.h"

static int __vorbis_start_encoder(coolmic_enc_t *self)
{
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;

    vorbis_info_init(&(self->vi));
    if (vorbis_encode_init_vbr(&(self->vi), self->channels, self->rate, self->quality) != 0)
        return -1;

    vorbis_comment_init(&(self->vc));
    vorbis_comment_add_tag(&(self->vc), "ENCODER", "libcoolmic-dsp");
    if (self->metadata)
        coolmic_metadata_add_to_vorbis_comment(self->metadata, &(self->vc));

    vorbis_analysis_init(&(self->vd), &(self->vi));
    vorbis_block_init(&(self->vd), &(self->vb));

    vorbis_analysis_headerout(&(self->vd), &(self->vc), &header, &header_comm, &header_code);
    ogg_stream_packetin(&(self->os), &header); /* automatically placed in its own page */
    ogg_stream_packetin(&(self->os), &header_comm);
    ogg_stream_packetin(&(self->os), &header_code);

    self->use_page_flush = 1;

    return 0;
}

static int __vorbis_stop_encoder(coolmic_enc_t *self)
{
    vorbis_block_clear(&(self->vb));
    vorbis_dsp_clear(&(self->vd));
    vorbis_comment_clear(&(self->vc));
    vorbis_info_clear(&(self->vi));

    memset(&(self->vb), 0, sizeof(self->vb));
    memset(&(self->vd), 0, sizeof(self->vd));
    memset(&(self->vc), 0, sizeof(self->vc));
    memset(&(self->vi), 0, sizeof(self->vi));
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

    if (self->state == STATE_EOF || self->state == STATE_NEED_RESET || self->state == STATE_NEED_RESTART) {
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

const coolmic_enc_cb_t __coolmic_enc_cb_vorbis = {
    .start = __vorbis_start_encoder,
    .stop = __vorbis_stop_encoder,
    .process = __vorbis_process
};
