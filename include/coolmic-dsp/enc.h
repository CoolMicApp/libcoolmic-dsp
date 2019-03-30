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
 * This file defines the API for the encoder part of this library.
 *
 * The encoder works by setting it up, attaching a IO handle
 * for the backend and then take the IO handle for the front end
 * and read data off it.
 */

#ifndef __COOLMIC_DSP_ENC_H__
#define __COOLMIC_DSP_ENC_H__

#include <stdint.h>
#include "iohandle.h"

/* forward declare internally used structures */
typedef struct coolmic_enc coolmic_enc_t;

#define COOLMIC_ENC_OPCODE(base,type) ((base)*4+(type))
#define COOLMIC_ENC_OPCODE_DO(base) COOLMIC_ENC_OPCODE((base), 0)
#define COOLMIC_ENC_OPCODE_GET(base) COOLMIC_ENC_OPCODE((base), 1)
#define COOLMIC_ENC_OPCODE_SET(base) COOLMIC_ENC_OPCODE((base), 2)

/* request codes for control function */
typedef enum coolmic_enc_op {
    /* invalid opcode */
    COOLMIC_ENC_OP_INVALID    = -1,
    /* no-op opcode */
    COOLMIC_ENC_OP_NONE       =  0,

    /* Object manipulation: 1-63 */
    COOLMIC_ENC_OP_RESET      = COOLMIC_ENC_OPCODE_DO(1),
    COOLMIC_ENC_OP_RESTART    = COOLMIC_ENC_OPCODE_DO(2),
    COOLMIC_ENC_OP_STOP       = COOLMIC_ENC_OPCODE_DO(3),

    /* Codec parameters: 64-127 */

    /* get and set quality
     * Argument is (double) in range -0.1 to 1.0.
     */
    COOLMIC_ENC_OP_GET_QUALITY = COOLMIC_ENC_OPCODE_GET(64),
    COOLMIC_ENC_OP_SET_QUALITY = COOLMIC_ENC_OPCODE_SET(64),

    /* Meta data: 128-191 */

    /* get and set metadata object
     * Argument is (coolmic_metadata_t*).
     */
    COOLMIC_ENC_OP_GET_METADATA = COOLMIC_ENC_OPCODE_GET(128),
    COOLMIC_ENC_OP_SET_METADATA = COOLMIC_ENC_OPCODE_SET(128)
} coolmic_enc_op_t;

/* Management of the encoder object */
coolmic_enc_t      *coolmic_enc_new(const char *name, igloo_ro_t associated, const char *codec, uint_least32_t rate, unsigned int channels);

/* Reset the encoder state */
int                 coolmic_enc_reset(coolmic_enc_t *self);
/* control the encoder */
int                 coolmic_enc_ctl(coolmic_enc_t *self, coolmic_enc_op_t op, ...);

/* This is to attach the IO Handle of the PCM data stream that is to be passed to the encoder */
int                 coolmic_enc_attach_iohandle(coolmic_enc_t *self, coolmic_iohandle_t *handle);

/* This function is to get the IO Handle to read the encoded data from */
coolmic_iohandle_t *coolmic_enc_get_iohandle(coolmic_enc_t *self);

#endif
