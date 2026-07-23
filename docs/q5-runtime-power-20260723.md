# Q5 runtime power policy slice — 2026-07-23

## Status and scope

The Q5 runtime power **software-policy slice** is complete at the host-tested
runtime/platform boundary. It adds an application runtime-owned power manager,
the `ACTIVE` / `DIMMED` / `SCREEN_OFF` policy, typed wake events, state-specific
render and sensor schedules, workout-aware checkpoint and sampling gates, a
sync permission gate, and display/backlight lifecycle restoration.

This result is not a measured-power result. No Gemini-S1 firmware was flashed,
no physical display, sensor, or radio current was sampled, and no battery-life
claim is made. The actual-power **Gate E remains red** until target-side current
and energy measurements satisfy its acceptance criteria.

## Frozen runtime contracts

- The runtime power manager is the only owner of notification wake effects.
  The flow is `notification_service_peek_wake()` -> accumulated power-event
  bitmask -> `smart_band_power_manager_handle()` ->
  `notification_service_ack_wake()` only after the policy accepts the event.
  The application presentation layer consumes haptic effects only and does not
  infer or acknowledge wake from presentation generation.
- All wake sources observed in one dispatch are ORed into one bitmask before a
  single policy application. The frozen selection priority is button > touch >
  notification > wrist > charging; a timeout tick cannot replace a real wake.
- An ordinary overlay with `wake_request=false` remains on `SCREEN_OFF` and
  does not synthesize a wake. A pending notification wake is acknowledged only
  after the runtime power consumer accepts it.
- Idle `SCREEN_OFF` disables motion sampling and workout checkpoints, uses a
  5000 ms heart-sampling period, and denies sync. Workout `SCREEN_OFF` retains
  motion sampling and checkpoints, uses a 1000 ms heart-sampling period, and
  still denies sync.
- The application render path calls `smart_band_runtime_render_due()` before
  consuming dirty state. Urgent presentation can bypass the cadence gate. The
  50 ms event pump remains independent of the 1000 ms business tick.
- Runtime deinitialization restores an off display to enabled/100% brightness
  through the normal power adapter before the manager is reset.
- Saturating render and heart deadlines fire once at `UINT64_MAX` and then stop;
  they do not remain permanently due after clock saturation.

## Verification

Commits:

- Implementation: `c49929b18cc21b88cc3ababc516a63fbe944193e` — runtime
  power manager, policy integration, sampling masks, scheduling, and host tests.
- Q4 integration: `9ba5e9069a794eca5b7ad6a14e141c46a6b8492e` — merges the
  final notification lifecycle and assigns notification wake exclusively to
  the runtime power manager.

Pull request: <https://github.com/918154429/smart-band-demo/pull/17>

Final checks for integration commit `9ba5e90`:

- Host run: <https://github.com/918154429/smart-band-demo/actions/runs/29973330134>
  — 58/58 jobs succeeded.
- Browser run: <https://github.com/918154429/smart-band-demo/actions/runs/29973330198>
  — 1/1 job succeeded.
- Aggregate PR result: 59/59 checks succeeded; PR merge state was `CLEAN`.
- Relevant strict matrix jobs include Power policy and Power manager under GCC,
  Clang, and MSVC, Central runtime under all three compilers, UI compile smoke
  under all three compilers, and the production C coverage gate.

The committed host tests specifically verify:

- each typed wake source and the combined-event priority chain, including
  button winning over every other source, touch winning over notification,
  wrist, and charging, and notification winning over wrist and charging;
- runtime coalescing of notification, wrist, and touch in one dispatch, with
  touch selected and the notification wake acknowledged by the sole owner;
- a normal no-wake overlay does not wake `SCREEN_OFF`, while a notification-only
  wake does;
- idle `SCREEN_OFF` omits `SMART_BAND_SENSOR_SAMPLE_MOTION`, rejects a queued
  checkpoint, and returns false from `smart_band_runtime_allows_sync()`;
- workout `SCREEN_OFF` includes motion, accepts a queued checkpoint, retains
  the 1000 ms heart cadence, and still denies sync;
- the off-state render schedule produces fewer than one render for every ten
  50 ms pump opportunities over 60 seconds, proving a host-scheduler reduction
  greater than 90%; this is not a physical power measurement;
- 1000 ACTIVE -> DIMMED -> SCREEN_OFF -> ACTIVE cycles produce exactly 3000
  state transitions without a pending platform application;
- deinit from `SCREEN_OFF` restores display enabled and 100% brightness;
- render and heart schedules behave correctly through `UINT64_MAX` saturation.

## Evidence boundaries and remaining Gate E work

1. The render reduction is a deterministic host scheduler assertion, not an
   LVGL frame-time, display-current, MCU sleep-residency, or battery measurement.
2. Platform adapter `OK`, retry/degraded, and lifecycle behavior is host tested;
   target driver wiring and physical display/backlight behavior remain unproved.
3. `smart_band_runtime_allows_sync()` is the Q5 policy boundary. Each later sync
   transport/driver must consult it; this slice does not claim live BLE radio
   suppression or energy savings.
4. Workout sampling/checkpoint behavior is policy and host-runtime evidence;
   it does not validate a physical motion or heart sensor in low-power states.
5. Gate E stays red until target instrumentation records reproducible state
   currents, transition energy, radio impact, sleep residency, and battery-life
   projections with an explicit setup and acceptance receipt.
