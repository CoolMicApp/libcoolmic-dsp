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

#define COOLMIC_COMPONENT "libcoolmic-dsp/simple"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include "types_private.h"
#include <coolmic-dsp/simple.h>
#include <coolmic-dsp/iohandle.h>
#include <coolmic-dsp/snddev.h>
#include <coolmic-dsp/tee.h>
#include <coolmic-dsp/enc.h>
#include <coolmic-dsp/shout.h>
#include <coolmic-dsp/vumeter.h>
#include <coolmic-dsp/metadata.h>
#include <coolmic-dsp/transform.h>
#include <coolmic-dsp/coolmic-dsp.h>
#include <coolmic-dsp/logging.h>

#define RECON_PROFILE_DEFAULT "disabled"
#define RECON_PROFILE_ENABLED "flat"

enum coolmic_simple_running {
    RUNNING_STOPPED = 0,
    RUNNING_STARTED = 1,
    RUNNING_STOPPING = 2,
    RUNNING_LOST,
    RUNNING_ERROR
};

struct coolmic_simple {
    /* base type */
    igloo_ro_base_t __base;

    pthread_mutex_t lock;
    pthread_t thread;
    enum coolmic_simple_running running;
    int need_reset;
    int thread_needs_join;

    coolmic_simple_callback_t callback;
    void *callback_userdata;

    size_t vumeter_interval;

    /* Reconnection profile */
    char *reconnection_profile;

    /* Next segment to play. That is a filename or NULL for live. */
    char *next_segment;
    coolmic_simple_segment_t *current_segment;
    igloo_list_t *segment_list;

    char *codec;
    uint_least32_t rate;
    unsigned int channels;
    ssize_t buffer;

    coolmic_snddev_t *dev;
    coolmic_tee_t *tee;
    coolmic_enc_t *enc;
    coolmic_shout_t *shout;
    coolmic_vumeter_t *vumeter;
    coolmic_iohandle_t *ogg;
    coolmic_metadata_t *metadata;
    coolmic_transform_t *transform;
};

/* emit an event */
static inline void __emit_event(coolmic_simple_t *self, coolmic_simple_event_t event, void *thread, void *arg0, void *arg1, int locked)
{
    coolmic_simple_callback_t callback;
    void *callback_userdata;
    
    if (!locked)
        pthread_mutex_lock(&(self->lock));

    if (!self->callback) {
        if (!locked)
            pthread_mutex_unlock(&(self->lock));
        return;
    }

    callback = self->callback;
    callback_userdata = self->callback_userdata;
    /* the callback is called in unlocked state. */
    pthread_mutex_unlock(&(self->lock));
    callback(self, callback_userdata, event, thread, arg0, arg1);

    if (locked)
        pthread_mutex_lock(&(self->lock));
}

static inline void __emit_event_locked(coolmic_simple_t *self, coolmic_simple_event_t event, void *thread, void *arg0, void *arg1)
{
    __emit_event(self, event, thread, arg0, arg1, 1);
}

static inline void __emit_event_unlocked(coolmic_simple_t *self, coolmic_simple_event_t event, void *thread, void *arg0, void *arg1)
{
    __emit_event(self, event, thread, arg0, arg1, 0);
}

static inline void __emit_cs_locked(coolmic_simple_t *self, void *thread, coolmic_simple_connectionstate_t cs, int error)
{
    __emit_event(self, COOLMIC_SIMPLE_EVENT_STREAMSTATE, thread, &cs, &error, 1);
}

static inline void __emit_cs_unlocked(coolmic_simple_t *self, void *thread, coolmic_simple_connectionstate_t cs, int error)
{
    __emit_event(self, COOLMIC_SIMPLE_EVENT_STREAMSTATE, thread, &cs, &error, 0);
}

static inline void __emit_error_locked(coolmic_simple_t *self, void *thread, int error)
{
    __emit_event(self, COOLMIC_SIMPLE_EVENT_ERROR, thread, &error, NULL, 1);
}
static inline void __emit_error_unlocked(coolmic_simple_t *self, void *thread, int error)
{
    __emit_event(self, COOLMIC_SIMPLE_EVENT_ERROR, thread, &error, NULL, 0);
}

static int __segment_disconnect(coolmic_simple_t *self)
{
    coolmic_shout_attach_iohandle(self->shout, NULL);
    coolmic_vumeter_attach_iohandle(self->vumeter, NULL);
    coolmic_enc_attach_iohandle(self->enc, NULL);
    coolmic_transform_attach_iohandle(self->transform, NULL);
    coolmic_tee_attach_iohandle(self->tee, NULL);

    igloo_ro_unref(self->ogg);
    igloo_ro_unref(self->enc);
    igloo_ro_unref(self->dev);
    igloo_ro_unref(self->transform);
    igloo_ro_unref(self->tee);
    igloo_ro_unref(self->vumeter);

    self->ogg = NULL;
    self->enc = NULL;
    self->dev = NULL;
    self->tee = NULL;
    self->vumeter = NULL;
    self->transform = NULL;

    return 0;
}

static int __segment_connect_live(coolmic_simple_t *self) {
    coolmic_iohandle_t *handle;

    do {
        if ((self->dev = coolmic_snddev_new(NULL, igloo_RO_NULL, COOLMIC_DSP_SNDDEV_DRIVER_AUTO, NULL, self->rate, self->channels, COOLMIC_DSP_SNDDEV_RX, self->buffer)) == NULL)
            break;
        if ((self->enc = coolmic_enc_new(NULL, igloo_RO_NULL, self->codec, self->rate, self->channels)) == NULL)
            break;
        if (coolmic_enc_ctl(self->enc, COOLMIC_ENC_OP_SET_METADATA, self->metadata) != 0)
            break;
        if ((self->tee = coolmic_tee_new(NULL, igloo_RO_NULL, 2)) == NULL)
            break;
        if ((self->vumeter = coolmic_vumeter_new(NULL, igloo_RO_NULL, self->rate, self->channels)) == NULL)
            break;
        if ((self->transform = coolmic_transform_new(NULL, igloo_RO_NULL, self->rate, self->channels)) == NULL)
            break;
        if ((self->ogg = coolmic_enc_get_iohandle(self->enc)) == NULL)
            break;
        if ((handle = coolmic_snddev_get_iohandle(self->dev)) == NULL)
            break;
        if (coolmic_transform_attach_iohandle(self->transform, handle) != 0)
            break;
        igloo_ro_unref(handle);
        if ((handle = coolmic_transform_get_iohandle(self->transform)) == NULL)
            break;
        if (coolmic_tee_attach_iohandle(self->tee, handle) != 0)
            break;
        igloo_ro_unref(handle);
        if ((handle = coolmic_tee_get_iohandle(self->tee, 0)) == NULL)
            break;
        if (coolmic_enc_attach_iohandle(self->enc, handle) != 0)
            break;
        igloo_ro_unref(handle);
        if ((handle = coolmic_tee_get_iohandle(self->tee, 1)) == NULL)
            break;
        if (coolmic_vumeter_attach_iohandle(self->vumeter, handle) != 0)
            break;
        igloo_ro_unref(handle);
        if (coolmic_shout_attach_iohandle(self->shout, self->ogg) != 0)
            break;
        return 0;
    } while (0);

    return -1;
}

static int __segment_connect_file(coolmic_simple_t *self, char *file) {
    self->enc = NULL;
    self->tee = NULL;
    self->vumeter = NULL;
    self->transform = NULL;

    do {
        if ((self->dev = coolmic_snddev_new(NULL, igloo_RO_NULL, COOLMIC_DSP_SNDDEV_DRIVER_STDIO, file, self->rate, self->channels, COOLMIC_DSP_SNDDEV_RX, self->buffer)) == NULL)
            break;
        if ((self->ogg = coolmic_snddev_get_iohandle(self->dev)) == NULL)
            break;
        if (coolmic_shout_attach_iohandle(self->shout, self->ogg) != 0)
            break;
        return 0;
    } while (0);

    return -1;
}

static int __segment_connect(coolmic_simple_t *self) {
    if (self->next_segment) {
        int ret = __segment_connect_file(self, self->next_segment);
        free(self->next_segment);
        self->next_segment = NULL;
        return ret;
    } else {
        return __segment_connect_live(self);
    }
}

static void __stop_locked(coolmic_simple_t *self)
{
    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Stopping worker thread requested. thread_needs_join=%i, running=%i", (int)self->thread_needs_join, (int)self->running);
    if (!(self->thread_needs_join || (self->running != RUNNING_STOPPING && self->running != RUNNING_STOPPED)))
        return;
    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Stopping worker thread.");
    self->running = RUNNING_STOPPING;
    __emit_event_locked(self, COOLMIC_SIMPLE_EVENT_THREAD_STOP, NULL, &(self->thread), NULL);
    pthread_mutex_unlock(&(self->lock));
    pthread_join(self->thread, NULL);
    pthread_mutex_lock(&(self->lock));
    self->thread_needs_join = 0;
}

static void __free(igloo_ro_t self)
{
    coolmic_simple_t *simple = igloo_RO_TO_TYPE(self, coolmic_simple_t);

    pthread_mutex_lock(&(simple->lock));
    __stop_locked(simple);
    __segment_disconnect(simple);
    igloo_ro_unref(simple->shout);

    igloo_ro_unref(simple->segment_list);

    free(simple->reconnection_profile);
    free(simple->codec);

    pthread_mutex_unlock(&(simple->lock));
    pthread_mutex_destroy(&(simple->lock));
}

igloo_RO_PUBLIC_TYPE(coolmic_simple_t,
        igloo_RO_TYPEDECL_FREE(__free)
        );

coolmic_simple_t   *coolmic_simple_new(const char *name, igloo_ro_t associated, const char *codec, uint_least32_t rate, unsigned int channels, ssize_t buffer, const coolmic_shout_config_t *conf)
{
    coolmic_simple_t *ret = igloo_ro_new_raw(coolmic_simple_t, name, associated);

    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Config: codec=%s, rate=%llu, channels=%u, buffer=%lli, conf=%p; ret=%p", codec, (long long unsigned int)rate, channels, (long long int)buffer, conf, ret);

    if (!ret)
        return NULL;

    pthread_mutex_init(&(ret->lock), NULL);

    ret->vumeter_interval = 20;
    ret->rate = rate;
    ret->channels = channels;
    ret->buffer = buffer;

    do {
        if ((ret->segment_list = igloo_ro_new(igloo_list_t)) == NULL)
            break;
        if (igloo_list_set_type(ret->segment_list, coolmic_simple_segment_t) != 0)
            break;
        if ((ret->codec = strdup(codec)) == NULL)
            break;
        if ((ret->shout = igloo_ro_new(coolmic_shout_t)) == NULL)
            break;
        if (coolmic_shout_set_config(ret->shout, conf) != 0)
            break;
        if ((ret->metadata = igloo_ro_new(coolmic_metadata_t)) == NULL)
            break;
        return ret;
    } while (0);

    igloo_ro_unref(ret);
    return NULL;
}

/* reset internal objects */
static inline int __reset(coolmic_simple_t *self)
{
    coolmic_enc_reset(self->enc);
    self->need_reset = 0;
    return COOLMIC_ERROR_NONE;
}

/* worker */
static inline void __worker_inner(coolmic_simple_t *self)
{
    enum coolmic_simple_running running;
    coolmic_shout_t *shout;
    coolmic_vumeter_t *vumeter;
    size_t vumeter_iter = 1;
    size_t vumeter_interval = 4;
    ssize_t ret;
    coolmic_vumeter_result_t vumeter_result;
    int error;

    if (self->need_reset) {
        if (__reset(self) != 0) {
            self->running = RUNNING_ERROR;
            return;
        }
    }

    running = self->running;
    igloo_ro_ref(shout = self->shout);
    igloo_ro_ref(vumeter = self->vumeter);
    pthread_mutex_unlock(&(self->lock));

    __emit_cs_unlocked(self, &(self->thread), COOLMIC_SIMPLE_CS_CONNECTING, COOLMIC_ERROR_NONE);
    if ((error = coolmic_shout_start(shout)) != COOLMIC_ERROR_NONE) {
        running = RUNNING_STOPPED;
        __emit_error_unlocked(self, &(self->thread), error);
        __emit_cs_unlocked(self, &(self->thread), COOLMIC_SIMPLE_CS_CONNECTIONERROR, error);
    } else {
        __emit_cs_unlocked(self, &(self->thread), COOLMIC_SIMPLE_CS_CONNECTED, COOLMIC_ERROR_NONE);
    }

    while (running == RUNNING_STARTED) {
        int need_next_segment;

        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Still running");

        if ((error = coolmic_shout_iter(shout)) != COOLMIC_ERROR_NONE) {
            __emit_error_unlocked(self, &(self->thread), error);
            __emit_cs_unlocked(self, &(self->thread), COOLMIC_SIMPLE_CS_CONNECTIONERROR, error);
            break;
        }

        if (coolmic_shout_need_next_segment(shout, &need_next_segment) != COOLMIC_ERROR_NONE) {
            need_next_segment = 0;
        }

        if (need_next_segment && self->enc) {
            if (!coolmic_iohandle_eof(self->ogg)) {
                need_next_segment = 0;
            } else {
            }
        }

        if (need_next_segment) {
            pthread_mutex_lock(&(self->lock));
            __segment_disconnect(self);
            __segment_connect(self);
            igloo_ro_unref(vumeter);
            igloo_ro_ref(vumeter = self->vumeter);
            pthread_mutex_unlock(&(self->lock));
        }

        if (vumeter) {
            ret = coolmic_vumeter_read(vumeter, -1);
            coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "VUmeter returned: %zi", ret);
            if (ret < 0) {
                __emit_error_unlocked(self, &(self->thread), COOLMIC_ERROR_GENERIC);
                break;
            } else if (ret > 0) {
                vumeter_iter++;
            }

            if (vumeter_interval && vumeter_iter == vumeter_interval) {
                vumeter_iter = 0;
                if (coolmic_vumeter_result(vumeter, &vumeter_result) == 0) {
                    __emit_event_unlocked(self, COOLMIC_SIMPLE_EVENT_VUMETER_RESULT, &(self->thread), &vumeter_result, NULL);
                }
            }
        }

        pthread_mutex_lock(&(self->lock));
        if (vumeter_interval != self->vumeter_interval) {
            vumeter_interval = self->vumeter_interval;
            if (vumeter_interval)
                vumeter_iter = vumeter_interval - 1;
        }
        if (self->need_reset)
            if (__reset(self) != 0)
                self->running = RUNNING_ERROR;
        running = self->running;
        pthread_mutex_unlock(&(self->lock));
    }

    pthread_mutex_lock(&(self->lock));
    if (self->running != RUNNING_STOPPING)
        self->running = RUNNING_LOST;
    self->need_reset = 1;
    __emit_cs_locked(self, &(self->thread), COOLMIC_SIMPLE_CS_DISCONNECTING, COOLMIC_ERROR_NONE);
    coolmic_shout_stop(shout);
    __emit_cs_locked(self, &(self->thread), COOLMIC_SIMPLE_CS_DISCONNECTED, COOLMIC_ERROR_NONE);
    igloo_ro_unref(shout);
    igloo_ro_unref(vumeter);
    return;
}

static inline struct timespec __min_ts (struct timespec a, struct timespec b)
{
    if (a.tv_sec <= b.tv_sec) {
        if (a.tv_nsec <= b.tv_nsec) {
            return a;
        } else {
            return b;
        }
    } else {
        return b;
    }
}

static inline struct timespec __sub_ts (struct timespec a, struct timespec b)
{
    if (b.tv_nsec > a.tv_nsec) {
        a.tv_sec--;
        a.tv_nsec += 1000000000;
    }

    a.tv_nsec -= b.tv_nsec;
    a.tv_sec  -= b.tv_sec;
    return a;
}

static inline int __isnonzero_ts (struct timespec a)
{
    return a.tv_sec || a.tv_nsec;
}

static void __worker_sleep(coolmic_simple_t *self)
{
    struct timespec to_sleep, req, rem;
    int ret;
    const struct timespec max_sleep = {
        .tv_sec = 0,
        .tv_nsec = 250000000
    };

    if (!self->reconnection_profile)
        return;

    memset(&to_sleep, 0, sizeof(to_sleep));

    if (!strcmp(self->reconnection_profile, "flat")) {
        to_sleep.tv_sec = 10;
    } else {
        /* TODO: FIXME: implement error handling here */
        self->running = RUNNING_STOPPED;
        return;
    }

    __emit_event_locked(self, COOLMIC_SIMPLE_EVENT_RECONNECT, &(self->thread), &to_sleep, NULL);
    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Entering reconnect sleep loop.");
    while (self->running != RUNNING_STOPPING && __isnonzero_ts(to_sleep)) {
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Sill need sleep before reconnect");
        req = __min_ts(to_sleep, max_sleep);
        pthread_mutex_unlock(&(self->lock));
        ret = nanosleep(&req, &rem);
        pthread_mutex_lock(&(self->lock));

        if (ret == -1 && errno == EINTR) {
            req = __sub_ts(req, rem);
        }

        to_sleep = __sub_ts(to_sleep, req);
        __emit_event_locked(self, COOLMIC_SIMPLE_EVENT_RECONNECT, &(self->thread), &to_sleep, NULL);
    }
    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Left reconnect sleep loop.");
}

static void *__worker(void *userdata)
{
    coolmic_simple_t *self = userdata;

    pthread_mutex_lock(&(self->lock));
    __emit_event_locked(self, COOLMIC_SIMPLE_EVENT_THREAD_POST_START, &(self->thread), NULL, NULL);
    while (1) {
        __worker_inner(self);

        if (self->running == RUNNING_STOPPED || self->running == RUNNING_STOPPING || !self->reconnection_profile)
            break;

        self->running = RUNNING_STARTED;
        __worker_sleep(self);
        if (self->running != RUNNING_STARTED)
            break;
    }
    __emit_event_locked(self, COOLMIC_SIMPLE_EVENT_THREAD_PRE_STOP, &(self->thread), NULL, NULL);
    self->thread_needs_join = 1;
    pthread_mutex_unlock(&(self->lock));
    return NULL;
}

/* thread control functions */
int                 coolmic_simple_start(coolmic_simple_t *self)
{
    enum coolmic_simple_running running;

    if (!self)
        return COOLMIC_ERROR_FAULT;
    pthread_mutex_lock(&(self->lock));
    if (self->running == RUNNING_STOPPED) {
        if (pthread_create(&(self->thread), NULL, __worker, self) == 0) {
            self->running = RUNNING_STARTED;
            __emit_event_locked(self, COOLMIC_SIMPLE_EVENT_THREAD_START, NULL, &(self->thread), NULL);
        }
    }
    running = self->running;
    pthread_mutex_unlock(&(self->lock));
    return running != RUNNING_STOPPED ? COOLMIC_ERROR_NONE : COOLMIC_ERROR_GENERIC;
}

int                 coolmic_simple_stop(coolmic_simple_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Stop has been called.");
    pthread_mutex_lock(&(self->lock));
    __stop_locked(self);
    pthread_mutex_unlock(&(self->lock));
    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Stop has completed.");
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

int                 coolmic_simple_set_vumeter_interval(coolmic_simple_t *self, size_t vumeter_interval) {
    if (!self)
        return COOLMIC_ERROR_FAULT;
    pthread_mutex_lock(&(self->lock));
    self->vumeter_interval = vumeter_interval;
    pthread_mutex_unlock(&(self->lock));
    return COOLMIC_ERROR_NONE;
}

ssize_t             coolmic_simple_get_vumeter_interval(coolmic_simple_t *self) {
    size_t ret;

    if (!self)
        return -1;
    pthread_mutex_lock(&(self->lock));
    ret = self->vumeter_interval;
    pthread_mutex_unlock(&(self->lock));
    return ret;
}

int                 coolmic_simple_set_quality(coolmic_simple_t *self, double quality)
{
    int ret;

    if (!self)
        return COOLMIC_ERROR_FAULT;

    pthread_mutex_lock(&(self->lock));
    ret = coolmic_enc_ctl(self->enc, COOLMIC_ENC_OP_SET_QUALITY, quality);
    pthread_mutex_unlock(&(self->lock));

    return ret;
}

double              coolmic_simple_get_quality(coolmic_simple_t *self)
{
    int ret;
    double quality;

    if (!self)
        return -1024.;
    
    pthread_mutex_lock(&(self->lock));
    ret = coolmic_enc_ctl(self->enc, COOLMIC_ENC_OP_GET_QUALITY, &quality);
    pthread_mutex_unlock(&(self->lock));

    if (ret != COOLMIC_ERROR_NONE)
        return -2048.;

    return quality;
}

int                 coolmic_simple_set_meta(coolmic_simple_t *self, const char *key, const char *value, int replace)
{
    int ret;

    if (!self || !key || !value)
        return COOLMIC_ERROR_FAULT;

    pthread_mutex_lock(&(self->lock));
    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "MetaData: %p", self->metadata);
    if (replace) {
        ret = coolmic_metadata_tag_set(self->metadata, key, value);
    } else {
        ret = coolmic_metadata_tag_add(self->metadata, key, value);
    }
    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "MetaData: %p -> %i", self->metadata, ret);
    pthread_mutex_unlock(&(self->lock));

    return ret;
}

int                 coolmic_simple_restart_encoder(coolmic_simple_t *self)
{
    int ret;

    if (!self)
        return COOLMIC_ERROR_FAULT;

    pthread_mutex_lock(&(self->lock));
    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Restart enc: %p", self->enc);
    ret = coolmic_enc_ctl(self->enc, COOLMIC_ENC_OP_RESTART);
    pthread_mutex_unlock(&(self->lock));

    return ret;
}

coolmic_transform_t *coolmic_simple_get_transform(coolmic_simple_t *self)
{
    if (!self)
        return NULL;
    if (igloo_ro_ref(self->transform) != COOLMIC_ERROR_NONE)
        return NULL;
    return self->transform;
}

int                 coolmic_simple_set_reconnection_profile(coolmic_simple_t *self, const char *profile)
{
    char *n;

    if (!self)
        return COOLMIC_ERROR_FAULT;

    if (!profile || !strcmp(profile, "default"))
        profile = RECON_PROFILE_DEFAULT;

    if (!strcmp(profile, "enabled"))
        profile = RECON_PROFILE_ENABLED;

    n = strdup(profile);
    if (!n)
        return COOLMIC_ERROR_NOMEM;

    pthread_mutex_lock(&(self->lock));
    free(self->reconnection_profile);
    self->reconnection_profile = n;
    pthread_mutex_unlock(&(self->lock));

    return COOLMIC_ERROR_NONE;
}

int                 coolmic_simple_get_reconnection_profile(coolmic_simple_t *self, const char **profile)
{
    if (!self || !profile)
        return COOLMIC_ERROR_FAULT;

    *profile = self->reconnection_profile;

    return COOLMIC_ERROR_NONE;
}

coolmic_simple_segment_t *  coolmic_simple_get_segment(coolmic_simple_t *self)
{
    coolmic_simple_segment_t *ret;

    if (!self)
        return NULL;

    pthread_mutex_lock(&(self->lock));
    if (igloo_ro_ref(self->current_segment) != 0) {
        pthread_mutex_unlock(&(self->lock));
        return NULL;
    }
    ret = self->current_segment;
    pthread_mutex_unlock(&(self->lock));

    return ret;
}

igloo_list_t *              coolmic_simple_get_segment_list(coolmic_simple_t *self)
{
    igloo_list_t *ret;

    if (!self)
        return NULL;

    pthread_mutex_lock(&(self->lock));
    if (igloo_ro_ref(self->segment_list) != 0) {
        pthread_mutex_unlock(&(self->lock));
        return NULL;
    }
    ret = self->segment_list;
    pthread_mutex_unlock(&(self->lock));

    return ret;
}

int                         coolmic_simple_queue_segment(coolmic_simple_t *self, coolmic_simple_segment_t *segment)
{
    int ret;

    if (!self || !segment)
        return COOLMIC_ERROR_FAULT;

    pthread_mutex_lock(&(self->lock));
    ret = igloo_list_push(self->segment_list, segment);
    pthread_mutex_unlock(&(self->lock));

    if (ret != 0)
        return COOLMIC_ERROR_GENERIC;
    return 0;
}

int                 coolmic_simple_change_segment(coolmic_simple_t *self, const char *file)
{
    int ret;

    if (!self)
        return COOLMIC_ERROR_FAULT;

    pthread_mutex_lock(&(self->lock));
    free(self->next_segment);
    if (file) {
        self->next_segment = strdup(file);
    } else {
        self->next_segment = NULL;
    }
    if (self->enc) {
        ret = coolmic_enc_ctl(self->enc, COOLMIC_ENC_OP_STOP);
    } else {
        ret = COOLMIC_ERROR_BUSY;
    }
    pthread_mutex_unlock(&(self->lock));

    return ret;
}
