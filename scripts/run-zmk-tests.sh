#!/usr/bin/env bash
# ZMK tests of this repository's firmware code: each directory in tests/zmk/
# with a native_sim.keymap is built for ZMK's native_sim test board and run
# with mock key events, as ZMK's own app/tests are; the filtered log must
# equal keycode_events.snapshot. tests/zmk/support logs every HID report.
#
#   bash scripts/run-zmk-tests.sh            # all cases
#   bash scripts/run-zmk-tests.sh usjis-table
#
# Uses the workspace and the pinned image of scripts/build-firmware.sh (run
# its prepare step first); runs without network. Outputs: build/zmk-tests/.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

if [[ "${1:-}" != in-container ]]; then
  python3 tests/zmk/generate.py --check
  image_ref="$(sed -n 's/^image_ref="\(.*\)"$/\1/p' scripts/build-firmware.sh)"
  image_platform="$(sed -n 's/^image_platform="\(.*\)"$/\1/p' scripts/build-firmware.sh)"
  workspace="$repo_root/workspace/firmware"
  out_dir="$repo_root/build/zmk-tests"
  [[ -d "$workspace/.west" ]] || { echo "workspace not prepared; run: bash scripts/build-firmware.sh prepare" >&2; exit 1; }
  rm -rf "$out_dir"
  mkdir -p "$out_dir"
  exec docker run --rm --platform "$image_platform" --network none \
    --user "$(id -u):$(id -g)" \
    --mount "type=bind,source=$workspace,target=/workspace" \
    --mount "type=bind,source=$repo_root,target=/workspace/config,readonly" \
    --mount "type=bind,source=$out_dir,target=/out" \
    --workdir /workspace \
    --env HOME=/workspace/.home \
    "$image_ref" bash /workspace/config/scripts/run-zmk-tests.sh in-container "${@}"
fi

# ---------------------------------------------------------------- container
shift
mkdir -p "$HOME"
git config --global --add safe.directory '*'
cd /workspace
export ZEPHYR_BASE=/workspace/zephyr
west zephyr-export >/dev/null

run_case() {
  local dir="$1" name
  name="$(basename "$dir")"
  local build_dir="/workspace/build/zmk-tests/$name"
  if ! west build -s zmk/app -d "$build_dir" -b native_sim//zmk_test_mock -p -- \
      -DCONFIG_ASSERT=y -DZMK_CONFIG="$dir" \
      -DZMK_EXTRA_MODULES=/workspace/config/tests/zmk/support >"/out/$name.build.log" 2>&1; then
    echo "FAILED: $name did not build (build/zmk-tests/$name.build.log)"
    return 1
  fi
  "$build_dir/zephyr/zmk.exe" | sed -e 's/.*> //' >"/out/$name.full.log"
  sed -n -f "$dir/events.patterns" "/out/$name.full.log" >"/out/$name.log"
  if diff -u "$dir/keycode_events.snapshot" "/out/$name.log" >"/out/$name.diff"; then
    echo "PASS: $name ($(wc -l <"/out/$name.log") lines)"
  else
    echo "FAILED: $name (build/zmk-tests/$name.diff)"
    head -40 "/out/$name.diff"
    return 1
  fi
}

status=0
for dir in /workspace/config/tests/zmk/*/; do
  dir="${dir%/}"
  [[ -f "$dir/native_sim.keymap" ]] || continue
  if [[ $# -gt 0 ]] && [[ ! " $* " == *" $(basename "$dir") "* ]]; then
    continue
  fi
  run_case "$dir" || status=1
done
exit $status
