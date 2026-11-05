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

mkdir -p "$BASE/state" "$BASE/drvstate" "$BASE/spool" "$BASE/ppd" "$BASE/userppd" "$BASE/filter" "$BASE/backend"
cat > "$BASE/ppd/installable.ppd" << 'EOF'
*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "INSTALL.PPD"
*Product: "(Installable Printer)"
*Manufacturer: "Test"
*ModelName: "Test Installable Printer"
*NickName: "Test Installable Printer"
*ShortNickName: "Test Installable"
*1284DeviceID: "MFG:Test;MDL:Installable Printer;CMD:POSTSCRIPT;"
*PSVersion: "(3010) 0"
*LanguageLevel: "3"
*ColorDevice: False
*DefaultColorSpace: Gray
*OpenGroup: InstallableOptions/Installable Options
*OpenUI *Duplexer/Duplex Unit: Boolean
*DefaultDuplexer: False
*Duplexer True/True: ""
*Duplexer False/False: ""
*CloseUI: *Duplexer
*CloseGroup: InstallableOptions
*OpenUI *PageSize/Page Size: PickOne
*DefaultPageSize: Letter
*PageSize Letter/US Letter: "<</PageSize[612 792]>>setpagedevice"
*CloseUI: *PageSize
*OpenUI *PageRegion: PickOne
*DefaultPageRegion: Letter
*PageRegion Letter/US Letter: "<</PageSize[612 792]>>setpagedevice"
*CloseUI: *PageRegion
*DefaultImageableArea: Letter
*ImageableArea Letter/US Letter: "18 36 594 756"
*DefaultPaperDimension: Letter
*PaperDimension Letter/US Letter: "612 792"
EOF
cat > "$BASE/backend/dummy" << 'EOF'
#!/bin/sh
if [ $# -eq 0 ]; then
  echo 'direct dummy:/x "Dummy" "Dummy Printer" "MFG:Test;MDL:Installable Printer;CMD:POSTSCRIPT;" ""'
  exit 0
fi
cat > /dev/null
EOF
chmod 755 "$BASE/backend/dummy"
PORT=$((20000 + ($$ % 20000)))

"$APP" server \
  -o server-port="$PORT" \
  -o server-options=no-tls \
  -o listen-hostname=127.0.0.1 \
  -o ppd-directories="$BASE/ppd" \
  -o user-ppd-directory="$BASE/userppd" \
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
  if curl -sS -o /dev/null "http://127.0.0.1:$PORT/" 2>/dev/null; then
    break
  fi
  i=$((i + 1))
  sleep 0.2
done

DRIVER=$("$APP" drivers \
  -o ppd-directories="$BASE/ppd" \
  -o user-ppd-directory="$BASE/userppd" \
  -o state-directory="$BASE/drvstate" \
  -o spool-directory="$BASE/spool" \
  -o filter-directory="$BASE/filter" \
  -o backend-directory="$BASE/backend" \
  2>/dev/null | awk '/[Ii]nstallable/ {print $1; exit}')
if [ -z "$DRIVER" ] || ! "$APP" add -d test -m "$DRIVER" -v 'cups:dummy:/x'; then
  echo "FAIL could not add a printer with the test PPD (driver '$DRIVER')" >&2
  cat "$BASE/stdout.log" >&2 || true
  exit 1
fi

URL="http://127.0.0.1:$PORT/test/device"
post() {
  curl -sS -o "$BASE/page.html" "$URL"
  SESSION=$(sed -n 's/.*name="session" value="\([^"]*\)".*/\1/p' "$BASE/page.html" | head -1)
  curl -sS -o "$BASE/$1.html" -w '%{http_code}' \
    -H 'Content-Type: application/x-www-form-urlencoded' \
    --data-binary "session=$SESSION&action=set-installable&$2" \
    "$URL" 2>/dev/null
}

fail=0
curl -sS -o "$BASE/page.html" "$URL"
if grep -F "$(printf 'name="\t\tDuplexer\tTrue"')" "$BASE/page.html" >/dev/null; then
  echo "PASS device page has the Duplexer check box"
else
  echo "FAIL device page has no Duplexer check box"
  fail=1
fi

code=$(post valid '%09%09Duplexer%09True=on')
if [ "$code" = 200 ] && grep -F 'Installable accessory configuration saved.' "$BASE/valid.html" >/dev/null; then
  echo "PASS checked Duplexer box was saved"
else
  echo "FAIL checked Duplexer box was not saved (HTTP $code)"
  fail=1
fi

code=$(post malformed '%09%09Duplexer=on')
if [ "$code" = 200 ] && kill -0 "$SRV" 2>/dev/null; then
  echo "PASS server answered a check box field with no choice"
else
  echo "FAIL server did not answer a check box field with no choice (HTTP $code)"
  fail=1
fi

code=$(curl -sS -o /dev/null -w '%{http_code}' "$URL" 2>/dev/null)
if [ "$code" = 200 ]; then
  echo "PASS server still serves the device page"
else
  echo "FAIL server no longer serves the device page (HTTP $code)"
  fail=1
fi

if [ "$fail" -ne 0 ]; then
  echo "---- server.log ----" >&2
  tail -n 40 "$BASE/server.log" >&2 || true
fi
exit "$fail"
