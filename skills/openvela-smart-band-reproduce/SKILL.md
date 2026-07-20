---
name: openvela-smart-band-reproduce
description: Reproduce the smart-band-demo end to end on openvela. Use when the user asks for one-click reproduction, openvela environment setup, smart band demo build/run, or wants a skill flow that installs openvela first and then shows the final browser demo.
---

# openvela Smart Band Reproduce

Use this skill to reproduce `smart-band-demo` from a fresh or existing openvela
workspace. The intended flow is: install openvela with the official open-vela
`.claude` skills, then integrate this demo, build it, and open the final demo
page.

## Workflow

1. Identify paths:
   - `SMART_BAND_DEMO_ROOT`: this repository.
   - `OPENVELA_ROOT`: an existing openvela checkout, or the directory where
     openvela will be created.

2. Let the repository script prepare the official open-vela skills. It checks
   out the exact `.claude` commit recorded in `versions.env`; do not replace
   this with an unpinned clone or `git pull`.

The openvela checkout is also verified against the pinned `tags/trunk-5.4.xml`
release manifest and manifest-repository commit in `versions.env`. Reviewed
alternate full commits and manifest names may be supplied through
`SMART_BAND_CLAUDE_REVISION` and
`SMART_BAND_OPENVELA_MANIFEST_REVISION` plus
`SMART_BAND_OPENVELA_MANIFEST_FILE`.
Unreviewed `.repo/local_manifests/*.xml` files are rejected because they change
the effective checkout outside the repository-owned pin.

3. If openvela is not installed yet, hand off environment setup to the official
   skill by following `.claude/skills/openvela-quickstart/SKILL.md`.

Prompt that should trigger the official quickstart skill:

```text
帮我搭建 openvela 开发环境
```

Do not invent a separate openvela installation flow. Let the official
`openvela-quickstart` skill handle dependency installation, repo init/sync,
build target selection, and first simulator boot.

4. After openvela reaches a successful build or at least has a valid checkout
   with `build.sh`, run this skill's script:

```bash
bash "$SMART_BAND_DEMO_ROOT/skills/openvela-smart-band-reproduce/scripts/reproduce.sh" \
  --openvela-root "$OPENVELA_ROOT"
```

The script will:
   - clone `$OPENVELA_ROOT/.claude` when it is absent;
   - copy `openvela_app/smart_band` into `packages/demos/smart_band_basic`;
   - copy it into `apps/packages/demos/smart_band_basic` when that mirror exists;
   - enable `CONFIG_LVX_USE_DEMO_SMART_BAND_BASIC` and its real-sensor provider
     option in the goldfish defconfig;
   - build goldfish arm64, trying CMake first and legacy build as fallback;
   - serve and open `demo/index.html` so the final browser demo is visible.

Before mutating an existing checkout, first run a read-only preflight:

```bash
bash "$SMART_BAND_DEMO_ROOT/scripts/reproduce_openvela_demo.sh" \
  --openvela-root "$OPENVELA_ROOT" --dry-run --no-browser
```

The script refuses to overwrite dirty target paths by default. Use
`--allow-dirty` only after the user has explicitly confirmed those changes may
be replaced.

Synchronization is overlay-only and never uses `rsync --delete`. The goldfish
defconfig is backed up before editing and restored automatically if any later
step fails or the script is interrupted.

5. For simulator verification after build, use:

```bash
cd "$OPENVELA_ROOT"
./emulator.sh cmake_out/vela_goldfish-arm64-v8a-ap \
  -skin xiaomi_smart_screen_10 \
  -skindir "$OPENVELA_ROOT/prebuilts/emulator/skins/"
```

At the NSH prompt:

```text
smart_band
```

For a non-interactive Linux runtime check, run:

```bash
python3 "$SMART_BAND_DEMO_ROOT/scripts/smoke_openvela_emulator.py" \
  --openvela-root "$OPENVELA_ROOT" \
  --evidence-dir /tmp/smart-band-emulator-smoke
```

This drives the real NSH terminal over a pseudo-terminal, checks the emulator
console, requires the native `smart_band: UI ready` marker, and verifies the
application PID twice. The pinned goldfish configuration does not enable ADB
shell, so an ADB device listing alone is not runtime proof.

## Validation

Run these checks before reporting success:

```bash
git -C "$SMART_BAND_DEMO_ROOT" status --short
python3 "$SMART_BAND_DEMO_ROOT/tests/test_watch_model.py"
bash "$SMART_BAND_DEMO_ROOT/scripts/test_reproduce_failure.sh"
```

The Python entry compiles and runs the production `watch_model.c`; it is not a
separate model implementation.

Confirm the browser demo URL printed by the script returns HTTP 200, usually:

```text
http://127.0.0.1:8765/demo/index.html
```

If the openvela build was skipped because the environment is not ready, report
the exact next handoff prompt:

```text
帮我搭建 openvela 开发环境
```

and tell the user to rerun the reproduce script after that official skill
finishes.
