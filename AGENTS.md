# Deploy naming convention

  supervisor.elf32  ->  /mnt/usb-red/xenon.elf
  apps/demo/demo.elf32 -> /mnt/usb-red/payload.elf

The bootloader loads `xenon.elf` (supervisor). The supervisor loads
`uda:/payload.elf` (guest) from USB at runtime.

# USB deploy (from this directory)

sudo mount /dev/sda /mnt/usb-red 2>/dev/null
sudo cp supervisor.elf32 /mnt/usb-red/xenon.elf
sudo cp apps/demo/demo.elf32 /mnt/usb-red/payload.elf
sync

# App discovery

Supervisor scans `uda:/apps/*/manifest.json` after `fatInitDefault()`
via `opendir()` + `readdir()` (libfat devoptab). Results are cached in
`AppMeta app_cache[MAX_APPS]` (BSS). No service call yet; the dashboard
will query via SVC_GET_APP_LIST later.

# Directory structure (USB)

uda:/
├── apps/
│   └── <app-id>/
│       ├── manifest.json   # { id, name, author, version }
│       ├── app.elf         # the app binary (loaded later)
│       └── icon.bmp        # v1: standard BMP (future)
├── payload.elf             # loaded by supervisor (legacy single-app path)
└── settings.json           # future: persistent settings
