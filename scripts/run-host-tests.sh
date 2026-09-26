#!/usr/bin/env bash
# Host tests of the pure logic in src/ (ghost filter, output policy). Needs a C compiler.
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$repo_root/build/host-tests"
mkdir -p "$out"
cc="${CC:-cc}"
status=0
for t in ghost_filter conn_policy led_logic; do
  "$cc" -std=c11 -Wall -Wextra -Werror -I"$repo_root/include" \
    "$repo_root/tests/host/test_$t.c" "$repo_root/src/$t.c" -o "$out/test_$t"
  "$out/test_$t" || status=1
done

# Boot invariant of the kscans in the built devicetree (after a build):
# ZMK reads each kscan twice before it sets the callback (resume, then
# enable), and its debouncer counts one scan period per read; with a press or
# release time of at most one scan period the second read flips the state and
# calls a NULL callback.
shopt -s nullglob
dts_files=("$repo_root"/build/firmware/*.dts)
if [[ ${#dts_files[@]} -eq 0 ]]; then
  echo "FAIL: no build/firmware/*.dts; build first (bash scripts/build-firmware.sh)"
  exit 1
fi
for dts in "${dts_files[@]}"; do
  echo "$(basename "$dts"):"
  python3 - "$dts" <<'PY2' || status=1
import re, sys
text = open(sys.argv[1]).read()
bad = 0
for m in re.finditer(r'(\w+): \w+ \{([^{}]*debounce-scan-period-ms[^{}]*)\}', text):
    body = m.group(2)
    val = lambda k: int(re.search(k + r' = < (0x[0-9a-f]+|\d+) >', body).group(1), 0)
    period, press, release = (val('debounce-scan-period-ms'), val('debounce-press-ms'),
                              val('debounce-release-ms'))
    ok = press > period and release > period
    print(f"  {m.group(1)}: period {period} press {press} release {release}: {'ok' if ok else 'FAIL'}")
    bad += not ok
sys.exit(1 if bad else 0)
PY2
done
exit $status
