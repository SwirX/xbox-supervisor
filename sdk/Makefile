CROSS     := xenon-
CC        := $(CROSS)gcc
AR        := $(CROSS)ar
CFLAGS    := -O2 -Wall -DXENON -m32 -maltivec -fno-pic -mpowerpc64 -mhard-float
CFLAGS    += -mcpu=cell -mtune=cell
CFLAGS    += -I../include -I/usr/local/xenon/usr/include

SRCS      := init.c present.c svc_send.c svc_notify.c input.c xl_ticks.c stubs.c svc_app.c
EXT_SRCS  := ../source/ipc_ring.c
OBJS      := $(SRCS:.c=.o) $(EXT_SRCS:../source/%.c=%.o)
LIB       := libxenolith.a

.PHONY: all clean

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) rcs $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

ipc_ring.o: ../source/ipc_ring.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(LIB)
