# Q4 notification service slice — 2026-07-22

## Status and scope

The first Q4 slice is complete at the service boundary. It adds a runtime-owned
notification service, deterministic injection through the central event path,
bounded local/external ingress, action dispatch, and host pressure/fault tests.

This document does **not** mark the C Gate complete. Notification center UI,
overlay timeout and input capture, incoming-call UI, haptic logging, synthetic
wake consumption, and real workout/UI integration remain open.

## Frozen contracts

- The existing 16-entry notification model remains the only business queue.
- Received events own bounded copies of ID, type, priority, source, title, body,
  and wall timestamp; no caller string pointer crosses the event boundary.
- Demo, local host/simulator, and locked external ingress all build the same
  received event. UI actions build the same bounded action event.
- Exact duplicate ID and content is a no-op and does not replay presentation.
  A same-ID, same-type content change remains a model update and may present
  again while preserving read/dismiss/action state.
- DND stores the item in the center model but creates no presentation. Workout
  policy downgrades Call/high-priority presentation to non-blocking overlay.
- Presentation uses retry-safe `peek` plus explicit `ack`; an unacknowledged
  Call cannot be replaced by App/System presentation.
- READ, DISMISS, ACCEPT, REJECT, and DELETE travel through the action event.
- Main event queue and locked external inbox admit notifications by priority;
  a Call is at least HIGH, Critical remains CRITICAL, and malformed payloads
  are LOW so they cannot evict valid control/notification work.
- Runtime dispatch preserves notification receive/action insertion order.

## Verification

Source commits:

- `d658b031a9e6dd3e7aabd3705419c46b0bcddb4e` — service/runtime implementation
- `54273fb687b353175536e46bd6550979c0c7e93e` — coverage and malformed-priority
  hardening

Pull request: <https://github.com/918154429/smart-band-demo/pull/14>

Final checks for this slice:

- Host run: <https://github.com/918154429/smart-band-demo/actions/runs/29899475732>
- Browser run: <https://github.com/918154429/smart-band-demo/actions/runs/29899475681>
- GCC, Clang, and MSVC strict builds passed for notification service, central
  runtime, and complete UI compile smoke.
- Production C core line coverage: `92.7% (4575/4936)`.
- `runtime.c`: `90.0% (298/331)`.
- `notification_service.c`: `95.1% (173/182)`.
- The local non-coverage host/evidence/UI Python suite passed in full.

The notification service test covers 1000 deterministic mixed receives,
exact duplicates, 4096-byte input truncation, full/protected model recovery,
Accept/Reject/Delete, DND/workout policy, retry-safe presentation, queue and
inbox saturation, wraparound eviction, malformed priority/type, lock failure,
and post-failure recovery.

## Resource snapshot

MSVC x64 host ABI measurements:

- `smart_band_event_t`: 232 bytes
- `smart_band_event_queue_t`: 3736 bytes
- `smart_band_event_inbox_t`: 3768 bytes
- `smart_band_notification_service_t`: 3792 bytes
- Queue + inbox + service: 11296 bytes

These host sizes are a regression sentinel, not target-board RAM proof. The
OpenVela/NuttX ABI, final ELF/map delta, task stack peak, and Gemini-S1 budget
remain unverified.

## Explicit remaining work

1. Build notification center, ordinary overlay, full-screen incoming Call, and
   full-screen input capture in fake LVGL/native UI.
2. Move visual presentation to a service-owned monotonic timeout lifecycle.
3. Separate visual acknowledgement from one-shot haptic and wake effects with
   generation/token semantics; add the simulator structured haptic log.
4. Install a real application event lock and a 50–100 ms event pump before any
   external producer is enabled.
5. Derive workout foreground policy from real runtime/UI state and verify that
   notification handling never pauses or misroutes workout controls.
6. Add explicit-length, UTF-8-safe host/BLE adapter validation before F-stage
   length-delimited input is accepted.
7. Revisit the roughly 11.3 KiB host static footprint with target ABI/map data.

