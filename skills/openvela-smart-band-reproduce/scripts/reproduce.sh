#!/usr/bin/env bash
set -Eeuo pipefail

CONFIG_PATH="vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap"
OUTPUT_DIR="cmake_out/vela_goldfish-arm64-v8a-ap"
PORT="${SMART_BAND_DEMO_PORT:-8765}"
JOBS="${SMART_BAND_BUILD_JOBS:-$(nproc 2>/dev/null || echo 2)}"
SKIP_BUILD=0
NO_BROWSER=0
DRY_RUN=0
ALLOW_DIRTY=0
OPENVELA_ROOT=""
DEFCONFIG_PATH=""
DEFCONFIG_BACKUP=""
DEFCONFIG_TRANSACTION_ACTIVE=0

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEMO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
VERSION_FILE="$DEMO_ROOT/skills/openvela-smart-band-reproduce/versions.env"

[ -r "$VERSION_FILE" ] || {
  printf '[smart-band-reproduce] ERROR: version manifest not found: %s\n' \
    "$VERSION_FILE" >&2
  exit 1
}
# shellcheck disable=SC1090
source "$VERSION_FILE"

CLAUDE_REPOSITORY="${SMART_BAND_CLAUDE_REPOSITORY:-$SMART_BAND_DEFAULT_CLAUDE_REPOSITORY}"
CLAUDE_REVISION="${SMART_BAND_CLAUDE_REVISION:-$SMART_BAND_DEFAULT_CLAUDE_REVISION}"
OPENVELA_MANIFEST_REVISION="${SMART_BAND_OPENVELA_MANIFEST_REVISION:-$SMART_BAND_DEFAULT_OPENVELA_MANIFEST_REVISION}"
OPENVELA_MANIFEST_FILE="${SMART_BAND_OPENVELA_MANIFEST_FILE:-$SMART_BAND_DEFAULT_OPENVELA_MANIFEST_FILE}"
FAIL_AT="${SMART_BAND_REPRODUCE_FAIL_AT:-}"

usage() {
  cat <<'USAGE'
Usage:
  reproduce.sh [--openvela-root PATH] [--skip-build] [--no-browser]
               [--dry-run] [--allow-dirty]

What it does:
  1. Ensures open-vela official .claude skills are present.
  2. Copies smart_band into openvela packages/demos.
  3. Enables the smart_band Kconfig options in the goldfish defconfig.
  4. Builds openvela for goldfish arm64.
  5. Serves and opens the browser demo page.

Environment:
  SMART_BAND_BUILD_JOBS    build parallelism, default nproc
  SMART_BAND_DEMO_PORT     browser demo port, default 8765
  SMART_BAND_CLAUDE_REVISION
                            full .claude commit, defaults to versions.env
  SMART_BAND_OPENVELA_MANIFEST_REVISION
                            full openvela manifest commit, defaults to versions.env
  SMART_BAND_OPENVELA_MANIFEST_FILE
                            release manifest path, defaults to tags/trunk-5.4.xml

Safety:
  --dry-run       Validate prerequisites and print planned mutations only.
  --allow-dirty   Permit overwriting dirty target paths in openvela.
USAGE
}

log() {
  printf '[smart-band-reproduce] %s\n' "$*"
}

die() {
  printf '[smart-band-reproduce] ERROR: %s\n' "$*" >&2
  exit 1
}

on_error() {
  local status="$1"
  local line="$2"

  trap - ERR
  printf '[smart-band-reproduce] ERROR: command failed at line %s (exit %s)\n' \
    "$line" "$status" >&2
  exit "$status"
}

on_exit() {
  local status="$?"

  trap - EXIT
  if [ "$DEFCONFIG_TRANSACTION_ACTIVE" -eq 1 ]; then
    if [ "$status" -eq 0 ]; then
      rm -f -- "$DEFCONFIG_BACKUP"
    else
      if cp -p -- "$DEFCONFIG_BACKUP" "$DEFCONFIG_PATH"; then
        log "restored defconfig after failure: $DEFCONFIG_PATH"
        rm -f -- "$DEFCONFIG_BACKUP"
      else
        printf '[smart-band-reproduce] ERROR: failed to restore defconfig from %s\n' \
          "$DEFCONFIG_BACKUP" >&2
      fi
    fi
  fi

  exit "$status"
}

trap 'on_error "$?" "$LINENO"' ERR
trap on_exit EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

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
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    --allow-dirty)
      ALLOW_DIRTY=1
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

OPENVELA_CANDIDATE="$(guess_openvela_root)"
if [ -d "$OPENVELA_CANDIDATE" ]; then
  OPENVELA_ROOT="$(cd "$OPENVELA_CANDIDATE" && pwd)"
else
  OPENVELA_ROOT="$OPENVELA_CANDIDATE"
fi

require_command() {
  command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

require_full_revision() {
  local name="$1"
  local revision="$2"

  [[ "$revision" =~ ^[0-9a-fA-F]{40}$ ]] || \
    die "$name must be a full 40-character Git commit: $revision"
}

maybe_fail() {
  local point="$1"

  if [ -n "$FAIL_AT" ] && [ "$FAIL_AT" = "$point" ]; then
    die "injected failure at $point"
  fi
}

preflight() {
  local value

  for value in cmp git rsync grep sed; do
    require_command "$value"
  done

  case "$JOBS" in
    ''|*[!0-9]*) die "SMART_BAND_BUILD_JOBS must be a positive integer: $JOBS" ;;
  esac
  [ "$JOBS" -gt 0 ] || die "SMART_BAND_BUILD_JOBS must be greater than zero"

  case "$PORT" in
    ''|*[!0-9]*) die "SMART_BAND_DEMO_PORT must be an integer: $PORT" ;;
  esac
  [ "$PORT" -ge 1 ] && [ "$PORT" -le 65535 ] || \
    die "SMART_BAND_DEMO_PORT must be between 1 and 65535"

  require_full_revision "SMART_BAND_CLAUDE_REVISION" "$CLAUDE_REVISION"
  require_full_revision "SMART_BAND_OPENVELA_MANIFEST_REVISION" \
    "$OPENVELA_MANIFEST_REVISION"
  case "$OPENVELA_MANIFEST_FILE" in
    ''|/*|*..*) die "SMART_BAND_OPENVELA_MANIFEST_FILE must be a repository-relative manifest name" ;;
  esac

  [ -d "$DEMO_ROOT/openvela_app/smart_band" ] || \
    die "smart band app source not found: $DEMO_ROOT/openvela_app/smart_band"
  [ -d "$DEMO_ROOT/skills/openvela-smart-band-reproduce" ] || \
    die "local reproduce skill not found"

  if [ "$NO_BROWSER" -eq 0 ]; then
    require_command curl
  fi

  log "preflight passed"
}

existing_parent() {
  local path="$1"

  while [ ! -e "$path" ] && [ "$path" != "/" ]; do
    path="$(dirname "$path")"
  done
  printf '%s\n' "$path"
}

check_target_clean() {
  local target="$1"
  local probe
  local repo_root
  local relative
  local dirty

  probe="$(existing_parent "$target")"
  repo_root="$(git -C "$probe" rev-parse --show-toplevel 2>/dev/null || true)"
  if [ -z "$repo_root" ]; then
    return 1
  fi

  case "$target" in
    "$repo_root") relative="." ;;
    "$repo_root"/*) relative="${target#"$repo_root"/}" ;;
    *) return 1 ;;
  esac

  dirty="$(git -C "$repo_root" status --porcelain --untracked-files=all -- "$relative")"
  if [ -n "$dirty" ]; then
    printf '%s\n' "$dirty" >&2
    die "refusing to overwrite dirty target: $target (use --allow-dirty to override)"
  fi
  return 0
}

protect_mutation_targets() {
  local found_git=0
  local target

  if [ "$ALLOW_DIRTY" -eq 1 ]; then
    log "dirty target protection disabled by --allow-dirty"
    return 0
  fi

  for target in \
    "$OPENVELA_ROOT/$CONFIG_PATH/defconfig" \
    "$OPENVELA_ROOT/packages/demos/smart_band_basic" \
    "$OPENVELA_ROOT/apps/packages/demos/smart_band_basic" \
    "$OPENVELA_ROOT/.claude/skills/openvela-smart-band-reproduce"; do
    if check_target_clean "$target"; then
      found_git=1
    fi
  done

  if [ "$found_git" -eq 0 ]; then
    log "warning: no Git worktree found for mutable targets; dirty check skipped"
  else
    log "mutable target paths are clean"
  fi
}

ensure_claude_skills() {
  local claude_root="$OPENVELA_ROOT/.claude"
  local dirty
  local current

  if [ ! -d "$claude_root/.git" ]; then
    if [ "$DRY_RUN" -eq 1 ]; then
      log "dry-run: would clone $CLAUDE_REPOSITORY at $CLAUDE_REVISION into $claude_root"
      return 0
    fi
    mkdir -p "$OPENVELA_ROOT"
    log "cloning official open-vela .claude skills at pinned revision"
    git clone "$CLAUDE_REPOSITORY" "$claude_root"
  fi

  dirty="$(git -C "$claude_root" status --porcelain --untracked-files=all -- \
    . ':(exclude)skills/openvela-smart-band-reproduce')"
  [ -z "$dirty" ] || die "official .claude checkout has local changes outside the installed smart-band skill"

  current="$(git -C "$claude_root" rev-parse HEAD 2>/dev/null || true)"
  if [ "$current" = "$CLAUDE_REVISION" ]; then
    log "official .claude revision: $CLAUDE_REVISION"
    return 0
  fi

  if [ "$DRY_RUN" -eq 1 ]; then
    log "dry-run: would fetch and check out .claude revision $CLAUDE_REVISION"
    return 0
  fi

  if ! git -C "$claude_root" cat-file -e "$CLAUDE_REVISION^{commit}" 2>/dev/null; then
    log "fetching pinned .claude revision $CLAUDE_REVISION"
    git -C "$claude_root" fetch --depth 1 origin "$CLAUDE_REVISION"
  fi

  git -C "$claude_root" checkout --detach "$CLAUDE_REVISION"

  log "official .claude revision: $CLAUDE_REVISION"
}

install_local_skill() {
  local src="$DEMO_ROOT/skills/openvela-smart-band-reproduce/"
  local dst="$OPENVELA_ROOT/.claude/skills/openvela-smart-band-reproduce/"

  if [ -d "$dst" ] && [ "$(cd "$src" && pwd)" = "$(cd "$dst" && pwd)" ]; then
    log "local reproduce skill is already installed in .claude"
    return 0
  fi

  if [ "$DRY_RUN" -eq 1 ]; then
    log "dry-run: would mirror local reproduce skill to $dst"
    return 0
  fi

  log "installing smart-band reproduce skill into .claude/skills"
  mkdir -p "$dst"
  rsync -a "$src" "$dst"
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

verify_openvela_revision() {
  local revision_root
  local current
  local dirty
  local selected_manifest="$OPENVELA_ROOT/.repo/manifest.xml"
  local pinned_manifest

  if compgen -G "$OPENVELA_ROOT/.repo/local_manifests/*.xml" >/dev/null; then
    die "openvela local manifests are present; remove or explicitly review them before pinned reproduction"
  fi

  if git -C "$OPENVELA_ROOT/.repo/manifests" rev-parse --is-inside-work-tree \
      >/dev/null 2>&1; then
    revision_root="$OPENVELA_ROOT/.repo/manifests"
  else
    die "cannot verify openvela revision: .repo/manifests is not a Git checkout"
  fi

  dirty="$(git -C "$revision_root" status --porcelain --untracked-files=no)"
  [ -z "$dirty" ] || die "openvela manifest checkout is dirty: $revision_root"
  current="$(git -C "$revision_root" rev-parse HEAD)"
  [ "$current" = "$OPENVELA_MANIFEST_REVISION" ] || die \
    "openvela manifest revision is $current, expected $OPENVELA_MANIFEST_REVISION; sync the pinned revision or explicitly set SMART_BAND_OPENVELA_MANIFEST_REVISION after review"
  pinned_manifest="$revision_root/$OPENVELA_MANIFEST_FILE"
  [ -f "$pinned_manifest" ] || die "pinned openvela manifest not found: $pinned_manifest"
  [ -e "$selected_manifest" ] || die "selected openvela manifest not found: $selected_manifest"
  cmp -s "$selected_manifest" "$pinned_manifest" || die \
    "openvela checkout does not use pinned manifest $OPENVELA_MANIFEST_FILE"
  log "openvela manifest: $OPENVELA_MANIFEST_FILE at $OPENVELA_MANIFEST_REVISION"
}

sync_demo_app() {
  local src="$DEMO_ROOT/openvela_app/smart_band/"
  local dst="$OPENVELA_ROOT/packages/demos/smart_band_basic/"
  local mirror="$OPENVELA_ROOT/apps/packages/demos/smart_band_basic/"

  if [ "$DRY_RUN" -eq 1 ]; then
    log "dry-run: would mirror $src to $dst"
    if [ -d "$OPENVELA_ROOT/apps/packages/demos" ]; then
      log "dry-run: would mirror $src to $mirror"
    fi
    return 0
  fi

  log "syncing smart_band app into packages/demos"
  mkdir -p "$dst"
  rsync -a "$src" "$dst"

  if [ -d "$OPENVELA_ROOT/apps/packages/demos" ]; then
    log "syncing smart_band app into apps/packages mirror"
    mkdir -p "$mirror"
    rsync -a "$src" "$mirror"
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

  if [ "$DRY_RUN" -eq 1 ]; then
    log "dry-run: would enable smart_band options in $defconfig"
    return 0
  fi

  DEFCONFIG_PATH="$defconfig"
  DEFCONFIG_BACKUP="$(mktemp "${TMPDIR:-/tmp}/smart-band-defconfig.XXXXXX")"
  cp -p -- "$DEFCONFIG_PATH" "$DEFCONFIG_BACKUP"
  DEFCONFIG_TRANSACTION_ACTIVE=1
  log "enabling smart_band config in $CONFIG_PATH/defconfig"
  append_unique_config "$defconfig" "CONFIG_GRAPHICS_LVGL" "y"
  append_unique_config "$defconfig" "CONFIG_LV_USE_NUTTX" "y"
  append_unique_config "$defconfig" "CONFIG_LV_USE_NUTTX_LIBUV" "y"
  append_unique_config "$defconfig" "CONFIG_SENSORS" "y"
  append_unique_config "$defconfig" "CONFIG_UORB" "y"
  append_unique_config "$defconfig" "CONFIG_LVX_USE_DEMO_SMART_BAND_BASIC" "y"
  append_unique_config "$defconfig" "CONFIG_LVX_DEMO_SMART_BAND_USE_SENSORS" "y"
  append_unique_config "$defconfig" "CONFIG_LVX_DEMO_SMART_BAND_BASIC_PRIORITY" "100"
  append_unique_config "$defconfig" "CONFIG_LVX_DEMO_SMART_BAND_BASIC_STACKSIZE" "32768"
  maybe_fail "after-enable-config"
}

build_openvela() {
  if [ "$SKIP_BUILD" -eq 1 ]; then
    log "build skipped by --skip-build"
    return 0
  fi

  if [ "$DRY_RUN" -eq 1 ]; then
    log "dry-run: would build openvela goldfish arm64 with $JOBS jobs"
    return 0
  fi

  cd "$OPENVELA_ROOT" || die "cannot cd to $OPENVELA_ROOT"
  log "building openvela goldfish arm64 with $JOBS jobs"

  if ./build.sh "$CONFIG_PATH" --cmake -j"$JOBS"; then
    log "cmake build completed"
    return 0
  fi

  log "cmake build failed; trying legacy build command"
  ./build.sh "$CONFIG_PATH" -j"$JOBS"

  if [ -f "$OPENVELA_ROOT/nuttx/nuttx" ]; then
    log "copying legacy build outputs into $OUTPUT_DIR"
    mkdir -p "$OPENVELA_ROOT/$OUTPUT_DIR"
    cp "$OPENVELA_ROOT/nuttx/nuttx" "$OPENVELA_ROOT/$OUTPUT_DIR/nuttx"
    for f in vela_system.bin vela_data.bin vela_ap.bin nuttx.bin nuttx.hex; do
      if [ -f "$OPENVELA_ROOT/nuttx/$f" ]; then
        cp "$OPENVELA_ROOT/nuttx/$f" "$OPENVELA_ROOT/$OUTPUT_DIR/$f"
      fi
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

  if [ "$DRY_RUN" -eq 1 ]; then
    log "dry-run: would serve browser demo at $url"
    return 0
  fi

  if command -v python3 >/dev/null 2>&1; then
    if ! curl --noproxy 127.0.0.1 -fsS "$url" >/dev/null 2>&1; then
      local server_pid
      log "serving browser demo at $url"
      nohup python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$DEMO_ROOT" > "$log_file" 2>&1 &
      server_pid=$!
      sleep 1
      if ! curl --noproxy 127.0.0.1 -fsS "$url" >/dev/null 2>&1; then
        kill "$server_pid" >/dev/null 2>&1 || true
        die "browser demo server did not become ready; see $log_file"
      fi
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
  preflight
  protect_mutation_targets
  ensure_claude_skills
  require_openvela_checkout
  verify_openvela_revision
  install_local_skill
  sync_demo_app
  enable_config
  build_openvela
  serve_browser_demo
  print_simulator_command
  log "done"
}

main "$@"
