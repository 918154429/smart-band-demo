# Q4 notification UI and application pump slice — 2026-07-22

## Status and scope

The third Q4 slice is complete at the application/LVGL boundary. It installs an
application-owned cross-platform event mutex, adds an independent 50 ms event
pump, and renders Notification Center, ordinary overlay, incoming Call, and
workout-aware Call presentation through the existing notification service.

This does **not** complete the C Gate. Structured simulator haptic logging,
synthetic wake consumption, DND/long-text/content-update UI journeys, Q4 native
diagnostics and visual evidence, explicit-length UTF-8 ingress, and target
resource proof remain open.

## Frozen application and UI contracts

- The application binds the event lock only after the storage platform has
  finished selecting its configured backend or fallback. Windows uses a
  `CRITICAL_SECTION`; NuttX/POSIX uses `pthread_mutex_t`.
- The existing 1000 ms runtime/business timer is unchanged. A separate 50 ms
  timer only calls `smart_band_runtime_dispatch_pending()` and
  `render_pending()`; it does not advance business time.
- `SMART_BAND_DIRTY_NOTIFICATION` is consumed independently of the current page
  dirty mask, so presentation is not lost while a system/app view is active.
- `smart_band_lvgl_post_notification_external()` is the application ingress for
  concurrent producers. It is thread-safe only while the created application
  is alive. Every producer must be stopped and joined before
  `smart_band_lvgl_destroy()`; ingress and teardown are deliberately not
  claimed to be concurrently safe.
- Notification Center is a lazy system view with four newest-first rows per
  page, Previous/Next paging, Mark read, and Delete actions.
- An ordinary overlay remains for the service-owned 5000 ms lifetime or until
  Dismiss. Its visible card absorbs clicks and swipe starts that would otherwise
  reach covered controls; the rest of the screen remains non-blocking.
- A non-workout Call is an opaque, clickable, foreground full-screen layer.
  Accept/Reject completes the current Call and promotes the next retained Call.
  The layer captures corner, bottom-area, click, and horizontal-drag input.
- During a live workout, Call presentation is downgraded to the ordinary card,
  so uncovered workout controls remain usable. The tested Pause control can be
  activated by coordinate hit testing while the Call card is visible.
- Presentation rendering reads the service-owned active generation, updates the
  LVGL layer, then uses `peek` to decide whether visual acknowledgement is still
  valid. Haptic and wake acknowledgement remain independent.
- The Apps launcher now exposes Notifications. The 11 launcher entries remain
  reachable within both the 320 x 480 compact and 667 x 375 framed test trees.

## Verification

Source commit:

- `71e7e32a6f8bfd57c4481917cfbfbdcbfee8cafd` — application mutex, 50 ms pump,
  Notification Center, ordinary overlay, full-screen Call, input isolation, and
  fake-LVGL journey coverage

Pull request: <https://github.com/918154429/smart-band-demo/pull/14>

Final checks for the source commit:

- Host run: <https://github.com/918154429/smart-band-demo/actions/runs/29906060140>
- Browser run: <https://github.com/918154429/smart-band-demo/actions/runs/29906060196>
- Host: 55/55 jobs succeeded, including GCC and Clang UI compile smoke, MSVC
  `/std:c11 /W4 /WX` full production UI compile/journey, notification core and
  service tests, central runtime tests, and the production coverage gate.
- Browser: the Chromium job succeeded.
- Production C core line coverage remained `92.9% (4838/5207)`.
- `event_queue.c`: `98.3% (113/115)`.
- `runtime.c`: `90.3% (315/349)`.
- `notification_service.c`: `94.8% (401/423)`.

The application/LVGL journey specifically verifies:

- four native threads complete 8000 mutex-protected increments;
- after 999 ms there are 19 event pumps and zero runtime ticks, while the next
  1 ms produces the twentieth pump and first runtime tick;
- external ingress is rejected before application creation and after destroy;
- an external notification is absent at 49 ms and visible at 50 ms;
- an ordinary overlay remains at 4999 ms and expires at 5000 ms;
- exact duplicate content does not replay a dismissed presentation;
- Center newest-first paging, Mark read, Delete, and every object-creation
  failure point roll back and retry without a resource leak;
- Accept promotes the next Call, Reject removes it, and the full-screen layer
  captures screen corners, bottom input, clicks, and horizontal drag;
- a workout Call leaves the uncovered Pause control reachable;
- application creation failure is swept at both supported test resolutions;
- failure of either timer creation rolls back cleanly and a later create can
  retry;
- 1000 create/destroy/navigation iterations finish with no net LVGL object or
  event growth; idle diagnostics report no root, timer, runtime, or mutex left.

The fake LVGL timer scheduler now advances timers by global deadline, so large
virtual-time steps preserve the ordering of the 50 ms pump and 1000 ms runtime
timer instead of draining one timer at a time.

## Evidence boundaries and remaining work

1. The producer stop/join rule is a required lifecycle contract, not an
   implementation of concurrent ingress-versus-destroy draining.
2. Host mutex contention and fake-LVGL input journeys do not prove NuttX task
   scheduling, target display/input behavior, or Gemini-S1 hardware behavior.
3. No structured haptic log or synthetic wake consumer is connected yet; no
   physical vibration, wake, BLE transport, or power-manager behavior is
   claimed.
4. DND, long-text truncation, and same-ID content-update behavior are covered in
   the service layer but still need end-to-end UI journeys.
5. Native Q4 diagnostics, reviewed native screenshots, explicit-length
   UTF-8-safe adapter ingress, cross-inbox/main total ordering, and target
   ELF/map/stack evidence remain open.
