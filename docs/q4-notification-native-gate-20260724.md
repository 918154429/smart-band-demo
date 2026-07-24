# Q4 notification native C Gate — 2026-07-24

## Decision

Q4 functional C Gate is **passed at the software/Goldfish level** for source commit
`743ff28a32e222255f39c51c89921cc29a16ab31`.

This decision combines the Host/Browser suites, a fresh pinned OpenVela build,
the four-boot native Q4 journey, two independently recomputed evidence
manifests, linker evidence and manual review of all eight native screenshots.
The machine-readable summary is
`docs/evidence/q4-gate-summary-20260724.json`.

This is a short, deterministic four-scenario journey, not the roadmap's
30-minute per-slice soak. Long-duration Q4 stability, the two-hour emulator RC
and the Q8 release/interaction Gate remain open and are not prerequisites being
silently inferred from this decision.

## Source and CI

- Pull request: <https://github.com/918154429/smart-band-demo/pull/14>
- Source commit: `743ff28a32e222255f39c51c89921cc29a16ab31`
- Host run `30076233905`: passed on GCC, Clang and MSVC, including the
  notification core/service, central runtime, UI compile and evidence harness.
- Browser run `30076233918`: Chromium passed.
- The Host notification tests cover the 1000-event mixed/duplicate path,
  bounded queues, overlong content, DND and actions. The native journey then
  checks the actual LVGL/OpenVela integration; neither substitutes for the
  other.

## Fresh fixed OpenVela artifact

- Workflow run: <https://github.com/918154429/smart-band-demo/actions/runs/30076360836>
- Result: `success`
- Artifact: `openvela-fixed-release-30076360836`
- Artifact size: `50,283,762` bytes
- Server digest:
  `sha256:f2d5be444305166cd811f4a175385db725fe05ab77a1acf8cc3135c06a4e7d20`
- Downloaded root manifest: 336 entries, zero missing or mismatched files.
- Nested Q4 manifest: 167 entries, zero missing or mismatched files.

The downloaded artifact was audited locally under
`output/github-openvela-30076360836/openvela-fixed-release-30076360836`.
`output/` is intentionally not committed.

## Native journey

The fresh GitHub four-boot run completed with:

- journey run ID `c50370c81438d5692076288d427f856e`;
- `status=passed`, `failure=null`;
- 24/24 top-level checks true;
- scenario order `ordinary / center / calls / workout`;
- isolated runtime copies, unchanged fixed sources, per-scenario process
  cleanup, removed runtime trees and released console port;
- actions `dismiss 701`, `read 715`, `delete 715`, `accept 721`,
  `reject 722` and `reject 731`, all with `result=applied`;
- simulated haptic and synthetic wake identities paired with their
  notification generations, with no retry/drop counter hidden by a later
  zero-valued marker.

The call screenshots also pass the semantic full-screen gate over
`[453,43,826,757]`: luminance `<90`, dark fraction `>=0.80`, and distinct
Alice/Bob hashes. Measured dark fractions were `0.904841` for Alice and
`0.904150` for Bob.

## Manual review of all native PNGs

| Screenshot | Manual result |
| --- | --- |
| `ordinary-initial.png` | Shows `Native message`, the initial explicit-length body and Dismiss without clipping. |
| `ordinary-updated-long-utf8.png` | Title changes to `Native message updated`; the long body remains within the overlay. Non-ASCII glyphs render as tofu, so this proves explicit-length/layout handling, not Unicode glyph readability. |
| `center-dnd.png` | Shows five DND-retained Center entries; full `Mark read` and `Delete` labels are visible. |
| `center-marked-read.png` | The first row loses `New:` and its `Mark read` action while Delete remains, matching the read transition. |
| `calls-alice.png` | Shows the full-screen Alice call with Reject and Accept. |
| `calls-bob-promoted.png` | Shows the distinct promoted Bob full-screen call with Reject and Accept. |
| `workout-call.png` | Coach call remains non-blocking over ACTIVE workout; button is `Pause` and counter is `Pauses 0`. |
| `workout-paused-with-call.png` | The same call remains present while the workout changes to `Resume` and `Pauses 1`. |

The visual review is required evidence. `status=passed`, nonblank screenshots
and different hashes alone would not have caught the earlier Alice watch-face
race.

## Stack and target linkage

One task-lifetime stack-coloration high-water sample was captured per isolated
scenario:

| Scenario | Used | Remaining | Remaining % |
| --- | ---: | ---: | ---: |
| ordinary | 12,232 B | 20,248 B | 62.340% |
| center | 14,456 B | 18,040 B | 55.515% |
| calls | 12,912 B | 19,584 B | 60.266% |
| workout | 15,000 B | 17,496 B | 53.840% |

The minimum observed margin is `53.840%`, above the provisional 25% native
journey budget.

The artifact contains the linked AArch64 ELF, `nuttx.map` and `System.map`.
Their SHA-256 values are:

- `vela_ap.elf`:
  `bcc3a2cdd6dcff7de060e2891200be6d51645fdd211e200b6fe697d8c3969a57`
- `nuttx.map`:
  `45d5b0b38a707e9e686f04f2bc27242488419b458446c5e5945c8cc000bb3c50`
- `System.map`:
  `38a7395f8f79dc63c4c7805abe40d298fab5f6e260cdc188a2a61a3ba1a45d00`

All 221 relevant defined ELF function/object symbols were present at the same
address in `System.map`. The five audited anchors
`smart_band_main`, `smart_band_lvgl_create`,
`smart_band_notification_view_mount`,
`smart_band_notification_service_process` and
`smart_band_notification_event_received_utf8` also matched the linker map.
`app_lvgl.c.o`, `notification_view.c.o` and
`notification_service.c.o` are present in the map.

An independent run on the outer development host `ubuntu24-hushen` produced
run ID `27598215109e7eda1367f60b783dc2ec`, 24/24 checks, 167/167 manifest
entries and a 53.225% minimum stack margin. It corroborates the GitHub result
but is not used to claim target-board execution.

## Evidence boundaries

This Gate proves the Q4 notification/call/haptic contract in Host tests,
Browser reference tests and pinned OpenVela Goldfish native execution. It does
not prove:

- readable Unicode glyph coverage: the reviewed non-ASCII body is tofu;
- physical vibration: haptic evidence is explicitly `simulated=1`;
- a real display or power transition: wake is
  `synthetic=1 power_transition=0`;
- Gemini-S1 or RK3568 execution, real touch hardware, BLE transport, battery
  life or measured power;
- any board flashing, flash durability or bootloader behavior.
- a 30-minute Q4 native soak, two-hour simulator RC, eight-hour top-tier RC or
  field qualification.

PR14 remains open. Closing this software Gate does not authorize merging the
PR, connecting to RK3568, flashing a board or changing a bootloader.
