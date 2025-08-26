#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="$ROOT_DIR/bin/c-http-server"
LOG="$ROOT_DIR/tests/integration/server.log"

if [ ! -x "$BIN" ]; then
  echo "Binary not found: $BIN"
  exit 2
fi

# start server in background
rm -f "$LOG"
"$BIN" > "$LOG" 2>&1 &
PID=$!
trap 'kill $PID 2>/dev/null || true; wait $PID 2>/dev/null || true' EXIT

# wait for server to listen
for i in $(seq 1 20); do
  if nc -z 127.0.0.1 8080; then
    break
  fi
  sleep 0.1
done

# basic requests
curl -sS -D - http://127.0.0.1:8080/ | sed -n '1,5p'
HTTP_OK=$(curl -sS -o /dev/null -w "%{http_code}" http://127.0.0.1:8080/)
if [ "$HTTP_OK" != "200" ]; then
  echo "expected 200, got $HTTP_OK"
  exit 1
fi

# test tiny file
curl -sS http://127.0.0.1:8080/ -o /dev/null || { echo "request failed"; exit 1; }

echo "Integration tests passed"

# exit trap will clean up server
