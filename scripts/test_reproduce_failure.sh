#!/usr/bin/env bash
set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEMO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TMP_BASE="${TMPDIR:-/tmp}"
TMP_ROOT="$(mktemp -d "$TMP_BASE/smart-band-reproduce-test.XXXXXX")"

cleanup() {
  case "$TMP_ROOT" in
    "$TMP_BASE"/smart-band-reproduce-test.*) rm -rf -- "$TMP_ROOT" ;;
    *) printf 'refusing to remove unexpected test path: %s\n' "$TMP_ROOT" >&2 ;;
  esac
}
trap cleanup EXIT

make_commit() {
  local repo="$1"
  local marker="$2"

  mkdir -p "$repo"
  git -C "$repo" init -q
  git -C "$repo" config user.name "smart-band test"
  git -C "$repo" config user.email "smart-band-test@example.invalid"
  printf '%s\n' "$marker" > "$repo/marker.txt"
  git -C "$repo" add marker.txt
  git -C "$repo" commit -qm "$marker"
  git -C "$repo" rev-parse HEAD
}

OPENVELA_ROOT="$TMP_ROOT/openvela"
FAKE_BIN="$TMP_ROOT/bin"
mkdir -p \
  "$OPENVELA_ROOT/vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap" \
  "$OPENVELA_ROOT/packages/demos" \
  "$OPENVELA_ROOT/.repo" \
  "$FAKE_BIN"

cat > "$FAKE_BIN/rsync" <<'RSYNC'
#!/usr/bin/env bash
set -euo pipefail
[ "$#" -eq 3 ] && [ "$1" = "-a" ]
mkdir -p -- "$3"
cp -a -- "${2%/}/." "${3%/}/"
RSYNC
chmod +x "$FAKE_BIN/rsync"

CLAUDE_REVISION="$(make_commit "$OPENVELA_ROOT/.claude" "claude pin")"
OPENVELA_REVISION="$(make_commit "$OPENVELA_ROOT/.repo/manifests" "openvela pin")"
cp "$OPENVELA_ROOT/.repo/manifests/marker.txt" \
  "$OPENVELA_ROOT/.repo/manifest.xml"
printf '#!/usr/bin/env bash\nexit 0\n' > "$OPENVELA_ROOT/build.sh"
chmod +x "$OPENVELA_ROOT/build.sh"

DEFCONFIG="$OPENVELA_ROOT/vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap/defconfig"
printf '%s\n' \
  '# original fixture' \
  '# CONFIG_LVX_USE_DEMO_SMART_BAND_BASIC is not set' > "$DEFCONFIG"
cp "$DEFCONFIG" "$TMP_ROOT/defconfig.before"

set +e
PATH="$FAKE_BIN:$PATH" \
SMART_BAND_CLAUDE_REVISION="$CLAUDE_REVISION" \
SMART_BAND_OPENVELA_MANIFEST_REVISION="$OPENVELA_REVISION" \
SMART_BAND_OPENVELA_MANIFEST_FILE=marker.txt \
SMART_BAND_REPRODUCE_FAIL_AT=after-enable-config \
  bash "$DEMO_ROOT/scripts/reproduce_openvela_demo.sh" \
    --openvela-root "$OPENVELA_ROOT" --skip-build --no-browser \
    > "$TMP_ROOT/reproduce.log" 2>&1
status=$?
set -e

if [ "$status" -eq 0 ]; then
  printf 'expected injected reproduction failure, got success\n' >&2
  cat "$TMP_ROOT/reproduce.log" >&2
  exit 1
fi

if ! grep -q 'injected failure at after-enable-config' \
    "$TMP_ROOT/reproduce.log" ||
   ! grep -q 'restored defconfig after failure' "$TMP_ROOT/reproduce.log"; then
  printf 'failure injection did not reach and roll back the defconfig transaction\n' >&2
  cat "$TMP_ROOT/reproduce.log" >&2
  exit 1
fi

if ! cmp -s "$TMP_ROOT/defconfig.before" "$DEFCONFIG"; then
  printf 'defconfig was not restored after injected failure\n' >&2
  diff -u "$TMP_ROOT/defconfig.before" "$DEFCONFIG" >&2 || true
  exit 1
fi

if grep -E --line-number 'rsync[[:space:]].*--delete' \
    "$DEMO_ROOT/scripts/reproduce_openvela_demo.sh" \
    "$DEMO_ROOT/skills/openvela-smart-band-reproduce/scripts/reproduce.sh"; then
  printf 'destructive rsync delete flag remains in reproduction scripts\n' >&2
  exit 1
fi

printf 'reproduction failure rollback test passed\n'
