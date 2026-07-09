---
name: openvela-smart-band-reproduce
description: Reproduce the smart-band-demo end to end on openvela. Use when the user asks for one-click reproduction, openvela environment setup, smart band demo build/run, or wants an AI/Claude skill flow that installs openvela first and then shows the final browser demo.
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

2. Ensure official open-vela skills exist in the openvela root:

```bash
cd "$OPENVELA_ROOT"
git clone https://github.com/open-vela/.claude.git .claude
```

If `.claude` already exists, keep it and optionally update it with
`git -C .claude pull --ff-only`.

3. If openvela is not installed yet, hand off environment setup to the official
   skill by following `.claude/skills/openvela-quickstart/SKILL.md`.

For Claude users, the prompt that should trigger that official skill is:

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
   - clone or update `$OPENVELA_ROOT/.claude`;
   - copy `openvela_app/smart_band` into `packages/demos/smart_band_basic`;
   - copy it into `apps/packages/demos/smart_band_basic` when that mirror exists;
   - enable `CONFIG_LVX_USE_DEMO_SMART_BAND_BASIC` in the goldfish defconfig;
   - build goldfish arm64, trying CMake first and legacy build as fallback;
   - serve and open `demo/index.html` so the final browser demo is visible.

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

## Validation

Run these checks before reporting success:

```bash
git -C "$SMART_BAND_DEMO_ROOT" status --short
python3 "$SMART_BAND_DEMO_ROOT/tests/test_watch_model.py"
```

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
