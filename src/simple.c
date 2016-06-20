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

#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <coolmic-dsp/simple.h>
#include <coolmic-dsp/iohandle.h>
#include <coolmic-dsp/snddev.h>
#include <coolmic-dsp/tee.h>
#include <coolmic-dsp/enc.h>
#include <coolmic-dsp/shout.h>
#include <coolmic-dsp/vumeter.h>
#include <coolmic-dsp/coolmic-dsp.h>

struct coolmic_simple {
    size_t refc;
    pthread_mutex_t lock;
    pthread_t thread;
    int running;
    int need_reset;

    coolmic_simple_callback_t callback;
    void *callback_userdata;

    coolmic_snddev_t *dev;
    coolmic_tee_t *tee;
    coolmic_enc_t *enc;
    coolmic_shout_t *shout;
    coolmic_vumeter_t *vumeter;
    coolmic_iohandle_t *ogg;
};

/* emit an event */
static inline void __emit_event(coolmic_simple_t *self, coolmic_simple_event_t event, void *thread, void *arg0, void *arg1)
{
    coolmic_simple_callback_t callback;
    void *callback_userdata;
    
    if (!self->callback)
        return;

    callback = self->callback;
    callback_userdata = self->callback_userdata;
    /* the callback is called in unlocked state. */
    pthread_mutex_unlock(&(self->lock));
    callback(self, callback_userdata, event, thread, arg0, arg1);
    pthread_mutex_lock(&(self->lock));
}

static inline void __emit_error_unlocked(coolmic_simple_t *self, void *thread, int error) {
    coolmic_simple_callback_t callback;
    void *callback_userdata;

    pthread_mutex_lock(&(self->lock));
    if (!self->callback)
        return;

    callback = self->callback;
    callback_userdata = self->callback_userdata;
    pthread_mutex_unlock(&(self->lock));
    /* the callback is called in unlocked state. */
    callback(self, callback_userdata, COOLMIC_SIMPLE_EVENT_ERROR, thread, &error, NULL);
}

static void __stop_unlocked(coolmic_simple_t *self)
{
    if (!self->running)
        return;
    self->running = 2;
    __emit_event(self, COOLMIC_SIMPLE_EVENT_THREAD_STOP, NULL, &(self->thread), NULL);
    pthread_mutex_unlock(&(self->lock));
    pthread_join(self->thread, NULL);
    pthread_mutex_lock(&(self->lock));
}

coolmic_simple_t   *coolmic_simple_new(const char *codec, uint_least32_t rate, unsigned int channels, ssize_t buffer, const coolmic_shout_config_t *conf)
{
    coolmic_simple_t *ret = calloc(1, sizeof(coolmic_simple_t));
    coolmic_iohandle_t *handle;

    if (!ret)
        return NULL;

    ret->refc = 1;
    pthread_mutex_init(&(ret->lock), NULL);

    do {
        if ((ret->dev = coolmic_snddev_new(COOLMIC_DSP_SNDDEV_DRIVER_AUTO, NULL, rate, channels, COOLMIC_DSP_SNDDEV_RX, buffer)) == NULL)
            break;
        if ((ret->enc = coolmic_enc_new(codec, rate, channels)) == NULL)
            break;
        if ((ret->shout = coolmic_shout_new()) == NULL)
            break;
        if ((ret->tee = coolmic_tee_new(2)) == NULL)
            break;
        if ((ret->vumeter = coolmic_vumeter_new(rate, channels)) == NULL)
            break;
        if ((ret->ogg = coolmic_enc_get_iohandle(ret->enc)) == NULL)
            break;
        if ((handle = coolmic_snddev_get_iohandle(ret->dev)) == NULL)
            break;
        if (coolmic_tee_attach_iohandle(ret->tee, handle) != 0)
            break;
        coolmic_iohandle_unref(handle);
        if ((handle = coolmic_tee_get_iohandle(ret->tee, 0)) == NULL)
            break;
        if (coolmic_enc_attach_iohandle(ret->enc, handle) != 0)
            break;
        coolmic_iohandle_unref(handle);
        if (coolmic_shout_attach_iohandle(ret->shout, ret->ogg) != 0)
            break;
        if (coolmic_shout_set_config(ret->shout, conf) != 0)
            break;
        if ((handle = coolmic_tee_get_iohandle(ret->tee, 1)) == NULL)
            break;
        if (coolmic_vumeter_attach_iohandle(ret->vumeter, handle) != 0)
            break;
        coolmic_iohandle_unref(handle);
        return ret;
    } while (0);

    coolmic_simple_unref(ret);
    return NULL;
}

int                 coolmic_simple_ref(coolmic_simple_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    pthread_mutex_lock(&(self->lock));
    self->refc++;
    pthread_mutex_unlock(&(self->lock));
    return COOLMIC_ERROR_NONE;
}

int                 coolmic_simple_unref(coolmic_simple_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;

    pthread_mutex_lock(&(self->lock));
    self->refc--;

    if (self->refc) {
        pthread_mutex_unlock(&(self->lock));
        return COOLMIC_ERROR_NONE;
    }

    __stop_unlocked(self);

    coolmic_iohandle_unref(self->ogg);
    coolmic_shout_unref(self->shout);
    coolmic_enc_unref(self->enc);
    coolmic_snddev_unref(self->dev);

    pthread_mutex_unlock(&(self->lock));
    pthread_mutex_destroy(&(self->lock));
    free(self);

    return COOLMIC_ERROR_NONE;
}

/* reset internal objects */
static inline int __reset(coolmic_simple_t *self)
{
    coolmic_enc_reset(self->enc);
    self->need_reset = 0;
    return COOLMIC_ERROR_NONE;
}

/* worker */
static void *__worker(void *userdata)
{
    coolmic_simple_t *self = userdata;
    int running;
    coolmic_shout_t *shout;
    coolmic_vumeter_t *vumeter;
    size_t vumeter_iter = 1;
    size_t vumeter_interval = 4;
    ssize_t ret;
    coolmic_vumeter_result_t vumeter_result;
    int error;

    pthread_mutex_lock(&(self->lock));
    __emit_event(self, COOLMIC_SIMPLE_EVENT_THREAD_POST_START, &(self->thread), NULL, NULL);
    if (self->need_reset) {
        if (__reset(self) != 0) {
            self->running = 0;
            __emit_event(self, COOLMIC_SIMPLE_EVENT_THREAD_PRE_STOP, &(self->thread), NULL, NULL);
            pthread_mutex_unlock(&(self->lock));
            return NULL;
        }
    }

    running = self->running;
    coolmic_shout_ref(shout = self->shout);
    coolmic_vumeter_ref(vumeter = self->vumeter);
    pthread_mutex_unlock(&(self->lock));

    if ((error = coolmic_shout_start(shout)) != COOLMIC_ERROR_NONE) {
        running = 0;
        __emit_error_unlocked(self, &(self->thread), error);
    }

    while (running == 1) {
        if ((error = coolmic_shout_iter(shout)) != COOLMIC_ERROR_NONE) {
            __emit_error_unlocked(self, &(self->thread), error);
            break;
        }
        ret = coolmic_vumeter_read(vumeter, -1);
        if (ret < 0) {
            __emit_error_unlocked(self, &(self->thread), COOLMIC_ERROR_GENERIC);
            break;
        } else if (ret > 0) {
            vumeter_iter++;
        }

        if (vumeter_iter == vumeter_interval) {
            vumeter_iter = 0;
            if (coolmic_vumeter_result(vumeter, &vumeter_result) == 0) {
                pthread_mutex_lock(&(self->lock));
                __emit_event(self, COOLMIC_SIMPLE_EVENT_VUMETER_RESULT, &(self->thread), &vumeter_result, NULL);
                pthread_mutex_unlock(&(self->lock));
            }
        }

        pthread_mutex_lock(&(self->lock));
        if (self->need_reset)
            if (__reset(self) != 0)
                self->running = 0;
        running = self->running;
        pthread_mutex_unlock(&(self->lock));
    }

    pthread_mutex_lock(&(self->lock));
    self->running = 0;
    self->need_reset = 1;
    coolmic_shout_stop(shout);
    coolmic_shout_unref(shout);
    coolmic_vumeter_unref(vumeter);
    __emit_event(self, COOLMIC_SIMPLE_EVENT_THREAD_PRE_STOP, &(self->thread), NULL, NULL);
    pthread_mutex_unlock(&(self->lock));
    return NULL;
}

/* thread control functions */
int                 coolmic_simple_start(coolmic_simple_t *self)
{
    int running;

    if (!self)
        return COOLMIC_ERROR_FAULT;
    pthread_mutex_lock(&(self->lock));
    if (!self->running)
        if (pthread_create(&(self->thread), NULL, __worker, self) == 0) {
            self->running = 1;
            __emit_event(self, COOLMIC_SIMPLE_EVENT_THREAD_START, NULL, &(self->thread), NULL);
        }
    running = self->running;
    pthread_mutex_unlock(&(self->lock));
    return running ? COOLMIC_ERROR_NONE : COOLMIC_ERROR_GENERIC;
}

int                 coolmic_simple_stop(coolmic_simple_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    pthread_mutex_lock(&(self->lock));
    if (self->running)
        __stop_unlocked(self);
    pthread_mutex_unlock(&(self->lock));
    return COOLMIC_ERROR_NONE;
}

int                 coolmic_simple_set_callback(coolmic_simple_t *self, coolmic_simple_callback_t callback, void *userdata)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    pthread_mutex_lock(&(self->lock));
    self->callback = callback;
    self->callback_userdata = userdata;
    pthread_mutex_unlock(&(self->lock));
    return COOLMIC_ERROR_NONE;
}
