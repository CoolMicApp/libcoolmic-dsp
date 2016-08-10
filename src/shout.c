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
#include <shout/shout.h>
#include <coolmic-dsp/shout.h>
#include <coolmic-dsp/coolmic-dsp.h>

struct coolmic_shout {
    size_t refc;
    shout_t *shout;
    coolmic_iohandle_t *in;
};

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
        case SHOUTERR_SOCKET:
        case SHOUTERR_METADATA:
#ifdef SHOUTERR_RETRY
        case SHOUTERR_RETRY:
#endif
        default:
            return COOLMIC_ERROR_GENERIC;
        break;
    }
}

static inline int libshout2error(coolmic_shout_t *self) {
    return libshouterror2error(shout_get_errno(self->shout));
}

coolmic_shout_t *coolmic_shout_new(void)
{
    coolmic_shout_t *ret = calloc(1, sizeof(coolmic_shout_t));
    if (!ret)
        return NULL;

    shout_init();

    ret->refc = 1;
    ret->shout = shout_new();
    if (!ret->shout) {
        free(ret);
        shout_shutdown();
        return NULL;
    }

    /* set some stuff that is always the same for all connections */
    shout_set_protocol(ret->shout, SHOUT_PROTOCOL_HTTP);
    shout_set_format(ret->shout, SHOUT_FORMAT_OGG);

    return ret;
}

int              coolmic_shout_set_config(coolmic_shout_t *self, const coolmic_shout_config_t *conf)
{
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

    return COOLMIC_ERROR_NONE;
}

int              coolmic_shout_ref(coolmic_shout_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    self->refc++;
    return COOLMIC_ERROR_NONE;
}

int              coolmic_shout_unref(coolmic_shout_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    self->refc--;

    if (self->refc)
        return COOLMIC_ERROR_NONE;

    shout_close(self->shout);
    shout_free(self->shout);
    coolmic_iohandle_unref(self->in);

    free(self);

    shout_shutdown();

    return COOLMIC_ERROR_NONE;
}

int              coolmic_shout_attach_iohandle(coolmic_shout_t *self, coolmic_iohandle_t *handle)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;
    if (self->in)
        coolmic_iohandle_unref(self->in);
    /* ignore errors here as handle is allowed to be NULL */
    coolmic_iohandle_ref(self->in = handle);
    return COOLMIC_ERROR_NONE;
}

int              coolmic_shout_start(coolmic_shout_t *self)
{
    if (!self)
        return COOLMIC_ERROR_FAULT;

    if (shout_get_connected(self->shout) == SHOUTERR_CONNECTED)
        return COOLMIC_ERROR_NONE;

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

    ret = coolmic_iohandle_read(self->in, buffer, sizeof(buffer));
    if (ret > 0)
        shouterror = shout_send(self->shout, (void*)buffer, ret);

    shout_sync(self->shout);

    return libshouterror2error(shouterror);
}
