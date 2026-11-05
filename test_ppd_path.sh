#!/bin/sh
set -u

APP=./legacy-printer-app
if [ ! -x "$APP" ]; then
  echo "FAIL legacy-printer-app is not built" >&2
  exit 1
fi
if ! command -v curl >/dev/null 2>&1; then
  echo "FAIL curl is required" >&2
  exit 1
fi

BASE=$(mktemp -d)
SRV=
cleanup() {
  if [ -n "$SRV" ]; then
    kill "$SRV" 2>/dev/null || true
    wait "$SRV" 2>/dev/null || true
  fi
  rm -rf "$BASE"
}
trap cleanup EXIT

mkdir -p "$BASE/state" "$BASE/spool" "$BASE/ppd" "$BASE/filter" "$BASE/backend" "$BASE/outside"
printf 'sentinel\n' > "$BASE/outside/sentinel"
printf 'sentinel-b\n' > "$BASE/outside/sentinel-b"
printf 'kept\n' > "$BASE/ppd/kept.ppd"
PORT=$((20000 + ($$ % 20000)))

"$APP" server \
  -o server-port="$PORT" \
  -o server-options=no-tls \
  -o listen-hostname=127.0.0.1 \
  -o user-ppd-directory="$BASE/ppd" \
  -o state-directory="$BASE/state" \
  -o spool-directory="$BASE/spool" \
  -o filter-directory="$BASE/filter" \
  -o backend-directory="$BASE/backend" \
  -o log-level=debug \
  -o log-file="$BASE/server.log" \
  >"$BASE/stdout.log" 2>&1 &
SRV=$!

i=0
while [ "$i" -lt 50 ]; do
  if curl -sS -o "$BASE/page.html" "http://127.0.0.1:$PORT/addppd" 2>/dev/null; then
    break
  fi
  i=$((i + 1))
  sleep 0.2
done
if [ ! -s "$BASE/page.html" ]; then
  echo "FAIL server did not serve /addppd" >&2
  cat "$BASE/stdout.log" >&2 || true
  exit 1
fi

SESSION=$(sed -n 's/.*name="session" value="\([^"]*\)".*/\1/p' "$BASE/page.html" | head -1)
if [ -z "$SESSION" ]; then
  echo "FAIL form has no session field" >&2
  exit 1
fi

{
  printf 'session=%s&action=delete-ppdfiles&' "$SESSION"
  printf '%%09..%%2Foutside%%2Fsentinel=on&'
  printf '%%09foo%%2F..%%2F..%%2Foutside%%2Fsentinel-b=on&'
  printf '%%09kept.ppd=on'
} > "$BASE/delete.body"

curl -sS -o "$BASE/delete.html" \
  -H 'Content-Type: application/x-www-form-urlencoded' \
  --data-binary @"$BASE/delete.body" \
  "http://127.0.0.1:$PORT/addppd"

fail=0
if [ ! -f "$BASE/outside/sentinel" ]; then
  echo "FAIL ../outside/sentinel was deleted"
  fail=1
else
  echo "PASS ../outside/sentinel was left in place"
fi
if [ ! -f "$BASE/outside/sentinel-b" ]; then
  echo "FAIL foo/../../outside/sentinel-b was deleted"
  fail=1
else
  echo "PASS foo/../../outside/sentinel-b was left in place"
fi
if [ -f "$BASE/ppd/kept.ppd" ]; then
  echo "FAIL kept.ppd was not deleted"
  fail=1
else
  echo "PASS kept.ppd was deleted"
fi

curl -sS -o "$BASE/page2.html" "http://127.0.0.1:$PORT/addppd"
SESSION=$(sed -n 's/.*name="session" value="\([^"]*\)".*/\1/p' "$BASE/page2.html" | head -1)

boundary=ppdbound
{
  printf -- '--%s\r\n' "$boundary"
  printf 'Content-Disposition: form-data; name="session"\r\n\r\n%s\r\n' "$SESSION"
  printf -- '--%s\r\n' "$boundary"
  printf 'Content-Disposition: form-data; name="action"\r\n\r\nadd-ppdfiles\r\n'
  printf -- '--%s\r\n' "$boundary"
  printf 'Content-Disposition: form-data; name="ppdfiles"; filename="../outside/uploaded-marker.ppd"\r\n'
  printf 'Content-Type: application/octet-stream\r\n\r\n'
  printf 'not-a-ppd\r\n'
  printf -- '--%s--\r\n' "$boundary"
} > "$BASE/upload-bad.body"

curl -sS -o "$BASE/upload-bad.html" \
  -H "Content-Type: multipart/form-data; boundary=$boundary" \
  --data-binary @"$BASE/upload-bad.body" \
  "http://127.0.0.1:$PORT/addppd"

if [ -f "$BASE/outside/uploaded-marker.ppd" ]; then
  echo "FAIL upload left ../outside/uploaded-marker.ppd"
  fail=1
else
  echo "PASS upload left no outside file"
fi
if grep -F "Creating file: $BASE/ppd/../outside/uploaded-marker.ppd" "$BASE/server.log" >/dev/null; then
  echo "FAIL upload opened a path outside the PPD directory"
  fail=1
else
  echo "PASS upload did not open a path outside the PPD directory"
fi

curl -sS -o "$BASE/page3.html" "http://127.0.0.1:$PORT/addppd"
SESSION=$(sed -n 's/.*name="session" value="\([^"]*\)".*/\1/p' "$BASE/page3.html" | head -1)
{
  printf -- '--%s\r\n' "$boundary"
  printf 'Content-Disposition: form-data; name="session"\r\n\r\n%s\r\n' "$SESSION"
  printf -- '--%s\r\n' "$boundary"
  printf 'Content-Disposition: form-data; name="action"\r\n\r\nadd-ppdfiles\r\n'
  printf -- '--%s\r\n' "$boundary"
  printf 'Content-Disposition: form-data; name="ppdfiles"; filename="plain.ppd"\r\n'
  printf 'Content-Type: application/octet-stream\r\n\r\n'
  printf 'not-a-ppd\r\n'
  printf -- '--%s--\r\n' "$boundary"
} > "$BASE/upload-ok.body"

curl -sS -o "$BASE/upload-ok.html" \
  -H "Content-Type: multipart/form-data; boundary=$boundary" \
  --data-binary @"$BASE/upload-ok.body" \
  "http://127.0.0.1:$PORT/addppd"

if grep -F "Creating file: $BASE/ppd/plain.ppd" "$BASE/server.log" >/dev/null; then
  echo "PASS plain.ppd was opened inside the PPD directory"
else
  echo "FAIL plain.ppd was not opened inside the PPD directory"
  fail=1
fi
if [ -f "$BASE/outside/plain.ppd" ]; then
  echo "FAIL plain.ppd landed outside the PPD directory"
  fail=1
fi

if [ "$fail" -ne 0 ]; then
  echo "---- server.log ----" >&2
  cat "$BASE/server.log" >&2 || true
fi
exit "$fail"
