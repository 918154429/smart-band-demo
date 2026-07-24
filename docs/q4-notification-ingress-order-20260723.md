# Q4 explicit UTF-8 ingress and cross-queue ordering — 2026-07-23

## Outcome

The remaining host-side notification ingress and ordering gaps are closed.
External adapters now provide explicit byte lengths, malformed UTF-8 and
embedded NUL bytes are rejected, and bounded text is truncated only at a
complete code-point boundary. Main-queue and locked-inbox producers share one
64-bit admission sequence, including wrap handling and equal-priority pressure.

This slice does not close the Q4 C Gate. Reviewed native notification visuals
and target ELF/linker-map/task-stack evidence remain required.

## Explicit-length UTF-8 contract

- `smart_band_notification_utf8_input_t` carries an explicit span for source,
  title, and body. A non-empty span requires a non-null pointer; an empty span
  may use a null pointer.
- The public concurrent LVGL ingress and runtime external ingress accept only
  the explicit-span type. The C-string helper remains restricted to trusted
  UI-thread/internal demo use.
- Validation rejects stray continuation bytes, invalid lead bytes, truncated
  sequences, two/three/four-byte overlong forms, UTF-16 surrogate code points,
  values above U+10FFFF, and embedded NUL bytes.
- The complete input is validated even after the bounded destination fills.
  Once a code point does not fit, the output remains the longest valid input
  prefix; later shorter bytes cannot create a non-prefix result.
- Invalid adapter input clears the temporary event and never reaches the inbox,
  notification model, presentation, haptic, or synthetic wake path.

## Cross-inbox/main ordering contract

- The runtime-owned external inbox owns the next non-zero 64-bit ingress
  sequence. Both locked external posts and UI-thread main posts allocate their
  sequence through that owner.
- Domain dispatch selects the oldest sequence after inbox drain, so an earlier
  external receive cannot be overtaken by a later local action or workout
  command merely because it was copied into the main queue later.
- Sequence comparison is modular across `UINT64_MAX -> 1`; zero remains the
  compatibility value for standalone queue tests that bypass runtime ingress.
- If the main queue is full and an older equal-priority inbox event arrives,
  the newest later event at that priority is evicted. A normal newer event is
  still dropped under the pre-existing pressure policy.
- Priority admission remains intentional: a later higher-priority control may
  evict a lower-priority event. Total order defines dispatch of surviving
  domain events; it does not remove the documented priority/backpressure rules.

## Verification

Local Windows verification used MSVC C11 `/W4 /WX`:

- notification service: passed;
- central runtime: passed;
- complete fake-LVGL production UI: passed;
- remaining non-coverage host harnesses: passed;
- Browser/Chromium: `7/7` passed;
- `git diff --check`: passed.

Independent Linux verification used the outer development host
`ubuntu24-hushen`, isolated at
`/data/smart-band-q4-ingress-20260723T1047CST`:

- GCC C11 `-Wall -Wextra -Werror -pedantic` notification service: passed;
- GCC central runtime: passed;
- GCC complete fake-LVGL production UI: passed;
- overall production line coverage: `93.0% (4972/5346)`;
- `services/event_queue.c`: `97.5% (156/160)`;
- `services/event_inbox.c`: `98.9% (91/92)`;
- `services/notification_service.c`: `94.9% (465/490)`.

GitHub CI for commit `cc0be669d859908b1beb672aaebbf6961d73f640`
completed successfully:

- Host tests run
  [`29975646888`](https://github.com/918154429/smart-band-demo/actions/runs/29975646888):
  all GCC, Clang, MSVC, coverage, UI compile, and evidence jobs passed;
- Browser tests run
  [`29975646864`](https://github.com/918154429/smart-band-demo/actions/runs/29975646864):
  Chromium passed.

The regression matrix includes non-NUL-terminated valid spans, empty spans,
ASCII and valid two/three/four-byte sequences, exact and partial capacity
boundaries, invalid UTF-8 classes, embedded NUL, the external-receive/local-
action inversion, `UINT64_MAX -> 1`, lock failure/close recovery, and a full
mixed inbox/main queue where the oldest external notification must survive.

Host ABI sizes after adding the sequence are: event `240` bytes, main queue
`3864` bytes, inbox `3904` bytes, notification service `4344` bytes, and their
queue+inbox+service subtotal `12112` bytes. These are host measurements, not
OpenVela/Gemini-S1 target ABI or RAM proof.

## Remaining C Gate work

1. Add and run a deterministic native notification journey, audit the artifact,
   and manually review its overlay, Call, Notification Center, DND, long-text,
   same-ID update, input-isolation, and workout-nonblocking screenshots.
2. Collect target ELF, real linker map when emitted, and task stack high-water
   evidence. Instrumented host sizes and fake-LVGL journeys cannot replace it.

Simulator haptic and wake markers remain synthetic and do not prove physical
vibration, display wake, BLE transport, or target-board behavior.
