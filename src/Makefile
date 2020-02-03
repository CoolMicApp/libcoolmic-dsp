include Makefile.inc

CMDSP_TARGET=posix
CMDSP_HAVE_OSS=true

TARGET=libcoolmic-dsp.so

OBJS=$(CMDSP_SOURCE_FILES:.c=.o)

ROOT ?= $(HOME)/ROOT
CFLAGS=$(CMDSP_CFLAGS) -I$(ROOT)/include
LDFLAGS=$(CMDSP_LDFLAGS) -L$(ROOT)/lib
LIBS=$(CMDSP_LIBS)

all: $(TARGET)
clean:
	rm -f $(TARGET) *.o
new: clean all
distclean: clean

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -shared -o $(TARGET) $(OBJS) $(LIBS)

test-%: test-%.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(TARGET) -ligloo

.PHONY: all clean new distclean
