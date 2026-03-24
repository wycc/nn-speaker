#!/bin/zsh

set -euo pipefail

PORT="${PORT:-/dev/cu.usbserial-1110}"
PIO_BIN="${PIO_BIN:-$HOME/.platformio/penv/bin/pio}"
BAUD="${BAUD:-115200}"

release_port() {
  local pids
  pids=$(lsof -t "$PORT" 2>/dev/null | tr '\n' ' ' | xargs echo -n || true)

  if [[ -n "${pids}" ]]; then
    echo "Releasing $PORT from PID(s): $pids"
    kill $pids 2>/dev/null || true
    sleep 1

    local remaining
    remaining=$(lsof -t "$PORT" 2>/dev/null | tr '\n' ' ' | xargs echo -n || true)
    if [[ -n "${remaining}" ]]; then
      echo "Force killing remaining PID(s): $remaining"
      kill -9 $remaining 2>/dev/null || true
      sleep 1
    fi
  fi
}

upload() {
  release_port
  "$PIO_BIN" run -t upload
}

monitor() {
  release_port
  "$PIO_BIN" device monitor -p "$PORT" -b "$BAUD"
}

upload_monitor() {
  upload
  sleep 2
  monitor
}

case "${1:-}" in
  upload)
    upload
    ;;
  monitor)
    monitor
    ;;
  both)
    upload_monitor
    ;;
  *)
    echo "Usage: ./serial_helper.sh [upload|monitor|both]"
    echo "Optional env: PORT=/dev/cu.usbserial-1110 BAUD=115200"
    exit 1
    ;;
esac
