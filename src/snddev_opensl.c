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

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <coolmic-dsp/snddev.h>
#include <coolmic-dsp/coolmic-dsp.h>

#define BUFFERFRAMES 1024

typedef struct snddev_opensl_lock {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    unsigned char   s;
} snddev_opensl_lock_t;

typedef struct snddev_opensl {
    /* engine interfaces */
    SLObjectItf engine_object;
    SLEngineItf engine_engine;

    /* recorder interfaces */
    SLObjectItf recorder_object;
    SLRecordItf recorder_record;
    SLAndroidSimpleBufferQueueItf recorder_buffer_queue;
    size_t recorder_buffer_length;
    size_t recorder_buffer_offset;
    void * recorder_buffer;
    void * recorder_buffer_other;
    snddev_opensl_lock_t * recorder_lock;
} snddev_opensl_t;

snddev_opensl_lock_t *__lock_new(void)
{
    snddev_opensl_lock_t *ret = calloc(1, sizeof(snddev_opensl_lock_t));

    if (!ret)
        return NULL;

    if (pthread_mutex_init(&ret->mutex, NULL) != 0) {
        free(ret);
        return NULL;
    }

    if (pthread_cond_init(&ret->cond, NULL) != 0) {
        pthread_mutex_destroy(&ret->mutex);
        free(ret);
        return NULL;
    }

    ret->s = 1;

    return ret;
}

void __lock_wait(snddev_opensl_lock_t *lock)
{
    pthread_mutex_lock(&lock->mutex);

    while (!lock->s)
        pthread_cond_wait(&lock->cond, &lock->mutex);

    lock->s = 0;

    pthread_mutex_unlock(&lock->mutex);
}

void __lock_notify(snddev_opensl_lock_t *lock)
{
    pthread_mutex_lock(&lock->mutex);

    lock->s = 1;

    pthread_cond_signal(&lock->cond);
    pthread_mutex_unlock(&lock->mutex);
}

void __lock_free(snddev_opensl_lock_t *lock)
{
    if (!lock)
        return;

    /* send notify, just in case someone is still waiting for it */
    __lock_notify(lock);

    /* lock it before we destory to avoid anyone using it */
    pthread_mutex_lock(&lock->mutex);

    pthread_cond_destroy(&lock->cond);

    /* finnaly unlock and destroy */
    pthread_mutex_unlock(&lock->mutex);
    pthread_mutex_destroy(&lock->mutex);

    free(lock);
}

static SLuint32 __samplerate_to_SLuint32(uint_least32_t rate)
{
    static const struct rates {
        const uint_least32_t rate;
        const SLuint32 res;
    } table[] = {
        {  8000, SL_SAMPLINGRATE_8},
        { 11025, SL_SAMPLINGRATE_11_025},
        { 16000, SL_SAMPLINGRATE_16},
        { 22050, SL_SAMPLINGRATE_22_05},
        { 24000, SL_SAMPLINGRATE_24},
        { 32000, SL_SAMPLINGRATE_32},
        { 44100, SL_SAMPLINGRATE_44_1},
        { 48000, SL_SAMPLINGRATE_48},
        { 64000, SL_SAMPLINGRATE_64 },
        { 88200, SL_SAMPLINGRATE_88_2},
        { 96000, SL_SAMPLINGRATE_96},
        {192000, SL_SAMPLINGRATE_192},
        {0, 0},
    };
    const struct rates *p;

    for (p = table; p->rate; p++)
        if (p->rate == rate)
            return p->res;
    return 0;
}

/* creates the OpenSL ES audio engine */
static SLresult openSLCreateEngine(snddev_opensl_t *self)
{
    SLresult result;

    /* create engine */
    result = slCreateEngine(&(self->engine_object), 0, NULL, 0, NULL, NULL);
    if (result != SL_RESULT_SUCCESS)
        return result;

    /* realize the engine */
    result = (*(self->engine_object))->Realize(self->engine_object, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS)
        return result;

    /* get the engine interface, which is needed in order to create other objects */
    result = (*(self->engine_object))->GetInterface(self->engine_object, SL_IID_ENGINE, &(self->engine_engine));
    if (result != SL_RESULT_SUCCESS)
        return result;

    return SL_RESULT_SUCCESS;
}

void __recorder_callback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    snddev_opensl_t *self = context;
    __lock_notify(self->recorder_lock);
}

static SLresult __recorder_open(snddev_opensl_t *self, uint_least32_t rate, SLuint32 channels, ssize_t buffer)
{
    SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
                                      SL_DEFAULTDEVICEID_AUDIOINPUT, NULL};
    SLDataSource audioSrc = {&loc_dev, NULL};
    SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLuint32 sr = __samplerate_to_SLuint32(rate);
    SLDataFormat_PCM format_pcm;
    SLDataSink audioSnk = {&loc_bq, &format_pcm};
    const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    SLresult result;
    int speakers;

    if (channels == 1) {
        speakers = SL_SPEAKER_FRONT_CENTER;
    } else if (channels == 2) {
        speakers = SL_SPEAKER_FRONT_LEFT|SL_SPEAKER_FRONT_RIGHT;
    } else {
        return SL_RESULT_PARAMETER_INVALID;
    }

    format_pcm = (SLDataFormat_PCM){SL_DATAFORMAT_PCM, channels, sr,
                                    SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                    speakers, SL_BYTEORDER_LITTLEENDIAN};


    self->recorder_buffer_length = (buffer > 0) ? buffer : channels*BUFFERFRAMES*2;
    self->recorder_buffer_offset = self->recorder_buffer_length;
    self->recorder_buffer = calloc(1, self->recorder_buffer_length);
    self->recorder_buffer_other = calloc(1, self->recorder_buffer_length);

    /* create audio recorder
     * (requires the RECORD_AUDIO permission)
     */
    result = (*(self->engine_engine))->CreateAudioRecorder(self->engine_engine, &(self->recorder_object), &audioSrc,
                                                     &audioSnk, 1, id, req);
    if (result != SL_RESULT_SUCCESS)
        return result;

    /* realize the audio recorder */
    result = (*(self->recorder_object))->Realize(self->recorder_object, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS)
        return result;

    /* get the record interface */
    result = (*(self->recorder_object))->GetInterface(self->recorder_object, SL_IID_RECORD, &(self->recorder_record));
    if (result != SL_RESULT_SUCCESS)
        return result;

    /* get the buffer queue interface */
    result = (*(self->recorder_object))->GetInterface(self->recorder_object, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                &(self->recorder_buffer_queue));
    if (result != SL_RESULT_SUCCESS)
        return result;

    /* register callback on the buffer queue */
    result = (*(self->recorder_buffer_queue))->RegisterCallback(self->recorder_buffer_queue, __recorder_callback, self);
    if (result != SL_RESULT_SUCCESS)
        return result;

    result = (*(self->recorder_record))->SetRecordState(self->recorder_record, SL_RECORDSTATE_RECORDING);

    return SL_RESULT_SUCCESS;
}

static ssize_t __read(coolmic_snddev_driver_t *dev, void *buffer, size_t len)
{
    snddev_opensl_t *self = dev->userdata_vp;

    if (self->recorder_buffer_offset == self->recorder_buffer_length) {
        void *other;
        __lock_wait(self->recorder_lock);
        SLresult result = (*(self->recorder_buffer_queue))->Enqueue(self->recorder_buffer_queue, self->recorder_buffer, self->recorder_buffer_length);
        if (result != SL_RESULT_SUCCESS)
            return 0;
        self->recorder_buffer_offset = 0;
        other = self->recorder_buffer_other;
        self->recorder_buffer_other = self->recorder_buffer;
        self->recorder_buffer = other;
    }

    if (len > (self->recorder_buffer_length - self->recorder_buffer_offset))
        len = self->recorder_buffer_length - self->recorder_buffer_offset;

    memcpy(buffer, self->recorder_buffer + self->recorder_buffer_offset, len);
    self->recorder_buffer_offset += len;

    return len;
}

static ssize_t __write(coolmic_snddev_driver_t *dev, const void *buffer, size_t len)
{
    return COOLMIC_ERROR_NOSYS;
}

static int __free(coolmic_snddev_driver_t *dev)
{
    snddev_opensl_t *self = dev->userdata_vp;

    /* destroy audio recorder object, and invalidate all associated interfaces */
    if (self->recorder_object)
        (*(self->recorder_object))->Destroy(self->recorder_object);

    /* destroy engine object, and invalidate all associated interfaces */
    if (self->engine_object)
        (*(self->engine_object))->Destroy(self->engine_object);

    if (self->recorder_buffer)
        free(self->recorder_buffer);
    if (self->recorder_buffer_other)
        free(self->recorder_buffer_other);

    __lock_free(self->recorder_lock);

    free(dev->userdata_vp);
    memset(dev, 0, sizeof(*dev));
    return COOLMIC_ERROR_NONE;
}

int coolmic_snddev_driver_opensl_open(coolmic_snddev_driver_t *dev, const char *driver, void *device, uint_least32_t rate, unsigned int channels, int flags, ssize_t buffer)
{
    snddev_opensl_t *self;

    if (channels > 2)
        return -1;

    self = calloc(1, sizeof(snddev_opensl_t));
    if (!self)
        return COOLMIC_ERROR_NOMEM;

    dev->free = __free;
    dev->read = __read;
    dev->write = __write;
    dev->userdata_vp = self;

    self->recorder_lock = __lock_new();
    if (!self->recorder_lock) {
        __free(dev);
        return COOLMIC_ERROR_GENERIC;
    }

    if (openSLCreateEngine(self) != SL_RESULT_SUCCESS) {
        __free(dev);
        return COOLMIC_ERROR_GENERIC;
    }

    if (flags & COOLMIC_DSP_SNDDEV_RX) {
        if (__recorder_open(self, rate, channels, buffer) != SL_RESULT_SUCCESS) {
            __free(dev);
            return COOLMIC_ERROR_GENERIC;
        }
    }

    return COOLMIC_ERROR_NONE;
}
