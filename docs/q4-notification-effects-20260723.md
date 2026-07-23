# Q4 notification effects and input-capture slice — 2026-07-23

## Status and scope

The fourth Q4 slice is complete at the host-verified application effect and UI
boundary. The application now consumes service-owned haptic and synthetic-wake
effects, drives the existing platform haptic adapter with retry-safe generation
acknowledgement, records structured simulator fallback markers, and closes the
remaining DND, long-text, same-ID update, and full-screen Call input-capture
journeys.

The Q4 C Gate remains **open**. This evidence is host and fake-LVGL evidence; it
does not claim physical vibration, a real display wake or power transition,
BLE behavior, or Gemini-S1 target execution.

## Frozen effect contracts

- Haptic consumption starts from
  `smart_band_notification_service_peek_haptic()`. The notification ID and
  non-zero generation returned by the service remain the acknowledgement key.
- `SUBTLE`, `NORMAL`, and `URGENT` map to one, two, and three bounded platform
  pulses respectively. The application calls the existing
  `runtime.platform.haptic.ops->play` adapter before acknowledging the effect.
- `SMART_BAND_PLATFORM_OK` acknowledges the exact haptic generation without a
  simulator haptic marker.
- `SMART_BAND_PLATFORM_BUSY` and `SMART_BAND_PLATFORM_IO` retain that exact
  generation and retry it on a later 50 ms application pump. They are not
  silently acknowledged or replaced by a new effect.
- `SMART_BAND_PLATFORM_UNAVAILABLE` emits the structured
  `smart_band:q4:haptic:v1` simulator marker and then acknowledges the exact
  generation. Other invalid adapter states remain pending rather than being
  reported as a successful fallback.
- Structured logging is best effort after the platform/fallback decision.
  Logger failure does not replay an otherwise completed effect; it is exposed
  through `haptic_log_dropped` or `wake_log_dropped` diagnostics.
- Synthetic wake is consumed only through the notification service
  `peek_wake`/`ack_wake` contract. It is not inferred from a visible overlay or
  presentation generation. The Q4 marker declares
  `reason=notification synthetic=1 power_transition=0`; Q5, not this slice,
  owns future power-state transitions and must become the sole wake-effect
  owner when integrated.

## UI and input behavior verified

- A same-ID content change replaces the visible content, allocates a new effect
  generation, and emits one new haptic/wake pair. Posting the exact updated
  content again does not replay those effects.
- Long notification text is truncated to the bounded model capacity and stays
  inside the overlay geometry. This verifies current C-string ingress only; it
  is not explicit-length UTF-8 ingress proof.
- With DND enabled, a retained critical notification appears in Notification
  Center without an overlay, haptic, or wake effect.
- A non-workout Call remains a full-screen foreground capture layer. Corner and
  lower-screen clicks plus horizontal press/release input land on that layer
  and cannot navigate the covered page; Accept promotes the queued Call and
  Reject removes it.
- During a live workout, the Call remains the ordinary non-blocking card and
  the uncovered Pause control remains usable.
- Application icons, system icons, Back, step-goal actions, page dots, and page
  drag handling honor the full-screen notification capture predicate.

## Verification

Implementation commit:

- `9a5ab901a8e15e81cf61e8de98ba7a888687f592` — platform haptic result
  handling, structured effect logging and diagnostics, service-owned synthetic
  wake consumption, DND/long-text/same-ID journeys, and complete Call capture
  guards.

Integration commit:

- `f6185d402b9847210e69e228ce51e95205ce1c3b` — ordinary merge of current
  `origin/master` into the Q4 branch after the implementation commit.

Pull request: <https://github.com/918154429/smart-band-demo/pull/14>

Final checks against the integration commit:

- Host run: <https://github.com/918154429/smart-band-demo/actions/runs/29972675675>
- Browser run: <https://github.com/918154429/smart-band-demo/actions/runs/29972675670>
- Combined: 56/56 jobs succeeded across the Host and Browser runs.
- Host: 55/55 jobs succeeded, including GCC/Clang notification and central
  runtime tests, MSVC `/std:c11 /W4 /WX` production UI compile/journeys, and
  the production coverage gates.
- Browser: the Chromium job succeeded (1/1).
- The host UI journey covers adapter `OK`, `BUSY` then `OK`, `IO` then `OK`,
  `UNAVAILABLE`, one/two/three-pulse mappings, structured fallback, logger
  failure counters, the service wake contract, DND retention, bounded long
  text, same-ID content replacement, and full-screen/workout input policy.

## Evidence boundaries and remaining C Gate work

1. A reviewed native notification journey remains required; fake-LVGL
   coordinate/input journeys are not target display or input proof.
2. Explicit-length UTF-8 validation at adapter ingress remains open. Bounded
   current C-string truncation is not equivalent to UTF-8-safe length handling.
3. Cross-inbox/main-queue total arrival ordering remains undefined. The current
   ordering guarantee begins after inbox events enter the main queue.
4. Target ELF/map sizing and task-stack evidence remain required.
5. No physical vibration, real wake/power transition, power consumption, BLE
   transport, or Gemini-S1 hardware result is claimed by this slice.
