# Xbox 360 Supervisor — libxenon build
#
# Targets:
#   make elf        - produce supervisor.elf32 (for USB / TFTP deploy)
#   make embed      - build with embedded test guest
#   make clean      - remove build artifacts
#   make deploy     - build + copy to TFTP root

TARGET       := supervisor
DEVKITXENON  := /usr/local/xenon
DOCKER_IMAGE := free60/libxenon

CROSS        := xenon-
CC           := $(CROSS)gcc
AS           := $(CROSS)as
LD           := $(CROSS)ld
OBJCOPY      := $(CROSS)objcopy
STRIP        := $(CROSS)strip
NM           := $(CROSS)nm

CFLAGS  = -g -O2 -Wall -Werror -Wno-main
CFLAGS += -DXENON -m32 -maltivec -fno-pic -mpowerpc64 -mhard-float
CFLAGS += -mcpu=cell -mtune=cell
CFLAGS += -I$(DEVKITXENON)/usr/include -Iinclude

LDFLAGS  = -n -Wl,-Map,$(TARGET).map
LDFLAGS += -L$(DEVKITXENON)/usr/lib
LDFLAGS += -L$(DEVKITXENON)/xenon/lib/32
LDFLAGS += -Tsupervisor.lds

LIBS = -lxenon -lm

# Core supervisor sources
SOURCES := \
    source/supervisor.c     \
    source/ipc_ring.c       \
    source/core1_engine.c   \
    source/elf_mapper.c

ASM_SOURCES := \
    source/guest_trampoline.S

OBJS := $(SOURCES:.c=.o) $(ASM_SOURCES:.S=.o)

# ── Test guest ──
GUEST_VMA     = 0x80800000
GUEST_TARGET  = test_guest
GUEST_OBJ     = guest_application.o
GUEST_ELF     = guest_application.elf

.PHONY: all elf clean deploy docker-build embed

all: elf

# ── Pattern rules ──

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Supervisor link ──

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

# ── Test guest build and embed ──

$(GUEST_TARGET).elf: source/$(GUEST_TARGET).S $(GUEST_TARGET).lds
	$(CC) $(CFLAGS) -nostartfiles -nostdlib \
	    -T$(GUEST_TARGET).lds \
	    -Wl,-Map,$(GUEST_TARGET).map \
	    -o $@ source/$(GUEST_TARGET).S
	@echo "--- Guest built: $@ ---"
	@ls -lh $@

# objcopy binary -> relocatable object for linking
$(GUEST_OBJ): $(GUEST_TARGET).elf
	cp $< $(GUEST_ELF)
	$(OBJCOPY) -I binary -O elf32-powerpc -B powerpc \
	    $(GUEST_ELF) $@
	@echo "--- Guest embedded: $@ (size: $$($(NM) --size-sort $@ 2>/dev/null | head -1)) ---"

# Supervisor with embedded guest
embed: $(GUEST_OBJ) $(OBJS)
# Build the supervisor object files (excluding elf_mapper if needed)
# but then link WITH the guest object
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET)_with_guest.elf \
	    $(OBJS) $(GUEST_OBJ) $(LIBS)
	@echo "--- Linked $(TARGET)_with_guest.elf ---"
	$(OBJCOPY) -O elf32-powerpc --adjust-vma 0x80000000 \
	    $(TARGET)_with_guest.elf $(TARGET)_with_guest.elf32
	$(STRIP) $(TARGET)_with_guest.elf32
	@echo "--- Output: $(TARGET)_with_guest.elf32 ---"
	@ls -lh $(TARGET)_with_guest.elf32

# ── Docker build ──

docker-build:
	docker run --rm -v $(CURDIR):/work -w /work $(DOCKER_IMAGE) sh -c \
	    ". /etc/profile.d/99-devkitxenon.sh && \
	     make -f /work/Makefile elf"

docker-embed:
	docker run --rm -v $(CURDIR):/work -w /work $(DOCKER_IMAGE) sh -c \
	    ". /etc/profile.d/99-devkitxenon.sh && \
	     make -f /work/Makefile embed"

# ── Deploy ──

deploy: elf
	cp $(TARGET).elf32 xenon.elf
	@echo "-> Copied $(TARGET).elf32 to xenon.elf (TFTP root)"
	@ls -lh xenon.elf

# ── Clean ──

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).elf32 $(TARGET).map xenon.elf
	rm -f $(GUEST_TARGET).elf $(GUEST_TARGET).map $(GUEST_OBJ) $(GUEST_ELF)
	rm -f $(TARGET)_with_guest.elf $(TARGET)_with_guest.elf32
