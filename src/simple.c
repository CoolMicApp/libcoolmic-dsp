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
#include <coolmic-dsp/enc.h>
#include <coolmic-dsp/shout.h>

struct coolmic_simple {
    size_t refc;
    pthread_mutex_t lock;
    pthread_t thread;
    int running;
    int need_reset;

    coolmic_simple_callback_t callback;
    void *callback_userdata;

    coolmic_snddev_t *dev;
    coolmic_enc_t *enc;
    coolmic_shout_t *shout;
    coolmic_iohandle_t *pcm;
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

static void __stop_unlocked(coolmic_simple_t *self)
{
    if (!self->running)
        return;
    self->running = 2;
    __emit_event(self, COOLMIC_SIMPLE_EVENT_THREAD_STOP, &(self->thread), NULL, NULL);
    pthread_mutex_unlock(&(self->lock));
    pthread_join(self->thread, NULL);
    pthread_mutex_lock(&(self->lock));
}

coolmic_simple_t   *coolmic_simple_new(const char *codec, uint_least32_t rate, unsigned int channels, ssize_t buffer, const coolmic_shout_config_t *conf)
{
    coolmic_simple_t *ret = calloc(1, sizeof(coolmic_simple_t));
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
        if ((ret->pcm = coolmic_snddev_get_iohandle(ret->dev)) == NULL)
            break;
        if ((ret->ogg = coolmic_enc_get_iohandle(ret->enc)) == NULL)
            break;
        if (coolmic_enc_attach_iohandle(ret->enc, ret->pcm) != 0)
            break;
        if (coolmic_shout_attach_iohandle(ret->shout, ret->ogg) != 0)
            break;
        if (coolmic_shout_set_config(ret->shout, conf) != 0)
            break;
        return ret;
    } while (0);

    coolmic_simple_unref(ret);
    return NULL;
}

int                 coolmic_simple_ref(coolmic_simple_t *self)
{
    if (!self)
        return -1;
    pthread_mutex_lock(&(self->lock));
    self->refc++;
    pthread_mutex_unlock(&(self->lock));
    return 0;
}

int                 coolmic_simple_unref(coolmic_simple_t *self)
{
    if (!self)
        return -1;

    pthread_mutex_lock(&(self->lock));
    self->refc--;

    if (self->refc) {
        pthread_mutex_unlock(&(self->lock));
        return 0;
    }

    __stop_unlocked(self);

    coolmic_iohandle_unref(self->pcm);
    coolmic_iohandle_unref(self->ogg);
    coolmic_shout_unref(self->shout);
    coolmic_enc_unref(self->enc);
    coolmic_snddev_unref(self->dev);

    pthread_mutex_unlock(&(self->lock));
    pthread_mutex_destroy(&(self->lock));
    free(self);

    return 0;
}

/* reset internal objects */
static inline int __reset(coolmic_simple_t *self)
{
    coolmic_enc_reset(self->enc);
    self->need_reset = 0;
    return 0;
}

/* worker */
static void *__worker(void *userdata)
{
    coolmic_simple_t *self = userdata;
    int running;
    coolmic_shout_t *shout;

    pthread_mutex_lock(&(self->lock));
    __emit_event(self, COOLMIC_SIMPLE_EVENT_THREAD_POST_START, &(self->thread), NULL, NULL);
    if (self->need_reset) {
        if (__reset(self) != 0) {
            self->running = 0;
            pthread_mutex_unlock(&(self->lock));
            return NULL;
        }
    }

    running = self->running;
    coolmic_shout_ref(shout = self->shout);
    coolmic_shout_start(shout);
    pthread_mutex_unlock(&(self->lock));

    while (running == 1) {
        if (coolmic_shout_iter(shout) != 0)
            break;

        pthread_mutex_lock(&(self->lock));
        if (self->need_reset)
            if (__reset(self) != 0)
                self->running = 0;
        running = self->running;
        pthread_mutex_unlock(&(self->lock));
    }

    pthread_mutex_lock(&(self->lock));
    if (self->running != 2)
        __emit_event(self, COOLMIC_SIMPLE_EVENT_ERROR, &(self->thread), NULL, NULL);
    self->running = 0;
    self->need_reset = 1;
    coolmic_shout_stop(shout);
    coolmic_shout_unref(shout);
    __emit_event(self, COOLMIC_SIMPLE_EVENT_THREAD_PRE_STOP, &(self->thread), NULL, NULL);
    pthread_mutex_unlock(&(self->lock));
    return NULL;
}

/* thread control functions */
int                 coolmic_simple_start(coolmic_simple_t *self)
{
    int running;

    if (!self)
        return -1;
    pthread_mutex_lock(&(self->lock));
    if (!self->running)
        if (pthread_create(&(self->thread), NULL, __worker, self) == 0) {
            self->running = 1;
            __emit_event(self, COOLMIC_SIMPLE_EVENT_THREAD_START, &(self->thread), NULL, NULL);
        }
    running = self->running;
    pthread_mutex_unlock(&(self->lock));
    return running ? 0 : -1;
}

int                 coolmic_simple_stop(coolmic_simple_t *self)
{
    if (!self)
        return -1;
    pthread_mutex_lock(&(self->lock));
    if (self->running)
        __stop_unlocked(self);
    pthread_mutex_unlock(&(self->lock));
    return 0;
}

int                 coolmic_simple_set_callback(coolmic_simple_t *self, coolmic_simple_callback_t callback, void *userdata)
{
    if (!self)
        return -1;
    pthread_mutex_lock(&(self->lock));
    self->callback = callback;
    self->callback_userdata = userdata;
    pthread_mutex_unlock(&(self->lock));
    return 0;
}
