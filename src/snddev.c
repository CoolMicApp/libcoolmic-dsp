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

#define COOLMIC_COMPONENT "libcoolmic-dsp/snddev"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "types_private.h"
#include <coolmic-dsp/snddev.h>
#include <coolmic-dsp/coolmic-dsp.h>
#include <coolmic-dsp/logging.h>

/* default driver */
#ifndef DEFAULT_SNDDRV_DRIVER
# ifdef HAVE_SNDDRV_DRIVER_OPENSL
#  define DEFAULT_SNDDRV_DRIVER COOLMIC_DSP_SNDDEV_DRIVER_OPENSL
# elif defined(HAVE_SNDDRV_DRIVER_OSS)
#  define DEFAULT_SNDDRV_DRIVER COOLMIC_DSP_SNDDEV_DRIVER_OSS
# else
#  define DEFAULT_SNDDRV_DRIVER COOLMIC_DSP_SNDDEV_DRIVER_NULL
# endif
#endif

/* forward decleration of drivers */
int coolmic_snddev_driver_null_open(coolmic_snddev_driver_t *dev, const char *driver, void *device, uint_least32_t rate, unsigned int channels, int flags, ssize_t buffer);
int coolmic_snddev_driver_sine_open(coolmic_snddev_driver_t *dev, const char *driver, void *device, uint_least32_t rate, unsigned int channels, int flags, ssize_t buffer);
#ifdef HAVE_SNDDRV_DRIVER_OSS
int coolmic_snddev_driver_oss_open(coolmic_snddev_driver_t *dev, const char *driver, void *device, uint_least32_t rate, unsigned int channels, int flags, ssize_t buffer);
#endif
#ifdef HAVE_SNDDRV_DRIVER_OPENSL
int coolmic_snddev_driver_opensl_open(coolmic_snddev_driver_t *dev, const char *driver, void *device, uint_least32_t rate, unsigned int channels, int flags, ssize_t buffer);
#endif
#ifdef HAVE_SNDDRV_DRIVER_STDIO
int coolmic_snddev_driver_stdio_open(coolmic_snddev_driver_t *dev, const char *driver, void *device, uint_least32_t rate, unsigned int channels, int flags, ssize_t buffer);
#endif

struct coolmic_snddev {
    /* base type */
    igloo_ro_base_t __base;

    /* driver */
    coolmic_snddev_driver_t driver;
    /* IO Handles */
    coolmic_iohandle_t *tx; /* Handle -data-> Device */
    /* Buffer for TX */
    char txbuffer[1024];
    size_t txbuffer_fill;
};

static void __free(igloo_ro_t self)
{
    coolmic_snddev_t *snddev = igloo_RO_TO_TYPE(self, coolmic_snddev_t);

    igloo_ro_unref(snddev->tx);

    if (snddev->driver.free)
        snddev->driver.free(&(snddev->driver));
}

igloo_RO_PUBLIC_TYPE(coolmic_snddev_t,
        igloo_RO_TYPEDECL_FREE(__free)
        );

static ssize_t __read(void *userdata, void *buffer, size_t len)
{
    coolmic_snddev_t *self = (coolmic_snddev_t*)userdata;

    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Read request, buffer=%p, len=%zu", buffer, len);

    if (!self->driver.read)
        return COOLMIC_ERROR_NOSYS;
    return self->driver.read(&(self->driver), buffer, len);
}

coolmic_snddev_t   *coolmic_snddev_new(const char *name, igloo_ro_t associated, const char *driver, void *device, uint_least32_t rate, unsigned int channels, int flags, ssize_t buffer)
{
    coolmic_snddev_t *ret;
    int (*driver_open)(coolmic_snddev_driver_t*, const char*, void*, uint_least32_t, unsigned int, int, ssize_t) = NULL;

    /* check arguments */
    if (!rate || !channels || !flags)
        return NULL;

    if (!driver)
        driver = DEFAULT_SNDDRV_DRIVER;

    if (strcasecmp(driver, COOLMIC_DSP_SNDDEV_DRIVER_NULL) == 0) {
        driver_open = coolmic_snddev_driver_null_open;
    } else if (strcasecmp(driver, COOLMIC_DSP_SNDDEV_DRIVER_SINE) == 0) {
        driver_open = coolmic_snddev_driver_sine_open;
#ifdef HAVE_SNDDRV_DRIVER_OSS
    } else if (strcasecmp(driver, COOLMIC_DSP_SNDDEV_DRIVER_OSS) == 0) {
        driver_open = coolmic_snddev_driver_oss_open;
#endif
#ifdef HAVE_SNDDRV_DRIVER_OPENSL
    } else if (strcasecmp(driver, COOLMIC_DSP_SNDDEV_DRIVER_OPENSL) == 0) {
        driver_open = coolmic_snddev_driver_opensl_open;
#endif
#ifdef HAVE_SNDDRV_DRIVER_STDIO
    } else if (strcasecmp(driver, COOLMIC_DSP_SNDDEV_DRIVER_STDIO) == 0) {
        driver_open = coolmic_snddev_driver_stdio_open;
#endif
    } else {
        /* unknown driver */
        return NULL;
    }

    ret = igloo_ro_new_raw(coolmic_snddev_t, name, associated);
    if (!ret)
        return NULL;

    if (driver_open(&(ret->driver), driver, device, rate, channels, flags, buffer) != 0) {
        igloo_ro_unref(ret);
        return NULL;
    }

    return ret;
}

int                 coolmic_snddev_attach_iohandle(coolmic_snddev_t *self, coolmic_iohandle_t *handle)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    if (self->tx)
        igloo_ro_unref(self->tx);
    /* ignore errors here as handle is allowed to be NULL */
    igloo_ro_ref(self->tx = handle);
    return COOLMIC_ERROR_NONE;
}

static int __free_snddev_iohandle(void *arg)
{
    coolmic_snddev_t *snddev = arg;
    igloo_ro_unref(snddev);
    return 0;
}

coolmic_iohandle_t *coolmic_snddev_get_iohandle(coolmic_snddev_t *self)
{
    if (!self)
        return NULL;
    //if (flags & COOLMIC_DSP_SNDDEV_RX) {
    //
    igloo_ro_ref(self);
    return coolmic_iohandle_new(NULL, igloo_RO_NULL, self, __free_snddev_iohandle, __read, NULL);
}

static inline int __flush_buffer(coolmic_snddev_t *self)
{
    ssize_t ret;

    if (!self->txbuffer_fill)
        return COOLMIC_ERROR_NONE;

    ret = self->driver.write(&(self->driver), self->txbuffer, self->txbuffer_fill);
    if (ret < 0) {
        return COOLMIC_ERROR_GENERIC;
    } else if (ret == 0) {
        return COOLMIC_ERROR_BUSY;
    } else if ((size_t)ret == self->txbuffer_fill) {
        self->txbuffer_fill = 0;
    } else {
        memmove(self->txbuffer, self->txbuffer + ret, self->txbuffer_fill - ret);
        self->txbuffer_fill -= ret;
        return COOLMIC_ERROR_BUSY;
    }

    return COOLMIC_ERROR_NONE;
}

int                 coolmic_snddev_iter(coolmic_snddev_t *self)
{
    int ret;

    if (!self->driver.write)
        return COOLMIC_ERROR_NOSYS;

    ret = __flush_buffer(self);
    if (ret != COOLMIC_ERROR_NONE)
        return ret;

    ret = coolmic_iohandle_read(self->tx, self->txbuffer, sizeof(self->txbuffer));
    if (ret < 0) {
        return COOLMIC_ERROR_GENERIC;
    } else if (ret == 0) {
        return COOLMIC_ERROR_NONE;
    }

    self->txbuffer_fill = ret;

    return __flush_buffer(self);
}
