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

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <coolmic-dsp/metadata.h>
#include <coolmic-dsp/coolmic-dsp.h>

#define TAG_SLOT_INCREMENT 8 /* how many tag slots to add when in need for a new one. */

struct coolmic_metadata_tag {
    char *key;
    size_t values_len;
    char **values;
    size_t iter_value;
};

struct coolmic_metadata {
    /* reference counter */
    size_t refc;

    /* object lock */
    pthread_mutex_t lock;

    /* Storage for tags */
    coolmic_metadata_tag_t *tags;
    size_t tags_len;

    size_t iter_tag;
};

static void __clear_tag_values(coolmic_metadata_tag_t *tag)
{
    size_t i;

    if (!tag->values)
        return;

    for (i = 0; i < tag->values_len; i++) {
        if (tag->values[i]) {
            free(tag->values[i]);
        }
    }

    memset(tag->values, 0, sizeof(tag->values[0])*tag->values_len);
}

static void __delete_tag(coolmic_metadata_tag_t *tag)
{
    if (tag->key)
        free(tag->key);
    tag->key = NULL;

    __clear_tag_values(tag);

    if (tag->values) {
        free(tag->values);
        tag->values_len = 0;
    }
}

static int __add_tag_value (coolmic_metadata_tag_t *tag, const char *value)
{
    size_t i;
    char **values_new;
    char **next;

    /* first see if we got a free slot. */
    if (tag->values) {
        for (i = 0; i < tag->values_len; i++) {
            if (tag->values[i])
                continue;

            tag->values[i] = strdup(value);
            if (!tag->values[i])
                return COOLMIC_ERROR_NOMEM;
            return COOLMIC_ERROR_NONE;
        }
    }

    /* Ok, bad luck, need to allocate a new one. */
    values_new = realloc(tag->values, sizeof(tag->values[0])*(tag->values_len + TAG_SLOT_INCREMENT));
    if (!values_new)
        return COOLMIC_ERROR_NOMEM;

    next = values_new + sizeof(tag->values[0])*tag->values_len;
    memset(next, 0, sizeof(tag->values[0])*TAG_SLOT_INCREMENT);
    tag->values = values_new;
    tag->values_len += TAG_SLOT_INCREMENT;

    *next = strdup(value);
    if (!*next)
        return COOLMIC_ERROR_NOMEM;
    return COOLMIC_ERROR_NONE;
}

static coolmic_metadata_tag_t * __add_tag(coolmic_metadata_t *self, const char *key)
{
    coolmic_metadata_tag_t *tag;
    coolmic_metadata_tag_t *tags_new;
    size_t i;

    /* First look if we have a free tag-slot. */
    if (self->tags) {
        for (i = 0; i < self->tags_len; i++) {
            tag = &(self->tags[i]);

            if (tag->key) {
                if (strcasecmp(tag->key, key) == 0) {
                    return tag;
                } else {
                    continue;
                }
            }

            tag->key = strdup(key);
            if (!tag->key) /* memory allocation problem */
                return NULL;

            return tag;
        }
    }

    /* Ok, we don't have a free tag-slot, make one. */
    tags_new = realloc(self->tags, sizeof(coolmic_metadata_tag_t)*(self->tags_len + TAG_SLOT_INCREMENT));
    if (!tags_new) /* memory allocation problem */
        return NULL;

    tag = tags_new + sizeof(coolmic_metadata_tag_t)*self->tags_len; /* find first new slot */
    memset(tag, 0, sizeof(coolmic_metadata_tag_t)*TAG_SLOT_INCREMENT);

    self->tags = tags_new;
    self->tags_len += TAG_SLOT_INCREMENT;

    tag->key = strdup(key);
    if (!tag->key) /* memory allocation problem */
        return NULL;

    return tag;
}

/* Management of the metadata object */
coolmic_metadata_t      *coolmic_metadata_new(void)
{
    coolmic_metadata_t *ret;

    ret = calloc(1, sizeof(coolmic_metadata_t));
    if (!ret)
        return NULL;

    ret->refc = 1;
    pthread_mutex_init(&(ret->lock), NULL);

    return ret;
}

int                 coolmic_metadata_ref(coolmic_metadata_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    pthread_mutex_lock(&(self->lock));
    self->refc++;
    pthread_mutex_unlock(&(self->lock));
    return COOLMIC_ERROR_NONE;
}

int                 coolmic_metadata_unref(coolmic_metadata_t *self)
{
    size_t i;

    if (!self)
        return COOLMIC_ERROR_FAULT;

    pthread_mutex_lock(&(self->lock));
    self->refc--;

    if (self->refc) {
        pthread_mutex_unlock(&(self->lock));
        return COOLMIC_ERROR_NONE;
    }

    if (self->tags) {
        for (i = 0; i < self->tags_len; i++) {
            if (self->tags[i].key != NULL) {
                __delete_tag(&(self->tags[i]));
            }
        }

        free(self->tags);
    }

    pthread_mutex_unlock(&(self->lock));
    pthread_mutex_destroy(&(self->lock));
    free(self);

    return COOLMIC_ERROR_NONE;
}

int                      coolmic_metadata_tag_add(coolmic_metadata_t *self, const char *key, const char *value)
{
    coolmic_metadata_tag_t *tag;
    int ret;

    if (!self || !key || !value)
        return COOLMIC_ERROR_FAULT;

    pthread_mutex_lock(&(self->lock));
    tag = __add_tag(self, key);
    if (!tag) {
        pthread_mutex_unlock(&(self->lock));
        return COOLMIC_ERROR_NOMEM;
    }

    ret = __add_tag_value(tag, value);
    pthread_mutex_unlock(&(self->lock));
    return ret;
}

int                      coolmic_metadata_tag_set(coolmic_metadata_t *self, const char *key, const char *value)
{
    coolmic_metadata_tag_t *tag;
    int ret;

    if (!self || !key || !value)
        return COOLMIC_ERROR_FAULT;

    pthread_mutex_lock(&(self->lock));
    tag = __add_tag(self, key);
    if (!tag) {
        pthread_mutex_unlock(&(self->lock));
        return COOLMIC_ERROR_NOMEM;
    }

    __clear_tag_values(tag);
    ret = __add_tag_value(tag, value);
    pthread_mutex_unlock(&(self->lock));
    return ret;
}

int                      coolmic_metadata_tag_remove(coolmic_metadata_t *self, const char *key)
{
    size_t i;

    if (!self || !key)
        return COOLMIC_ERROR_FAULT;

    if (!self->tags)
        return COOLMIC_ERROR_INVAL;

    pthread_mutex_lock(&(self->lock));
    for (i = 0; i < self->tags_len; i++) {
        if (strcasecmp(self->tags[i].key, key) == 0) {
            __clear_tag_values(&(self->tags[i]));
            pthread_mutex_unlock(&(self->lock));
            return COOLMIC_ERROR_NONE;
        }
    }
    pthread_mutex_unlock(&(self->lock));

    return COOLMIC_ERROR_NONE;
}

int                      coolmic_metadata_add_to_vorbis_comment(coolmic_metadata_t *self, vorbis_comment *vc)
{
    size_t i, j;
    coolmic_metadata_tag_t *tag;

    if (!self || !vc)
        return COOLMIC_ERROR_FAULT;

    if (!self->tags)
        return COOLMIC_ERROR_INVAL;

    pthread_mutex_lock(&(self->lock));
    for (i = 0; i < self->tags_len; i++) {
        tag = &(self->tags[i]);

        if (!tag->key)
            continue;

        for (j = 0; j < tag->values_len; j++) {
            if (!tag->values[j])
                continue;

            vorbis_comment_add_tag(vc, tag->key, tag->values[j]);
        }
    }
    pthread_mutex_unlock(&(self->lock));

    return COOLMIC_ERROR_NONE;
}

int                      coolmic_metadata_iter_start(coolmic_metadata_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    pthread_mutex_lock(&(self->lock));
    self->iter_tag = 0;
    return COOLMIC_ERROR_NONE;
}

int                      coolmic_metadata_iter_end(coolmic_metadata_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    pthread_mutex_unlock(&(self->lock));
    return COOLMIC_ERROR_NONE;
}

int                      coolmic_metadata_iter_rewind(coolmic_metadata_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    self->iter_tag = 0;
    return COOLMIC_ERROR_NONE;
}

coolmic_metadata_tag_t  *coolmic_metadata_iter_next_tag(coolmic_metadata_t *self)
{
    if (!self)
        return NULL;

    for (; self->iter_tag < self->tags_len; self->iter_tag++) {
        if (self->tags[self->iter_tag].key) {
            self->tags[self->iter_tag].iter_value = 0;
            return &(self->tags[self->iter_tag++]);
        }
    }

    return NULL;
}

const char              *coolmic_metadata_iter_tag_key(coolmic_metadata_tag_t *tag)
{
    if (!tag)
        return NULL;
    return tag->key;
}

const char              *coolmic_metadata_iter_tag_next_value(coolmic_metadata_tag_t *tag)
{
    if (!tag)
        return NULL;

    for (; tag->iter_value < tag->values_len; tag->iter_value++) {
        if (tag->values[tag->iter_value])
            return tag->values[tag->iter_value++];
    }
    return NULL;
}
