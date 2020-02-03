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

/* This is a dummy sound driver. It supports record and playback.
 * In record mode it will read as zeros (silence).
 */

#include <stdio.h>
#include "types_private.h"
#include <coolmic-dsp/snddev.h>
#include <coolmic-dsp/coolmic-dsp.h>

static int __free(coolmic_snddev_driver_t *dev)
{
    if (fclose(dev->userdata_vp) != 0)
        return COOLMIC_ERROR_GENERIC;
    return COOLMIC_ERROR_NONE;
}

static ssize_t __read(coolmic_snddev_driver_t *dev, void *buffer, size_t len)
{
    return fread(buffer, 1, len, dev->userdata_vp);
}

static ssize_t __write(coolmic_snddev_driver_t *dev, const void *buffer, size_t len)
{
    return fwrite(buffer, 1, len, dev->userdata_vp);
}

int coolmic_snddev_driver_stdio_open(coolmic_snddev_driver_t *dev, const char *driver, void *device, uint_least32_t rate, unsigned int channels, int flags, ssize_t buffer)
{
    const char *mode = NULL;

    (void)driver, (void)rate, (void)channels, (void)buffer;

    if (!device || !*(const char*)device)
        return COOLMIC_ERROR_FAULT;

    dev->free = __free;
    dev->read = __read;
    dev->write = __write;

    if ((flags & COOLMIC_DSP_SNDDEV_RXTX) == COOLMIC_DSP_SNDDEV_RXTX) {
        mode = "w+b";
    } else if (flags & COOLMIC_DSP_SNDDEV_RX) {
        mode = "rb";
    } else if (flags & COOLMIC_DSP_SNDDEV_TX) {
        mode = "wb";
    } else {
        return COOLMIC_ERROR_INVAL;
    }

    dev->userdata_vp = fopen(device, mode);
    if (!dev->userdata_vp)
        return COOLMIC_ERROR_GENERIC;

    return COOLMIC_ERROR_NONE;
}
