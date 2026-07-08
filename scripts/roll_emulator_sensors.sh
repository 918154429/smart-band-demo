#!/usr/bin/env bash
set -euo pipefail

HOST="${EMULATOR_CONSOLE_HOST:-127.0.0.1}"
PORT="${EMULATOR_CONSOLE_PORT:-5554}"
AUTH_TOKEN_FILE="${EMULATOR_AUTH_TOKEN_FILE:-$HOME/.emulator_console_auth_token}"
DELAY_SECONDS="${SMART_BAND_ROLL_DELAY:-2}"
LOOPS="${SMART_BAND_ROLL_LOOPS:-0}"

if ! command -v nc >/dev/null 2>&1; then
  echo "nc is required to talk to the emulator console" >&2
  exit 1
fi

if [ ! -r "$AUTH_TOKEN_FILE" ]; then
  echo "Cannot read emulator auth token: $AUTH_TOKEN_FILE" >&2
  exit 1
fi

AUTH_TOKEN="$(tr -d '\r\n' < "$AUTH_TOKEN_FILE")"

heart_rates=(72 88 104 118 82 96)
battery_levels=(100 96 92 88 94 99)
wrist_tilts=(0 1 0 1 0 1)
accelerations=(
  "0:9.81:0"
  "3.6:7.4:1.2"
  "-3.2:9.1:0.4"
  "4.4:6.8:1.7"
  "-2.8:9.5:0.9"
  "0:9.81:0"
)

emit_console_commands() {
  local loop=0
  local index=0
  local count="${#heart_rates[@]}"

  printf 'auth %s\n' "$AUTH_TOKEN"
  printf 'sensor status\n'

  while [ "$LOOPS" -eq 0 ] || [ "$loop" -lt "$LOOPS" ]; do
    index=$((loop % count))

    echo "roll[$loop]: heart=${heart_rates[$index]} accel=${accelerations[$index]} battery=${battery_levels[$index]} wrist=${wrist_tilts[$index]}" >&2

    printf 'sensor set heart-rate %s\n' "${heart_rates[$index]}"
    printf 'sensor set acceleration %s\n' "${accelerations[$index]}"
    printf 'sensor set wrist-tilt %s\n' "${wrist_tilts[$index]}"
    printf 'power capacity %s\n' "${battery_levels[$index]}"
    printf 'sensor get heart-rate\n'
    printf 'sensor get acceleration\n'
    printf 'power display\n'

    loop=$((loop + 1))
    sleep "$DELAY_SECONDS"
  done

  printf 'quit\n'
}

emit_console_commands | nc "$HOST" "$PORT"
