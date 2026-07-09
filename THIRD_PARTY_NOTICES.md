# Third-Party Notices

This file lists third-party technologies that the smart band demo is designed
to build or run with. The third-party projects listed below are not vendored
or redistributed by this repository unless explicitly noted.

## openvela

- Role: target operating system, build system integration, board configuration,
  packages demo framework, simulator workflow.
- Distribution in this repository: not included.
- License note: the local SDK used during development includes Apache License
  2.0 text in `apps/LICENSE`, `packages/LICENSE` and related upstream license
  files. Use the exact license and notice files from the openvela SDK used by
  the final submission platform.

## Apache NuttX

- Role: RTOS layer, shell command model, device files, sensor/uORB interfaces,
  battery ioctl definitions and application registration.
- Distribution in this repository: not included.
- License note: the local SDK contains `nuttx/LICENSE` with Apache License 2.0
  and `nuttx/NOTICE` with Apache NuttX notices.

## LVGL

- Role: UI toolkit used by `smart_band_main.c`, `app_lvgl.c`, and every app
  view under `openvela_app/smart_band/apps`.
- Distribution in this repository: not included.
- License note: the local SDK contains `apps/graphics/lvgl/lvgl/LICENCE.txt`
  with MIT license text for LVGL.

## libuv

- Role: event loop integration through LVGL's NuttX/libuv adapter.
- Distribution in this repository: not included.
- License note: the local SDK contains `apps/system/libuv/libuv/LICENSE` with
  MIT license text for libuv.

## Android Emulator / QEMU

- Role: goldfish simulator used to run and demonstrate the application.
- Distribution in this repository: not included.
- Compliance note: emulator binaries, skins, QEMU components and their license
  files belong to the openvela/prebuilt SDK installation, not this repository.

## Browser demo runtime

- Role: the `demo` folder can be opened directly in a browser for screenshots
  and recording when a simulator or board is not available.
- Distribution in this repository: HTML, CSS and JavaScript authored for this
  project are included.
- Third-party libraries: none. The demo uses only standard browser APIs.

## Project images and icons

- Role: health icons, app icons and the electronic wooden fish illustration.
- Distribution in this repository: included under
  `openvela_app/smart_band/assets`.
- License: distributed as part of this project under the repository `LICENSE`
  unless a downstream platform requires a different policy.
- Compliance note: these are project-owned assets and do not intentionally
  reproduce third-party trademarks, brand marks or product logos.

## Summary

The smart band demo source code and project assets in this repository are
provided under the MIT License. Upstream operating system, UI, simulator and
toolchain components are referenced as build/runtime dependencies only and
must be accompanied by their own upstream notices when distributed together.
