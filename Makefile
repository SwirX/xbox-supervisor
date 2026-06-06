# Xbox 360 Supervisor — libxenon build
#
# Targets:
#   make elf      - produce supervisor.elf32 (for USB / TFTP deploy)
#   make clean    - remove build artifacts
#   make deploy   - build + copy to TFTP root

TARGET       := supervisor
DEVKITXENON  := /usr/local/xenon
DOCKER_IMAGE := free60/libxenon

CROSS        := xenon-
CC           := $(CROSS)gcc
OBJCOPY      := $(CROSS)objcopy
STRIP        := $(CROSS)strip

CFLAGS  = -g -O2 -Wall -Werror -Wno-main
CFLAGS += -DXENON -m32 -maltivec -fno-pic -mpowerpc64 -mhard-float
CFLAGS += -mcpu=cell -mtune=cell
CFLAGS += -I$(DEVKITXENON)/usr/include -Iinclude

LDFLAGS  = -n -Wl,-Map,$(TARGET).map
LDFLAGS += -L$(DEVKITXENON)/usr/lib
LDFLAGS += -L$(DEVKITXENON)/xenon/lib/32
LDFLAGS += -Tsupervisor.lds

LIBS = -lxenon -lm

# Source files (no vectors.S, uart.c, stubs.c, shmem.c, elf_mapper.c — libxenon provides those)
SOURCES := \
    source/supervisor.c     \
    source/ipc_ring.c       \
    source/core1_engine.c

ASM_SOURCES := \
    source/guest_trampoline.S

OBJS := $(SOURCES:.c=.o) $(ASM_SOURCES:.S=.o)

.PHONY: all elf clean deploy docker-build

all: elf

# ── Pattern rules ──

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Link ──

$(TARGET).elf: $(OBJS) supervisor.lds
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	@echo "--- Linked $(TARGET).elf ---"
	@$(CROSS)size $@

elf: $(TARGET).elf32

$(TARGET).elf32: $(TARGET).elf
	$(OBJCOPY) -O elf32-powerpc --adjust-vma 0x80000000 $< $@
	$(STRIP) $@
	@echo "--- Output: $(TARGET).elf32 ---"
	@ls -lh $@

# ── Docker build (for Hyperion) ──

docker-build:
	docker run --rm -v $(CURDIR):/work -w /work $(DOCKER_IMAGE) sh -c \
	    ". /etc/profile.d/99-devkitxenon.sh && \
	     make -f /work/Makefile elf"

# ── Deploy ──

deploy: elf
	cp $(TARGET).elf32 xenon.elf
	@echo "-> Copied $(TARGET).elf32 to xenon.elf (TFTP root)"
	@ls -lh xenon.elf

# ── Clean ──

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).elf32 $(TARGET).map xenon.elf
