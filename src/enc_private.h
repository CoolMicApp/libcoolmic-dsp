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

/*
 * This file defines the private API for the encoder part of this library.
 */

#ifndef __COOLMIC_DSP_ENC_PRIVATE_H__
#define __COOLMIC_DSP_ENC_PRIVATE_H__

#include <stdint.h>
#include <vorbis/vorbisenc.h>
#include <coolmic-dsp/iohandle.h>
#include <coolmic-dsp/metadata.h>
#ifdef HAVE_ENC_OPUS
#include <opus/opus.h>
#endif

typedef enum coolmic_enc_state {
    STATE_NEED_INIT = 0,
    STATE_RUNNING,
    STATE_EOF,
    STATE_NEED_RESET,
    STATE_NEED_RESTART
} coolmic_enc_state_t;

typedef struct coolmic_enc_cb {
    /* Called to set up the codec specific internals.
     * Returns a COOLMIC_ERROR_*.
     */
    int (*start)(coolmic_enc_t *self);
    /* Called to free the codec specific internals.
     * Returns a COOLMIC_ERROR_*.
     */
    int (*stop)(coolmic_enc_t *self);
    /* Called when more data is needed from the codec.
     * Returns: 0 on success, -1 on error and -2 on recoverable error.
     */
    int (*process)(coolmic_enc_t *self);
} coolmic_enc_cb_t;

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

    /* Callbacks: */
    coolmic_enc_cb_t cb;

    /* Codec private data: */
    union {
        /* Vorbis: */
        struct {
            vorbis_info      vi; /* struct that stores all the static vorbis bitstream
                                    settings */
            vorbis_comment   vc; /* struct that stores all the user comments */

            vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
            vorbis_block     vb; /* local working space for packet->PCM decode */
        } vorbis;
#ifdef HAVE_ENC_OPUS
        /* Opus: */
        struct {
            OpusEncoder   *enc;
        } opus;
#endif
    } codec;

    float quality;       /* quality level, -0.1 to 1.0 */

    coolmic_metadata_t *metadata;
};

const coolmic_enc_cb_t __coolmic_enc_cb_vorbis;
const coolmic_enc_cb_t __coolmic_enc_cb_opus;

#endif
