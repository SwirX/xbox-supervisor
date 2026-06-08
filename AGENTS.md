# Deploy naming convention

  supervisor.elf32  ->  /mnt/usb-red/xenon.elf
  guest/payload.elf32 -> /mnt/usb-red/payload.elf32

The bootloader loads `xenon.elf` (supervisor). The supervisor loads
`uda:/payload.elf32` (guest) from USB at runtime.

# USB deploy (from this directory)

sudo mount /dev/sda /mnt/usb-red 2>/dev/null
sudo cp supervisor.elf32 /mnt/usb-red/xenon.elf
sudo cp guest/payload.elf32 /mnt/usb-red/payload.elf32
sync
