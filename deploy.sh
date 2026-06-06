#!/usr/bin/env bash
# deploy.sh — build supervisor (Docker), copy to USB, unmount
# Usage: ./deploy.sh
# Requires: USB stick in FAT32, Docker, sudo for mount

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
MOUNT_POINT="/mnt/usb-red"
ELF_NAME="xenon.elf"

# --- Wait for USB -----------------------------------------------------------
echo "[*] Waiting for a removable USB block device..."

while true; do
    for dev in /sys/block/*/removable; do
        [ -f "$dev" ] || continue
        if [ "$(cat "$dev" 2>/dev/null)" = "1" ]; then
            DEVICE="/dev/$(basename "$(dirname "$dev")")"
            if [ -b "$DEVICE" ]; then
                echo "[+] Found removable device: $DEVICE"
                break 2
            fi
        fi
    done
    sleep 1
done

# Use partition 1 if available
PART="${DEVICE}1"
[ -b "$PART" ] && DEVICE="$PART"

# --- Mount ------------------------------------------------------------------
echo "[*] Mounting $DEVICE to $MOUNT_POINT..."
sudo mkdir -p "$MOUNT_POINT"
sudo mount "$DEVICE" "$MOUNT_POINT"
echo "[+] Mounted at $MOUNT_POINT"

# --- Build (Docker) ---------------------------------------------------------
echo "[*] Building supervisor in Docker..."
cd "$PROJECT_DIR"
docker run --rm \
    -v "$(pwd):/work" -w /work \
    free60/libxenon sh -c \
    ". /etc/profile.d/99-devkitxenon.sh && \
     make -f /work/Makefile elf 2>&1"
echo "[+] Build complete"

# --- Copy to USB as xenon.elf ------------------------------------------------
echo "[*] Copying supervisor.elf32 to USB as $ELF_NAME..."
sudo cp supervisor.elf32 "$MOUNT_POINT/$ELF_NAME"
sync
echo "[+] Copied: $(ls -lh "$MOUNT_POINT/$ELF_NAME" | awk '{print $5}')"

# --- Unmount ----------------------------------------------------------------
echo "[*] Unmounting..."
sudo umount "$MOUNT_POINT"
echo "[+] Safe to remove USB. ($MOUNT_POINT unmounted)"
