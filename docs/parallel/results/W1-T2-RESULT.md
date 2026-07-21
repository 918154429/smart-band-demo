# W1-T2 Result: Q3 workout core

## Delivery scope

Branch: `codex/w1-q3-workout-core`

This delivery contains only the standalone step normalizer, workout model, host
test, and task-specific coverage runner. It does not connect runtime, storage,
LVGL, events, CI, or product history. Therefore this is Q3-1 core ready, not the
Q3 product Gate.

## Step normalizer contract

### Input and units

- `source`: `SENSOR`, `DERIVED`, or `SIMULATION`.
- `raw_counter`: unsigned 32-bit or 64-bit counter selected by config.
- `available` and `fresh`: explicit adapter-provided quality inputs.
- `monotonic_ms`: nondecreasing monotonic milliseconds.
- Configured thresholds: `counter_bits`, `max_forward_delta`,
  `wrap_high_threshold`, `wrap_low_threshold`, and `max_sample_gap_ms`.

No threshold is hidden in production code. The tests use a maximum accepted
delta of 100 steps, a 100-count wrap window at each counter edge, and a 1000 ms
maximum sample gap.

### Output enums and provenance

- Results: `OK`, `INVALID_ARGUMENT`, `INVALID_SAMPLE`, `TIME_REGRESSION`,
  `OVERFLOW`.
- Reasons: `NONE`, `BASELINE`, `DELTA`, `SOURCE_SWITCH`, `UNAVAILABLE`,
  `STALE`, `GAP`, `RESET`, `WRAP`, `FORWARD_JUMP`.
- Quality flags: `ACCEPTED`, `REBASED`, `DISCONTINUITY`, `SOURCE_CHANGED`,
  `UNAVAILABLE`, `STALE`, `WRAPPED`.
- Numeric output: nonnegative `delta` and `total`, both in steps.

Unavailable/stale samples invalidate the baseline. Recovery accepts the first
fresh sample only as a new baseline. A source switch also rebases, so counters
from different providers cannot be subtracted or double-counted.

### Wrap assumption

Counters are unsigned and wrap modulo `2^counter_bits`. A decreasing sample is
classified as wrap only when the prior value is at or above
`wrap_high_threshold`, the new value is at or below `wrap_low_threshold`, and
the modulo delta is no larger than `max_forward_delta`. Any other decrease is a
reset; an excessive increase is a forward jump. Reset/jump values are not added.

## Workout model contract

### Enums and units

- Modes: `WALK`, `RUN`.
- States: `IDLE`, `COUNTDOWN`, `ACTIVE`, `PAUSED`, `FINISHED`, `ABORTED`,
  `RECOVERY_CONFIRMATION`.
- Commands: `START`, `PAUSE`, `RESUME`, `FINISH`, `ABORT`,
  `CONFIRM_RECOVERY`.
- Results: `OK`, `INVALID_ARGUMENT`, `INVALID_CONFIG`, `INVALID_STATE`,
  `INVALID_SAMPLE`, `TIME_REGRESSION`, `OVERFLOW`.
- Time: unsigned monotonic milliseconds.
- Distance: integer millimetres.
- Energy: integer milli-kcal.
- Heart rate: bpm; weighted accumulator is bpm-ms and its denominator is ms.
- Pace: integer ms/km, truncated toward zero; zero means unavailable or an
  unrepresentable overflow result, while distance and duration remain exported.

Mode config fixes `stride_mm`, `calories_milli_kcal_per_step`, accepted step
delta, and heart-rate range independently for Walk and Run. The tests use Walk
800 mm and 40 milli-kcal per step, and Run 1200 mm and 80 milli-kcal per step.
All multiplication and accumulation is checked before model state is committed.

Heart rate `0` is never used as a measurement. Current validity and aggregate
validity are explicit. The weighted average uses the latest valid active sample
over subsequent active time; paused time is excluded.

### Legal transitions

| From | Command/event | To |
| --- | --- | --- |
| `IDLE` | `START` | `COUNTDOWN` |
| `COUNTDOWN` | configured countdown elapsed | `ACTIVE` |
| `COUNTDOWN` | `ABORT` | `ABORTED` |
| `ACTIVE` | `PAUSE` | `PAUSED` |
| `ACTIVE` | `FINISH` | `FINISHED` |
| `ACTIVE` | `ABORT` | `ABORTED` |
| `PAUSED` | `RESUME` | `ACTIVE` |
| `PAUSED` | `FINISH` | `FINISHED` |
| `PAUSED` | `ABORT` | `ABORTED` |
| `RECOVERY_CONFIRMATION` | `CONFIRM_RECOVERY` | `PAUSED` |
| `RECOVERY_CONFIRMATION` | `ABORT` | `ABORTED` |

All other transitions return `INVALID_STATE` and leave the complete model
unchanged. Restoring a live checkpoint always enters `RECOVERY_CONFIRMATION`;
confirmation goes to `PAUSED`, so a separate `RESUME` is required before any
active accumulation can continue.

## Checkpoint data

The snapshot exports mode, state, countdown progress, active duration, steps,
distance, energy, current/min/max heart rate with validity, and weighted heart
sum/duration. It contains pure data only and performs no encoding or I/O.

## Test evidence

- `python tests/test_workout_core.py`: PASS on MSVC C11 with `/W4 /WX`.
- Test binary output: `smart band workout core tests passed`.
- `python -m py_compile tests/test_workout_core.py tests/test_workout_core_coverage.py`:
  PASS.
- `git diff --cached --check`: PASS for all eight delivered files.
- GCC/Clang: not installed on this Windows host; WSL has no installed distro.
- `python tests/test_workout_core_coverage.py`: not runnable locally,
  `required coverage tool is unavailable: gcc`.
- Production per-file line coverage is therefore not claimed. The runner enforces
  `>=90%` separately for `step_normalizer.c` and `workout_model.c` when GCC,
  gcov, and gcovr are available.

The C matrix covers normal increments, 32/64-bit wrap edges, resets, excessive
jumps, source switching, stale/unavailable recovery, gap rebasing, overflow and
time regression; it also covers the three-second countdown, pause/resume,
finish/abort/recovery, Walk/Run isolation, zero steps, absent/extreme valid heart
rate, 24-hour acceleration, invalid enums/config/samples, null pointers,
duplicate commands, terminal immutability, and checked arithmetic overflow.

## Integration request

W1-I should:

1. Run `python tests/test_workout_core.py` and
   `python tests/test_workout_core_coverage.py` in Linux CI with GCC/gcov/gcovr.
2. Add the two production sources to shared build entry points only after source
   freeze.
3. Map runtime samples to `source`, raw counter, availability, freshness, and
   monotonic milliseconds; choose device-specific discontinuity thresholds.
4. Feed only normalized `delta` into workout updates and persist the exported
   snapshot using the shared storage layer.
5. Map sensor heart validity explicitly and send a fresh sample after resume so
   a pre-pause heart value is not held longer than the adapter intends.

## Known risks

- Coverage remains a required Linux gate because it cannot be measured here.
- Wrap/reset classification depends on device-specific thresholds supplied by
  the adapter; incorrect thresholds can intentionally favor rebasing over count.
- Pace returns `0` both when distance is zero and when scale multiplication would
  overflow; callers should inspect distance and duration to distinguish them.
