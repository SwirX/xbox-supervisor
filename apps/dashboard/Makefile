CROSS     := xenon-
CC        := $(CROSS)gcc
OBJCOPY   := $(CROSS)objcopy
STRIP     := $(CROSS)strip

CFLAGS    := -O2 -Wall -DXENON -m32 -maltivec -fno-pic -mpowerpc64 -mhard-float
CFLAGS    += -mcpu=cell -mtune=cell
CFLAGS    += -I../../include -I/usr/local/xenon/usr/include

LDFLAGS   := -nostartfiles -n -Wl,-m,elf32ppc -T../../guest/guest.lds -Wl,-Map,dashboard.map
LDFLAGS   += -L/usr/local/xenon/usr/lib -L/usr/local/xenon/xenon/lib/32
LDFLAGS   += -L/usr/local/xenon/lib/gcc/xenon/9.2.0/32

LIBS      := -L../../sdk -lxenolith -lxenon -lgcc

ASM_SRCS  := start.S
C_SRCS    := main.c
OBJS      := $(ASM_SRCS:.S=.o) $(C_SRCS:.c=.o)

.PHONY: all clean

all: app.elf dashboard.elf32

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

dashboard.elf: $(OBJS) ../../guest/guest.lds
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	$(CROSS)size $@

dashboard.elf32: dashboard.elf
	$(OBJCOPY) -O elf32-powerpc $< $@
	$(STRIP) $@
	ls -lh $@

app.elf: dashboard.elf32
	cp $< $@

clean:
	rm -f $(OBJS) dashboard.elf dashboard.elf32 app.elf dashboard.map
