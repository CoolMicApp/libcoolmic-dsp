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

#include <stdlib.h>
#include "types_private.h"
#include <coolmic-dsp/iohandle.h>
#include <coolmic-dsp/coolmic-dsp.h>

struct coolmic_iohandle {
    /* base type */
    igloo_ro_base_t __base;

    void    *userdata;
    int     (*free)(void *userdata);
    ssize_t (*read)(void *userdata, void *buffer, size_t len);
    int     (*eof )(void *userdata);
};

static void __free(igloo_ro_t self)
{
    coolmic_iohandle_t *iohandle = igloo_RO_TO_TYPE(self, coolmic_iohandle_t);

    if (iohandle->free) {
        iohandle->free(iohandle->userdata);
    }
}

igloo_RO_PUBLIC_TYPE(coolmic_iohandle_t,
        igloo_RO_TYPEDECL_FREE(__free)
        );

coolmic_iohandle_t *coolmic_iohandle_new(const char *name, igloo_ro_t associated, void *userdata, int(*free)(void*), ssize_t(*read)(void*,void*,size_t), int(*eof)(void*))
{
    coolmic_iohandle_t *ret;

    /* we should at least have a read function for this to make sense */
    if (!read)
        return NULL;

    ret = igloo_ro_new_raw(coolmic_iohandle_t, name, associated);
    if (!ret)
        return NULL;

    ret->userdata = userdata;
    ret->free = free;
    ret->read = read;
    ret->eof = eof;

    return ret;
}

ssize_t             coolmic_iohandle_read(coolmic_iohandle_t *self, void *buffer, size_t len)
{
    ssize_t done = 0;
    ssize_t ret;

    if (!self || !buffer)
        return COOLMIC_ERROR_FAULT;
    if (!len)
        return COOLMIC_ERROR_NONE;
    if (!self->read)
        return COOLMIC_ERROR_NOSYS;

    while (len) {
        ret = self->read(self->userdata, buffer, len);
        if (ret < 0) {
            if (done) {
                return done;
            } else {
                return ret;
            }
        } else if (ret == 0) {
            return done;
        }

        buffer += ret;
        len    -= ret;
        done   += ret;
    }

    return done;
}

int                 coolmic_iohandle_eof(coolmic_iohandle_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    if (self->eof)
        return self->eof(self->userdata);
    return 0; /* bool */
}
