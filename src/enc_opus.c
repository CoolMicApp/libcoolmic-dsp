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

static int __opus_start_encoder(coolmic_enc_t *self)
{
    int error;

    self->codec.opus.enc = opus_encoder_create(self->rate, self->channels, OPUS_APPLICATION_AUDIO, &error);
    if (!self->codec.opus.enc) {
        return libopuserror2error(error);
    }

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
    return COOLMIC_ERROR_NOSYS;
}

const coolmic_enc_cb_t __coolmic_enc_cb_opus = {
    .start = __opus_start_encoder,
    .stop = __opus_stop_encoder,
    .process = __opus_process
};
