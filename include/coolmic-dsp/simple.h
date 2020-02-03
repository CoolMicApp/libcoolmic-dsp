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

/*
 * This provides a very simple interface for the encoder framework.
 */

#ifndef __COOLMIC_DSP_SIMPLE_H__
#define __COOLMIC_DSP_SIMPLE_H__

#include <stdint.h>

#include <igloo/ro.h>
#include <igloo/list.h>
#include "shout.h"
#include "transform.h"
#include "simple-segment.h"

/* forward declare internally used structures */
typedef struct coolmic_simple coolmic_simple_t;

/* Connection states */
typedef enum coolmic_simple_connectionstate {
    /* invalid connection state. */
    COOLMIC_SIMPLE_CS_INVALID              = -1,
    /* API is connecting to the server. */
    COOLMIC_SIMPLE_CS_CONNECTING           =  1,
    /* API successfully connected to the server. */
    COOLMIC_SIMPLE_CS_CONNECTED            =  2,
    /* API is disconnecting from server. */
    COOLMIC_SIMPLE_CS_DISCONNECTING        =  3,
    /* API disconnected (or got disconnected) from server. */
    COOLMIC_SIMPLE_CS_DISCONNECTED         =  4,
    /* There is a connection error.
     * Next state is likely COOLMIC_SIMPLE_CS_DISCONNECTING,
     * COOLMIC_SIMPLE_CS_DISCONNECTED or COOLMIC_SIMPLE_CS_CONNECTING.
     */
    COOLMIC_SIMPLE_CS_CONNECTIONERROR      =  5
} coolmic_simple_connectionstate_t;

/* Events emitted by simple API */
typedef enum coolmic_simple_event {
    /* some invalid event
     * arg0 and arg1 are undefined.
     */
    COOLMIC_SIMPLE_EVENT_INVALID           = -1,
    /* no event happend.
     * arg0 and arg1 are undefined.
     */
    COOLMIC_SIMPLE_EVENT_NONE              =  0,
    /* an error happend.
     * arg0 points to an const int containing the error value or NULL.
     * YOU MUST NOT ALTER THIS VALUE.
     * arg1 is undefined.
     */
    COOLMIC_SIMPLE_EVENT_ERROR             =  1,
    /* a thread got started.
     * arg0 is the pointer (see 'thread' below) to the newly created thread.
     * arg1 is undefined.
     */
    COOLMIC_SIMPLE_EVENT_THREAD_START      =  2,
    /* as COOLMIC_SIMPLE_EVENT_THREAD_START but ran inside the thread.
     * arg0 and arg1 are undefined.
     */
    COOLMIC_SIMPLE_EVENT_THREAD_POST_START =  3,
    /* a thread is stopped.
     * arg0 is the pointer (see 'thread' below) to the stopped thread.
     * arg1 is undefined.
     */
    COOLMIC_SIMPLE_EVENT_THREAD_STOP       =  4,
    /* as COOLMIC_SIMPLE_EVENT_THREAD_STOP but ran inside the thread.
     * arg0 and arg1 are undefined.
     */
    COOLMIC_SIMPLE_EVENT_THREAD_PRE_STOP   =  5,
    /* a VU-Meter result is ready for use.
     * arg0 is the result (coolmic_vumeter_result_t*).
     * arg1 is undefined.
     */
    COOLMIC_SIMPLE_EVENT_VUMETER_RESULT    =  6,
    /* A stream state change.
     * arg0 is a pointer to a coolmic_simple_connectionstate_t object.
     * arg1 points to an const int containing the error value or NULL.
     * YOU MUST NOT ALTER THOSE VALUES.
     */
    COOLMIC_SIMPLE_EVENT_STREAMSTATE       =  7,
    /* A stream reconnect is schedule.
     * arg0 is a pointer to a const struct timespec that is
     *      the remaining time before reconnect.
     * arg1 is undefined.
     * YOU MUST NOT ALTER THOSE VALUES.
     */
    COOLMIC_SIMPLE_EVENT_RECONNECT         = 8,
    /* A segment is connected to the stream.
     * arg0 is a pointer to a coolmic_simple_segment_pipeline_t.
     * arg1 is undefined.
     * YOU MUST NOT ALTER THOSE VALUES.
     */
    COOLMIC_SIMPLE_EVENT_SEGMENT_CONNECT   = 9,
    /* A segment is disconnected to the stream.
     * arg0 is a pointer to a coolmic_simple_segment_pipeline_t.
     * arg1 is undefined.
     * YOU MUST NOT ALTER THOSE VALUES.
     */
    COOLMIC_SIMPLE_EVENT_SEGMENT_DISCONNECT= 10,
} coolmic_simple_event_t;

/* Generic callback for events.
 *
 * Parameters:
 * * inst is the coolmic_simple_t instance calling.
 * * userdata is the userdata pointer as passed on setting the callback function.
 * * event is the event code as defined in coolmic_simple_event_t.
 * * thread is set to a unique pointer to some OS specific structure.
 *   This pointer can be used to identify the thread emiting the event.
 * * arg0's and arg1's usage is up to the specific event.
 *
 * Return value:
 * The callback is expected to return 0 in case of success.
 * The callback is expected to never fail.
 *
 * Notes:
 * Note that as this library is multi-threaded events such as
 * COOLMIC_SIMPLE_EVENT_THREAD_START and COOLMIC_SIMPLE_EVENT_THREAD_POST_START
 * may be emitted in counterintuitive order.
 */
typedef int (*coolmic_simple_callback_t)(coolmic_simple_t *inst, void *userdata, coolmic_simple_event_t event, void *thread, void *arg0, void *arg1);

/* Management of the encoder object */
coolmic_simple_t   *coolmic_simple_new(const char *name, igloo_ro_t associated, const char *codec, uint_least32_t rate, unsigned int channels, ssize_t buffer, const coolmic_shout_config_t *conf);

/* thread control functions */
int                 coolmic_simple_start(coolmic_simple_t *self);
int                 coolmic_simple_stop(coolmic_simple_t *self);

/* status callbacks */
int                 coolmic_simple_set_callback(coolmic_simple_t *self, coolmic_simple_callback_t callback, void *userdata);

/* VU-Meter control */
/* This sets the VU-Meter interval.
 * Useful values are in range of [10:400].
 * Setting this to 0 disables regular VU-Meter events.
 * Setting this to any value may cause a out of sync event to update values to the new interval.
 */
int                 coolmic_simple_set_vumeter_interval(coolmic_simple_t *self, size_t vumeter_interval);
/* This sets the current VU-Meter interval.
 */
ssize_t             coolmic_simple_get_vumeter_interval(coolmic_simple_t *self);

/* Quality level */
/* This sets quality level for quality based codecs such as Vorbis.
 * Range is from -0.1 to 1.0.
 */
int                 coolmic_simple_set_quality(coolmic_simple_t *self, double quality);
double              coolmic_simple_get_quality(coolmic_simple_t *self);

/* Simple metadata function */
/* This allows very simple manipulation of the meta data.
 * If replace is false the value is added to the key. If true the value is replaced by the new one.
 */
int                 coolmic_simple_set_meta(coolmic_simple_t *self, const char *key, const char *value, int replace);

/* Restart the encoder */
/* This is used to apply quality changes as well as meta data changes.
 * Call this after you fisnihed changeing encoder settings such as quality or meta data.
 * This does not reconnect to the streaming server (e.g. Icecast2) or cause interrupts on the
 * listeners end.
 */
int                 coolmic_simple_restart_encoder(coolmic_simple_t *self);

/* Get the internally used transform object.
 * This allows to manipulate the transformation of the input signal by applying e.g. gains.
 */
coolmic_transform_t *coolmic_simple_get_transform(coolmic_simple_t *self);


/* Auto-Reconnect support */
/* This sets and gets the reconnection profile.
 * The profile returned by the getter is valid only untill altered using the setter or destrction of the object.
 */
int                 coolmic_simple_set_reconnection_profile(coolmic_simple_t *self, const char *profile);
int                 coolmic_simple_get_reconnection_profile(coolmic_simple_t *self, const char **profile);

coolmic_simple_segment_t *  coolmic_simple_get_segment(coolmic_simple_t *self);
igloo_list_t *              coolmic_simple_get_segment_list(coolmic_simple_t *self);
int                         coolmic_simple_queue_segment(coolmic_simple_t *self, coolmic_simple_segment_t *segment);
int                         coolmic_simple_switch_segment(coolmic_simple_t *self);

#endif
