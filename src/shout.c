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

#define COOLMIC_COMPONENT "libcoolmic-dsp/shout"
#include <stdlib.h>
#include <shout/shout.h>
#include "types_private.h"
#include <coolmic-dsp/shout.h>
#include <coolmic-dsp/coolmic-dsp.h>
#include <coolmic-dsp/logging.h>

struct coolmic_shout {
    /* base type */
    igloo_ro_base_t __base;

    shout_t *shout;
    coolmic_iohandle_t *in;
    int need_next_segment;
};

static void __free(igloo_ro_t self)
{
    coolmic_shout_t *shout = igloo_RO_TO_TYPE(self, coolmic_shout_t);

    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Asked to shut down.");

    shout_close(shout->shout);
    shout_free(shout->shout);
    igloo_ro_unref(shout->in);

    shout_shutdown();
    coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "... and down.");
}

static int __new(igloo_ro_t self, const igloo_ro_type_t *type, va_list ap)
{
    coolmic_shout_t *shout = igloo_RO_TO_TYPE(self, coolmic_shout_t);

    (void)type, (void)ap;

    shout_init();

    shout->shout = shout_new();
    if (!shout->shout) {
        shout_shutdown();
        return -1;
    }

    shout_set_nonblocking(shout->shout, SHOUT_BLOCKING_NONE);

    /* set some stuff that is always the same for all connections */
    shout_set_protocol(shout->shout, SHOUT_PROTOCOL_HTTP);
    shout_set_format(shout->shout, SHOUT_FORMAT_OGG);

    return 0;
}

igloo_RO_PUBLIC_TYPE(coolmic_shout_t,
        igloo_RO_TYPEDECL_FREE(__free),
        igloo_RO_TYPEDECL_NEW(__new)
        );

static int libshouterror2error(const int err) {
    switch (err) {
        case SHOUTERR_SUCCESS:
            return COOLMIC_ERROR_NONE;
        break;
        case SHOUTERR_INSANE:
            /* could also be COOLMIC_ERROR_FAULT. But libshout isn't too clear on that. */
            return COOLMIC_ERROR_INVAL;
        break;
        case SHOUTERR_NOCONNECT:
            return COOLMIC_ERROR_CONNREFUSED;
        break;
        case SHOUTERR_NOLOGIN:
            return COOLMIC_ERROR_PERM;
        break;
        case SHOUTERR_MALLOC:
            return COOLMIC_ERROR_NOMEM;
        break;
        case SHOUTERR_CONNECTED:
            return COOLMIC_ERROR_CONNECTED;
        break;
        case SHOUTERR_UNCONNECTED:
            return COOLMIC_ERROR_UNCONNECTED;
        break;
        case SHOUTERR_UNSUPPORTED:
            return COOLMIC_ERROR_NOSYS;
        break;
        case SHOUTERR_BUSY:
            return COOLMIC_ERROR_BUSY;
        break;
#ifdef SHOUTERR_NOTLS
        case SHOUTERR_NOTLS:
            return COOLMIC_ERROR_NOTLS;
        break;
#endif
#ifdef SHOUTERR_TLSBADCERT
        case SHOUTERR_TLSBADCERT:
            return COOLMIC_ERROR_TLSBADCERT;
        break;
#endif
#ifdef SHOUTERR_RETRY
        case SHOUTERR_RETRY:
            return COOLMIC_ERROR_RETRY;
        break;
#endif
        case SHOUTERR_SOCKET:
        case SHOUTERR_METADATA:
        default:
            return COOLMIC_ERROR_GENERIC;
        break;
    }
}

static inline int libshout2error(coolmic_shout_t *self) {
    return libshouterror2error(shout_get_errno(self->shout));
}

int              coolmic_shout_set_config(coolmic_shout_t *self, const coolmic_shout_config_t *conf)
{
    char ua[256];

    if (!self || !conf)
        return COOLMIC_ERROR_FAULT;

    if (shout_set_host(self->shout, conf->hostname) != SHOUTERR_SUCCESS)
        return libshout2error(self);

    if (shout_set_port(self->shout, conf->port) != SHOUTERR_SUCCESS)
        return libshout2error(self);

#ifdef SHOUT_TLS
    if (shout_set_tls(self->shout, conf->tlsmode) != SHOUTERR_SUCCESS)
        return libshout2error(self);

    if (conf->cadir)
        if (shout_set_ca_directory(self->shout, conf->cadir) != SHOUTERR_SUCCESS)
            return libshout2error(self);

    if (conf->cafile)
        if (shout_set_ca_file(self->shout, conf->cafile) != SHOUTERR_SUCCESS)
            return libshout2error(self);

    if (conf->client_cert)
        if (shout_set_client_certificate(self->shout, conf->client_cert) != SHOUTERR_SUCCESS)
            return libshout2error(self);
#else
    if (!(conf->tlsmode == 0 || conf->tlsmode == 1)) /* 0 = plain, 1 = auto (plain allowed) */
        return COOLMIC_ERROR_NOSYS;
    if (conf->cadir || conf->cafile || conf->cafile)
        return COOLMIC_ERROR_NOSYS;
#endif

    if (shout_set_mount(self->shout, conf->mount) != SHOUTERR_SUCCESS)
        return libshout2error(self);

    if (conf->username)
        if (shout_set_user(self->shout, conf->username) != SHOUTERR_SUCCESS)
            return libshout2error(self);

    if (shout_set_password(self->shout, conf->password) != SHOUTERR_SUCCESS)
        return libshout2error(self);

    if (conf->software_name && conf->software_version && conf->software_comment) {
        snprintf(ua, sizeof(ua), "%s/%s (%s) libcoolmic-dsp libshout/%s", conf->software_name, conf->software_version, conf->software_comment, shout_version(NULL, NULL, NULL));
    } else if (conf->software_name && conf->software_version) {
        snprintf(ua, sizeof(ua), "%s/%s libcoolmic-dsp libshout/%s", conf->software_name, conf->software_version, shout_version(NULL, NULL, NULL));
    } else if (conf->software_name) {
        snprintf(ua, sizeof(ua), "%s libcoolmic-dsp libshout/%s", conf->software_name, shout_version(NULL, NULL, NULL));
    } else {
        snprintf(ua, sizeof(ua), "libcoolmic-dsp libshout/%s", shout_version(NULL, NULL, NULL));
    }

    shout_set_agent(self->shout, ua);

    return COOLMIC_ERROR_NONE;
}

int              coolmic_shout_attach_iohandle(coolmic_shout_t *self, coolmic_iohandle_t *handle)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    if (self->in)
        igloo_ro_unref(self->in);
    /* ignore errors here as handle is allowed to be NULL */
    igloo_ro_ref(self->in = handle);
    return COOLMIC_ERROR_NONE;
}

int              coolmic_shout_start(coolmic_shout_t *self)
{
    int ret;

    if (!self)
        return COOLMIC_ERROR_FAULT;

    ret = shout_get_connected(self->shout);
    if (ret == SHOUTERR_CONNECTED)
        return COOLMIC_ERROR_NONE;
    if (ret != SHOUTERR_UNCONNECTED)
        return libshouterror2error(ret);

    if (shout_open(self->shout) != SHOUTERR_SUCCESS)
        return libshout2error(self);

    return COOLMIC_ERROR_NONE;
}
int              coolmic_shout_stop(coolmic_shout_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;

    if (shout_get_connected(self->shout) == SHOUTERR_UNCONNECTED)
        return COOLMIC_ERROR_NONE;

    if (shout_close(self->shout) != SHOUTERR_SUCCESS)
        return libshout2error(self);

    return COOLMIC_ERROR_NONE;
}

int              coolmic_shout_iter(coolmic_shout_t *self)
{
    char buffer[1024];
    ssize_t ret;
    int shouterror = SHOUTERR_SUCCESS;

    if (!self)
        return COOLMIC_ERROR_FAULT;

    if (shout_get_connected(self->shout) == SHOUTERR_UNCONNECTED)
        return COOLMIC_ERROR_UNCONNECTED;

    if (self->in) {
        ret = coolmic_iohandle_read(self->in, buffer, sizeof(buffer));
        coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "Got %zi bytes from backend", ret);
        if (ret > 0) {
            shouterror = shout_send(self->shout, (void*)buffer, (size_t)ret);
            coolmic_logging_log(COOLMIC_LOGGING_LEVEL_DEBUG, COOLMIC_ERROR_NONE, "shout status: %i: %s", shouterror, shout_get_error(self->shout));
            self->need_next_segment = 0;
        } else {
            self->need_next_segment = 1;
        }
    } else {
        self->need_next_segment = 1;
    }

    shout_sync(self->shout);

    return libshouterror2error(shouterror);
}

int              coolmic_shout_need_next_segment(coolmic_shout_t *self, int *need)
{
    if (!self || !need)
        return COOLMIC_ERROR_FAULT;

    *need = self->need_next_segment;

    return COOLMIC_ERROR_NONE;
}
