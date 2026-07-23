# Q4 native Gate harness implementation — 2026-07-23

## Status

The deterministic Q4 native notification harness is implemented and locally
verified, but it has not yet completed a fresh fixed OpenVela run. The Q4 C
Gate therefore remains **open**. This document records the harness contract and
pre-run verification only; final evidence must be written from a successful
artifact and manual review of every native PNG.

## Diagnostics-only scenario contract

When `CONFIG_LVX_DEMO_SMART_BAND_E2E_DIAGNOSTICS=y`, the application accepts
exactly one optional argument:

```text
--q4-native-scenario=ordinary
--q4-native-scenario=center
--q4-native-scenario=calls
--q4-native-scenario=workout
```

Without that argument, normal application startup creates no Q4 scenario
timer, injects no notification, and has no scenario side effect. Unknown or
malformed values fail closed. When diagnostics are disabled, the argument path
and helper are not compiled.

Every scenario enters through the production explicit-length external ingress:

- `ordinary`: notification 701, then a 2.5-second same-ID long UTF-8 update;
- `center`: DND plus notifications 711–715, followed by Center, Mark read and
  Delete actions;
- `calls`: Alice 721 then Bob 722, with Accept/Reject and full-screen input
  isolation;
- `workout`: arm Coach Call 731, wait for Workout `ACTIVE`, then prove the
  non-blocking card, Pause action and Reject.

The injector emits strict `smart_band:q4:inject:v1` markers. Existing Q3/Q4
state markers and simulated haptic/synthetic-wake markers remain the source of
truth for state and effect assertions. All injector markers are flushed
immediately.

## Four-boot runner

`scripts/run_q4_native_e2e.py` stages four isolated runtime-output copies and
boots one scenario per copy. It:

- parses Q3, Q4, injector, haptic and wake markers with exact field contracts;
- records native screenshots for ordinary initial/update, DND Center actions,
  Call input isolation and Accept/Reject, and Workout non-blocking behavior;
- requires simulated haptic markers and
  `synthetic=1 power_transition=0` wake markers rather than claiming physical
  effects;
- captures NSH `ps` at each checkpoint and derives smart-band stack peak,
  filled percentage and minimum remaining margin from
  `STACK/USED/FILLED`; every sample fails closed below the provisional
  worst-path 25% stack-margin budget;
- verifies fixed source artifacts remain unchanged, runtime staging uses
  isolated copies, and every emulator process group, port and staging tree is
  cleaned;
- writes `q4-native-journey.json`, transcripts, screenshots, resource samples
  and an `evidence.sha256` manifest.

The nightly workflow enables diagnostics for Q3 or Q4 native runs, requires
`CONFIG_STACK_COLORATION=y`, and collects `vela_ap.elf`, `nuttx.map`,
`System.map`, hashes, section sizes, ELF headers/sections, smart-band symbols
and relevant linker-map lines. Artifact hashes use paths relative to the
downloaded evidence root, and the final workflow step regenerates a recursive
root `evidence.sha256` covering the target files and nested Q4 evidence.

## Pre-run verification

Local Windows verification:

- complete Host-equivalent suite: `22/22` scripts passed;
- Q4 runner unit tests: `13/13` passed;
- production UI compile and journeys: MSVC C11 `/W4 /WX` passed;
- Browser/Chromium: `7/7` passed;
- workflow YAML parsing and `git diff --check`: passed.

Independent outer development host verification used `ubuntu24-hushen` at
`/data/smart-band-q4-native-20260723T1150CST`:

- GCC strict production UI: passed;
- notification service and central runtime: passed;
- Q4 runner unit tests at that snapshot: `12/12` passed.

No real emulator was run on that host, and `codex-2c8g` was not used as the
development machine.

## Required final audit

A fresh Q4-enabled OpenVela run must still prove all of the following before
the C Gate can close:

1. four scenario JSON results and every required check are passed;
2. all native PNGs are manually reviewed, including the ordinary initial/update
   title difference and all input-isolation checkpoints;
3. `evidence.sha256` is fully recomputed with no mismatch;
4. ELF, real linker map, symbols and artifact hashes are present and internally
   consistent;
5. stack samples cover every scenario and report a credible peak and minimum
   margin;
6. fixed source output, emulator process groups, console port and staged
   runtime cleanup all pass.

This Goldfish evidence cannot prove Gemini-S1 or RK3568 execution, physical
vibration, real display wake, real power transition or BLE transport.
