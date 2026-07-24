# Q4 notification lifecycle slice — 2026-07-22

## Status and scope

The second Q4 slice is complete at the notification service and central runtime
boundary. It owns visual lifetime, separates visual/haptic/wake acknowledgement,
queues overlapping Calls, derives workout policy from the live workout service,
and preserves workout/notification insertion order in the main event queue.

This does **not** complete the C Gate. Notification center UI, ordinary overlay
and incoming-call LVGL layers, input capture, application event locking and fast
pump, simulator haptic logging, synthetic wake consumption, and native visual
evidence remain open.

## Frozen lifecycle contracts

- Ordinary non-Call overlays expire after 5000 ms using the service monotonic
  clock. The unsigned half-range comparison is safe across 32-bit wrap.
- A Call has no visual deadline, including a workout-mode Call overlay. It stays
  active until READ, DISMISS, ACCEPT, REJECT, DELETE, or DND suppression.
- Every presentation has a non-zero generation that is not reused while it is
  active in visual, effect, or Call-backlog state.
- `peek_presentation` returns only an unacknowledged visual generation.
  `get_active_presentation` remains available until lifecycle removal.
- Visual, haptic, and wake consumers acknowledge independently with ID plus
  generation. A visual success cannot discard an unconsumed haptic or wake.
- Pending effects are bounded to 16 entries. Same-ID updates replace the older
  effect; model eviction removes its effect; defensive saturation prefers higher
  priority and Call effects at equal priority.
- A Call is normalized to at least HIGH both in the trusted event builder and
  again at service processing, so a malformed LOW Call cannot lose model
  protection or leave an orphan full-screen presentation.
- A second Call is retained in a bounded backlog. Completing the active Call
  promotes the next still-valid Call without replaying already acknowledged
  effects.
- Central runtime dispatch handles workout commands/checkpoints and notification
  receive/actions in their physical main-queue insertion order. Notification
  policy is derived from `smart_band_workout_service_is_live()` immediately
  before each notification.

## Verification

Source commits:

- `4cf163ad5e8d81e604387c1eb966580802421928` — lifecycle, effects, Call backlog,
  priority normalization, and ordered runtime dispatch
- `394662e970881b601595a1ec360ba1e72bc2256f` — event-queue coverage closure

Pull request: <https://github.com/918154429/smart-band-demo/pull/14>

Final checks:

- Host run: <https://github.com/918154429/smart-band-demo/actions/runs/29902320055>
- Browser run: <https://github.com/918154429/smart-band-demo/actions/runs/29902320024>
- Host: 55/55 jobs succeeded across GCC, Clang, and MSVC strict builds, complete
  UI compile smoke, and evidence harnesses.
- Production C core line coverage: `92.9% (4838/5207)`.
- `event_queue.c`: `98.3% (113/115)`.
- `runtime.c`: `90.3% (315/349)`.
- `notification_service.c`: `94.8% (401/423)`.
- Local MSVC notification service, central runtime, and complete UI compile
  tests passed with `/std:c11 /W4 /WX`.

The first run at commit `4cf163a` intentionally remains as negative evidence:
Host run `29901995462` failed only because changing `event_queue.c` activated its
per-file 90% gate at 80.0% (92/115). No production code or threshold was
excluded or weakened; added priority, selection, success, empty, and invalid
queue tests raised the final file result to 98.3%.

## Resource snapshot

MSVC x64 host ABI measurements:

- `smart_band_event_t`: 232 bytes
- `smart_band_event_queue_t`: 3736 bytes
- `smart_band_event_inbox_t`: 3768 bytes
- `smart_band_notification_service_t`: 4344 bytes
- Queue + inbox + service: 11848 bytes

The service grew by 552 bytes from the first Q4 slice for bounded effect and
Call-backlog state. These host values are regression sentinels, not OpenVela or
Gemini-S1 RAM proof; final ELF/map and task stack evidence remain required.

## Explicit remaining work

1. Install an application-owned event mutex and a 50–100 ms event pump before
   enabling a real external producer.
2. Render `SMART_BAND_DIRTY_NOTIFICATION` independently of the current page and
   implement notification center, ordinary overlay, full-screen Call, and input
   capture.
3. Connect haptic consumption to a structured simulator log and connect wake to
   the future power manager without claiming physical vibration or wake proof.
4. Add native UI journeys for timeout, input isolation, Accept/Reject, long text,
   DND, workout non-blocking behavior, and consecutive Calls.
5. Define a cross-inbox/main-queue sequence contract if producers require total
   arrival order across both queues; the current guarantee starts after inbox
   drain into the main queue.
6. Keep explicit-length UTF-8 ingress validation and target ELF/map/stack proof
   open for later adapters and board work.
