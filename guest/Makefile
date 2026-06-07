# Guest payload — minimal dashboard using IPC input

CROSS       := xenon-
CC          := $(CROSS)gcc
OBJCOPY     := $(CROSS)objcopy
STRIP       := $(CROSS)strip

CFLAGS  = -O2 -Wall -DXENON -m32 -maltivec -fno-pic -mpowerpc64 -mhard-float
CFLAGS += -mcpu=cell -mtune=cell
CFLAGS += -I/usr/local/xenon/usr/include -I../include
CFLAGS += -L/usr/local/xenon/usr/lib
CFLAGS += -L/usr/local/xenon/xenon/lib/32
CFLAGS += -L/usr/local/xenon/lib/gcc/xenon/9.2.0/32

LDFLAGS  = -nostartfiles -n -Wl,-m,elf32ppc -Tguest.lds -Wl,-Map,guest.map

LIBS = -lxenon -lgcc

ASM_SRCS := start.S
C_SRCS   := main.c
OBJS     := $(ASM_SRCS:.S=.o) $(C_SRCS:.c=.o)

.PHONY: all guest clean

all: guest

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

guest: payload.elf32

payload.elf: $(OBJS) guest.lds
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	@echo "--- Linked $@ ---"
	@$(CROSS)size $@

payload.elf32: payload.elf
	$(OBJCOPY) -O elf32-powerpc $< $@
	$(STRIP) $@
	@ls -lh $@

clean:
	rm -f $(OBJS) payload.elf payload.elf32 guest.map
