
CMDSP_TARGET   ?= posix
CMDSP_HAVE_OSS ?= false

CMDSP_SOURCE_FILES = \
	common_opus.c \
	coolmic-dsp.c \
	enc.c \
	enc_opus.c \
	enc_vorbis.c \
	iohandle.c \
	logging.c \
	metadata.c \
	shout.c \
	simple.c \
	simple-segment.c \
	snddev.c \
	snddev_null.c \
	snddev_sine.c \
	snddev_stdio.c \
	tee.c \
	transform.c \
	util.c \
	vumeter.c

CMDSP_CFLAGS  = -Wall -Wextra -O2 -g -fPIC -I../include -DHAVE_SNDDRV_DRIVER_STDIO -DHAVE_ENC_OPUS -D_GNU_SOURCE
CMDSP_LDFLAGS = -Wall -Wextra -O2 -g
CMDSP_LIBS    = -pthread -lshout -lvorbis -lvorbisenc -lm -lopus -ligloo

ifeq ($(CMDSP_HAVE_OSS),true)
	CMDSP_SOURCE_FILES += snddev_oss.c
	CMDSP_CFLAGS += -DHAVE_SNDDRV_DRIVER_OSS
endif

ifeq ($(CMDSP_TARGET),android)
	CMDSP_SOURCE_FILES += snddev_opensl.c
	CMDSP_CFLAGS += -DHAVE_SNDDRV_DRIVER_OPENSL
endif
