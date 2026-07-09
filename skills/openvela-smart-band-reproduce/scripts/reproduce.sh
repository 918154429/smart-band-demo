#!/usr/bin/env bash
set -u

CONFIG_PATH="vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap"
OUTPUT_DIR="cmake_out/vela_goldfish-arm64-v8a-ap"
PORT="${SMART_BAND_DEMO_PORT:-8765}"
JOBS="${SMART_BAND_BUILD_JOBS:-$(nproc 2>/dev/null || echo 2)}"
SKIP_BUILD=0
NO_BROWSER=0
OPENVELA_ROOT=""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEMO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

usage() {
  cat <<'USAGE'
Usage:
  reproduce.sh [--openvela-root PATH] [--skip-build] [--no-browser]

What it does:
  1. Ensures open-vela official .claude skills are present.
  2. Copies smart_band into openvela packages/demos.
  3. Enables the smart_band Kconfig options in the goldfish defconfig.
  4. Builds openvela for goldfish arm64.
  5. Serves and opens the browser demo page.

Environment:
  SMART_BAND_BUILD_JOBS    build parallelism, default nproc
  SMART_BAND_DEMO_PORT     browser demo port, default 8765
USAGE
}

log() {
  printf '[smart-band-reproduce] %s\n' "$*"
}

die() {
  printf '[smart-band-reproduce] ERROR: %s\n' "$*" >&2
  exit 1
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --openvela-root)
      [ "$#" -ge 2 ] || die "--openvela-root requires a path"
      OPENVELA_ROOT="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --no-browser)
      NO_BROWSER=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      die "unknown argument: $1"
      ;;
  esac
done

guess_openvela_root() {
  if [ -n "$OPENVELA_ROOT" ]; then
    printf '%s\n' "$OPENVELA_ROOT"
    return
  fi

  if [ -f "$(pwd)/build.sh" ]; then
    pwd
    return
  fi

  if [ -f "$DEMO_ROOT/../../build.sh" ]; then
    cd "$DEMO_ROOT/../.." && pwd
    return
  fi

  pwd
}

OPENVELA_ROOT="$(cd "$(guess_openvela_root)" 2>/dev/null && pwd || printf '%s\n' "$(guess_openvela_root)")"

ensure_claude_skills() {
  mkdir -p "$OPENVELA_ROOT"
  if [ ! -d "$OPENVELA_ROOT/.claude/.git" ]; then
    log "cloning official open-vela .claude skills"
    git clone https://github.com/open-vela/.claude.git "$OPENVELA_ROOT/.claude" || \
      die "failed to clone https://github.com/open-vela/.claude.git"
  else
    log "official .claude skills already exist"
    git -C "$OPENVELA_ROOT/.claude" pull --ff-only || \
      log "warning: .claude update failed; continuing with existing copy"
  fi
}

install_local_skill() {
  local src="$DEMO_ROOT/skills/openvela-smart-band-reproduce/"
  local dst="$OPENVELA_ROOT/.claude/skills/openvela-smart-band-reproduce/"

  [ -d "$src" ] || die "local reproduce skill not found: $src"

  if [ "$(cd "$src" && pwd)" = "$(mkdir -p "$dst" && cd "$dst" && pwd)" ]; then
    log "local reproduce skill is already installed in .claude"
    return 0
  fi

  log "installing smart-band reproduce skill into .claude/skills"
  rsync -a --delete "$src" "$dst"
}

require_openvela_checkout() {
  if [ ! -f "$OPENVELA_ROOT/build.sh" ]; then
    cat <<EOF

openvela is not installed at:
  $OPENVELA_ROOT

The official skills have been prepared at:
  $OPENVELA_ROOT/.claude

Next handoff prompt for the official quickstart flow:
  帮我搭建 openvela 开发环境

After the official openvela-quickstart skill finishes, rerun:
  bash "$DEMO_ROOT/skills/openvela-smart-band-reproduce/scripts/reproduce.sh" --openvela-root "$OPENVELA_ROOT"

EOF
    exit 20
  fi
}

sync_demo_app() {
  local src="$DEMO_ROOT/openvela_app/smart_band/"
  local dst="$OPENVELA_ROOT/packages/demos/smart_band_basic/"
  local mirror="$OPENVELA_ROOT/apps/packages/demos/smart_band_basic/"

  [ -d "$src" ] || die "smart band app source not found: $src"

  log "syncing smart_band app into packages/demos"
  mkdir -p "$dst"
  rsync -a --delete "$src" "$dst"

  if [ -d "$OPENVELA_ROOT/apps/packages/demos" ]; then
    log "syncing smart_band app into apps/packages mirror"
    mkdir -p "$mirror"
    rsync -a --delete "$src" "$mirror"
  fi
}

append_unique_config() {
  local defconfig="$1"
  local key="$2"
  local value="$3"

  if grep -Eq "^(# )?${key}[ =]" "$defconfig"; then
    sed -i "s/^# ${key} is not set/${key}=${value}/" "$defconfig"
    sed -i "s/^${key}=.*/${key}=${value}/" "$defconfig"
  else
    printf '%s=%s\n' "$key" "$value" >> "$defconfig"
  fi
}

enable_config() {
  local defconfig="$OPENVELA_ROOT/$CONFIG_PATH/defconfig"

  [ -f "$defconfig" ] || die "defconfig not found: $defconfig"

  log "enabling smart_band config in $CONFIG_PATH/defconfig"
  append_unique_config "$defconfig" "CONFIG_GRAPHICS_LVGL" "y"
  append_unique_config "$defconfig" "CONFIG_LV_USE_NUTTX" "y"
  append_unique_config "$defconfig" "CONFIG_LV_USE_NUTTX_LIBUV" "y"
  append_unique_config "$defconfig" "CONFIG_SENSORS" "y"
  append_unique_config "$defconfig" "CONFIG_UORB" "y"
  append_unique_config "$defconfig" "CONFIG_LVX_USE_DEMO_SMART_BAND_BASIC" "y"
  append_unique_config "$defconfig" "CONFIG_LVX_DEMO_SMART_BAND_BASIC_PRIORITY" "100"
  append_unique_config "$defconfig" "CONFIG_LVX_DEMO_SMART_BAND_BASIC_STACKSIZE" "32768"
}

build_openvela() {
  if [ "$SKIP_BUILD" -eq 1 ]; then
    log "build skipped by --skip-build"
    return 0
  fi

  cd "$OPENVELA_ROOT" || die "cannot cd to $OPENVELA_ROOT"
  log "building openvela goldfish arm64 with $JOBS jobs"

  if ./build.sh "$CONFIG_PATH" --cmake -j"$JOBS"; then
    log "cmake build completed"
    return 0
  fi

  log "cmake build failed; trying legacy build command"
  ./build.sh "$CONFIG_PATH" -j"$JOBS" || die "openvela build failed"

  if [ -f "$OPENVELA_ROOT/nuttx/nuttx" ]; then
    log "copying legacy build outputs into $OUTPUT_DIR"
    mkdir -p "$OPENVELA_ROOT/$OUTPUT_DIR"
    cp "$OPENVELA_ROOT/nuttx/nuttx" "$OPENVELA_ROOT/$OUTPUT_DIR/nuttx"
    for f in vela_system.bin vela_data.bin vela_ap.bin nuttx.bin nuttx.hex; do
      [ -f "$OPENVELA_ROOT/nuttx/$f" ] && cp "$OPENVELA_ROOT/nuttx/$f" "$OPENVELA_ROOT/$OUTPUT_DIR/$f"
    done
  fi
}

serve_browser_demo() {
  local url="http://127.0.0.1:$PORT/demo/index.html"
  local log_file="$DEMO_ROOT/demo/browser-demo.log"

  if [ "$NO_BROWSER" -eq 1 ]; then
    log "browser demo skipped by --no-browser"
    return 0
  fi

  if command -v python3 >/dev/null 2>&1; then
    if ! curl --noproxy 127.0.0.1 -fsS "$url" >/dev/null 2>&1; then
      log "serving browser demo at $url"
      nohup python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$DEMO_ROOT" > "$log_file" 2>&1 &
      sleep 1
    else
      log "browser demo server already responds at $url"
    fi
  else
    log "python3 not found; open the file directly: $DEMO_ROOT/demo/index.html"
    return 0
  fi

  if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "$url" >/dev/null 2>&1 || true
  fi

  log "browser demo URL: $url"
}

print_simulator_command() {
  cat <<EOF

Simulator command after build:
  cd "$OPENVELA_ROOT"
  ./emulator.sh "$OUTPUT_DIR" -skin xiaomi_smart_screen_10 -skindir "$OPENVELA_ROOT/prebuilts/emulator/skins/"

At the NSH prompt:
  smart_band

EOF
}

main() {
  log "demo root: $DEMO_ROOT"
  log "openvela root: $OPENVELA_ROOT"
  ensure_claude_skills
  install_local_skill
  require_openvela_checkout
  sync_demo_app
  enable_config
  build_openvela
  serve_browser_demo
  print_simulator_command
  log "done"
}

main "$@"
