#!/usr/bin/env bash
# Reproducible firmware build for the Keychron B1 Pro on upstream ZMK.
#
# Runs inside the pinned zmkfirmware/zmk-build-arm:4.1 image. The west
# workspace lives in workspace/firmware/ (not tracked by Git), this repository
# is mounted into it as `config`, and the outputs land in build/firmware/.
#
#   bash scripts/build-firmware.sh            # prepare (network) + build (offline)
#   bash scripts/build-firmware.sh prepare    # fetch the pinned sources
#   bash scripts/build-firmware.sh build      # build from the prepared workspace, no network
#
# The pins: config/west.yml (ZMK and Zephyr commits; Zephyr's own manifest
# pins its modules) and the image digest below. build/firmware/build-info.json
# records what was actually used.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
case "$repo_root" in
  *,*) echo "Repository path must not contain a comma (docker --mount syntax)" >&2; exit 1 ;;
esac

# zmkfirmware/zmk-build-arm:4.1, pinned by its multi-platform index digest
# (resolved on 2026-09-25); --platform selects the linux/amd64 image in it.
image_platform="linux/amd64"
image_ref="zmkfirmware/zmk-build-arm:4.1@sha256:edb1c953438c6f720ddb79c3762f3972013b7fbbaf4fff3592fc869983e7afc5"

board="keychron_b1_pro//zmk"
artifact="keychron-b1-pro"

mode="${1:-all}"
case "$mode" in
  all|prepare|build|in-container-prepare|in-container-build) ;;
  *) echo "usage: $0 [all|prepare|build]" >&2; exit 2 ;;
esac

workspace="$repo_root/workspace/firmware"
out_dir="$repo_root/build/firmware"

if [[ "$mode" != in-container-* ]]; then
  mkdir -p "$workspace/config" "$out_dir"

  repo_commit="$(git rev-parse HEAD 2>/dev/null || echo none)"
  # Zephyr embeds the build time through __DATE__/__TIME__ in some places;
  # GCC takes them from SOURCE_DATE_EPOCH, so equal commits give equal files.
  # BUILD_VERSION and SOURCE_DATE_EPOCH from the environment override the
  # values derived from git.
  source_date_epoch="${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct HEAD 2>/dev/null || echo 0)}"
  short="$(git rev-parse --short HEAD 2>/dev/null || echo none)"
  if [[ -n "$(git status --porcelain --untracked-files=no 2>/dev/null)" ]]; then
    repo_dirty=true
    build_version="${BUILD_VERSION:-kb1-$short-dirty}"
  else
    repo_dirty=false
    build_version="${BUILD_VERSION:-kb1-$short}"
  fi

  run_in_container() {
    local network="$1" step="$2"
    # The repository is mounted read-only as the workspace's manifest repository
    # (config); the workspace and the output directory are the only writable
    # mounts. Run as the invoking user so nothing on the host becomes root-owned.
    docker run --rm --platform "$image_platform" --network "$network" \
      --user "$(id -u):$(id -g)" \
      --mount "type=bind,source=$workspace,target=/workspace" \
      --mount "type=bind,source=$repo_root,target=/workspace/config,readonly" \
      --mount "type=bind,source=$out_dir,target=/out" \
      --workdir /workspace \
      --env HOME=/workspace/.home \
      --env BUILD_VERSION="$build_version" \
      --env SOURCE_DATE_EPOCH="$source_date_epoch" \
      --env REPO_COMMIT="$repo_commit" \
      --env REPO_DIRTY="$repo_dirty" \
      --env IMAGE_REF="$image_ref" \
      --env BOARD="$board" --env ARTIFACT="$artifact" \
      "$image_ref" bash /workspace/config/scripts/build-firmware.sh "in-container-$step"
  }

  case "$mode" in
    all)     run_in_container bridge prepare; run_in_container none build ;;
    prepare) run_in_container bridge prepare ;;
    build)   run_in_container none build ;;
  esac
  exit 0
fi

# ---------------------------------------------------------------- container
step="${mode#in-container-}"
mkdir -p "$HOME"
# The mounts are owned by the host user, which git treats as dubious ownership
# when the container's user id differs.
git config --global --add safe.directory '*'
cd /workspace

if [[ "$step" == prepare ]]; then
  if [[ ! -d .west ]]; then
    west init -l config --mf config/west.yml
  fi
  west update --narrow --fetch-opt=--depth=1
  west list -f '{name} {path} {sha}' > /workspace/.west-list
  exit 0
fi

# ---- build (offline)
[[ -d .west ]] || { echo "workspace not prepared; run: bash scripts/build-firmware.sh prepare" >&2; exit 1; }
# The checked-out sources must be the pinned ones.
west list -f '{name} {path} {sha}' > /workspace/.west-list
expected_zmk="$(sed -nE 's/^ *revision: ([0-9a-f]{40})$/\1/p' config/config/west.yml | sed -n 1p)"
expected_zephyr="$(sed -nE 's/^ *revision: ([0-9a-f]{40})$/\1/p' config/config/west.yml | sed -n 2p)"
grep -q "^zmk zmk $expected_zmk$" /workspace/.west-list || { echo "zmk checkout is not at the pinned commit" >&2; exit 1; }
grep -q "^zephyr zephyr $expected_zephyr$" /workspace/.west-list || { echo "zephyr checkout is not at the pinned commit" >&2; exit 1; }

build_dir="/workspace/build/keychron_b1_pro"
rm -rf "$build_dir"
export ZEPHYR_BASE=/workspace/zephyr
west zephyr-export >/dev/null
# studio-rpc-usb-uart: ZMK Studio over the USB CDC ACM serial port.
west build -s zmk/app -d "$build_dir" -b "$BOARD" -S studio-rpc-usb-uart -- \
  -DZMK_CONFIG=/workspace/config/config \
  -DBUILD_VERSION="$BUILD_VERSION" \
  2>&1 | tee /out/build.log
[[ "${PIPESTATUS[0]}" -eq 0 ]] || exit 1

# ---- collect outputs and the record of what was built
rm -f /out/"$ARTIFACT".uf2 /out/"$ARTIFACT".hex /out/"$ARTIFACT".elf /out/"$ARTIFACT".map
cp "$build_dir/zephyr/zmk.uf2" /out/"$ARTIFACT".uf2
cp "$build_dir/zephyr/zmk.hex" /out/"$ARTIFACT".hex
cp "$build_dir/zephyr/zmk.elf" /out/"$ARTIFACT".elf
cp "$build_dir/zephyr/zmk.map" /out/"$ARTIFACT".map
cp "$build_dir/zephyr/.config" /out/zephyr.config
cp "$build_dir/zephyr/zephyr.dts" /out/zephyr.dts
cp "$build_dir/zephyr_modules.txt" /out/zephyr_modules.txt

# The license texts and notices of everything linked into the image; fails if
# linked code has terms the notice does not cover.
python3 /workspace/config/scripts/third-party-notices.py "$build_dir" \
  /out/"$ARTIFACT".map /workspace/.west-list /out/THIRD-PARTY-NOTICES.txt

python3 - "$build_dir" <<'PY'
import hashlib, json, os, re, subprocess, sys
build_dir = sys.argv[1]
def sha(p): return hashlib.sha256(open(p, "rb").read()).hexdigest()
log = open("/out/build.log", encoding="utf-8", errors="replace").read()
def find(pattern):
    m = re.search(pattern, log, re.M); return m.group(1).strip() if m else None
projects = [l.split() for l in open("/workspace/.west-list").read().splitlines() if l.strip()]
art = os.environ["ARTIFACT"]
cache = open(build_dir + "/CMakeCache.txt").read()
info = {
    "repository_commit": os.environ["REPO_COMMIT"],
    "repository_dirty": os.environ["REPO_DIRTY"] == "true",
    "build_version": os.environ["BUILD_VERSION"],
    "source_date_epoch": int(os.environ["SOURCE_DATE_EPOCH"]),
    "board": os.environ["BOARD"],
    "zmk_config": "/workspace/config/config",
    "keymap_file": find(r"^-- Using keymap file: (.*)$"),
    "west_projects": [{"name": n, "path": p, "sha": s} for n, p, s in projects],
    "container_image": os.environ["IMAGE_REF"],
    "west_version": subprocess.check_output(["west", "--version"]).decode().split()[-1],
    "cmake_version": subprocess.check_output(["cmake", "--version"]).decode().split()[2],
    "compiler": subprocess.check_output([re.search(r"^CMAKE_C_COMPILER:\w+=(.*)$", cache, re.M).group(1), "--version"]).decode().splitlines()[0],
    "zephyr_modules": [l.split('":"')[0].strip('"') for l in open("/out/zephyr_modules.txt").read().splitlines() if l.strip()],
    "artifacts": {f: sha("/out/" + f)
                  for f in (art + ".uf2", art + ".hex", art + ".elf", "THIRD-PARTY-NOTICES.txt")},
}

# Keycode listeners in the order the event manager calls them (their
# subscriptions' addresses in the .event_subscription section). US-JIS must
# come after the behaviors that re-raise keycode events and right before
# hid_listener (docs/usjis-architecture.md section 6); the firmware checks
# this at startup too, but a release build does not log the result.
nm = re.search(r"^CMAKE_NM:\w+=(.*)$", cache, re.M).group(1)
prefix, suffix = "zmk_event_sub_", "zmk_keycode_state_changed"
listeners = [l.split()[2][len(prefix):-len(suffix)]
             for l in subprocess.check_output([nm, "-n", build_dir + "/zephyr/zmk.elf"]).decode().splitlines()
             if len(l.split()) == 3 and l.split()[2].startswith(prefix) and l.split()[2].endswith(suffix)]
info["keycode_listeners"] = listeners
json.dump(info, open("/out/build-info.json", "w"), indent=2)
if "usjis" in listeners:
    i = listeners.index("usjis")
    before = ("behavior_hold_tap", "behavior_sticky_key", "behavior_caps_word", "behavior_key_repeat")
    if listeners[i + 1:i + 2] != ["hid_listener"] or any(
            n in listeners[i:] for n in before):
        sys.exit("US-JIS listener order wrong: " + " ".join(listeners))
    print("keycode listeners: " + " ".join(listeners))
print(json.dumps({k: info[k] for k in ("build_version", "keymap_file", "artifacts")}, indent=2))
PY
