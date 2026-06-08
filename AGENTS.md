# Deploy naming convention

  supervisor.elf32  ->  /mnt/usb-red/xenon.elf
  apps/demo/demo.elf32      -> /mnt/usb-red/payload.elf (legacy, unused for dashboard)
  apps/dashboard/app.elf    -> /mnt/usb-red/apps/xenolith.dashboard/app.elf

The bootloader loads `xenon.elf` (supervisor). The supervisor auto-launches
the discovered app with id `xenolith.dashboard` on boot (loads
`uda:/apps/xenolith.dashboard/app.elf`). Guide menu "Load from USB" still
loads `uda:/payload.elf` as a fallback.

# USB deploy (from this directory)

sudo mount /dev/sda /mnt/usb-red 2>/dev/null
sudo cp supervisor.elf32 /mnt/usb-red/xenon.elf
sudo mkdir -p /mnt/usb-red/apps/xenolith.dashboard
sudo cp apps/dashboard/app.elf /mnt/usb-red/apps/xenolith.dashboard/app.elf
sudo cp apps/dashboard/manifest.json /mnt/usb-red/apps/xenolith.dashboard/manifest.json
sync

# App discovery

Supervisor scans `uda:/apps/*/manifest.json` after `fatInitDefault()`
via `opendir()` + `readdir()` (libfat devoptab). Results are cached in
`AppMeta app_cache[MAX_APPS]` (BSS). Dashboard queries via SVC_GET_APP_COUNT
and SVC_GET_APP_INFO service calls.

# Directory structure (USB)

uda:/
├── apps/
│   └── <app-id>/
│       ├── manifest.json   # { id, name, author, version }
│       ├── app.elf         # the app binary
│       └── icon.bmp        # v1: standard BMP (future)
├── payload.elf             # legacy single-app path (Guide → Load from USB)
└── settings.json           # future: persistent settings

# Auto-launch

After `scan_apps()`, main() calls `launch_app_by_id("xenolith.dashboard", ...)`.
If found and loaded successfully, `send_exec_guest()` starts it on Core 1.
The dashboard then uses `svc_get_app_count()` / `svc_get_app_name()` (IPC round-trips)
to discover apps — including itself. Guide button pauses the guest, opens menu,
and on resume sends STATE_RESUME to Core 1.
