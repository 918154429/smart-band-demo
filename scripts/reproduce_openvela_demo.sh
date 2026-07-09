#!/usr/bin/env bash
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEMO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

exec "$DEMO_ROOT/skills/openvela-smart-band-reproduce/scripts/reproduce.sh" "$@"
