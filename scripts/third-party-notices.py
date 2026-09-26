#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Writes THIRD-PARTY-NOTICES.txt for a firmware build.

Runs in the build container (scripts/build-firmware.sh) after the build:

    third-party-notices.py <build dir> <map file> <west list> <output>

It finds every object file that the linker put into the image (from the map
file), maps it to its source (compile_commands.json) and reads the source's
license (SPDX tag, or a known file). The notice lists each component with its
license and includes the license texts, and the per-file notices of files
under terms other than their component's. A linked source whose license is
not one of those handled here stops the build, so that a change of the pinned
sources cannot add terms that the notice does not cover.
"""

import json
import os
import re
import sys

ALLOWED = {"Apache-2.0", "MIT", "BSD-3-Clause", "Zlib", "CC0-1.0",
           "Apache-2.0 OR GPL-2.0-or-later"}

# Sources without an SPDX tag whose license is known.
KNOWN = [
    (r"^/workspace/modules/lib/nanopb/", "Zlib"),
    (r"/proto/zmk/[a-z_]+\.pb\.c$", "MIT"),  # generated from zmk-studio-messages
    (r"^/workspace/zephyr/misc/empty_file\.c$", "Apache-2.0"),
    (r"/zephyr/misc/generated/configs\.c$", "Apache-2.0"),
    (r"^/workspace/build/[^/]+/zephyr/isr_tables\.c$", "Apache-2.0"),  # gen_isr_tables.py
    # 3-clause BSD text in the header, no SPDX tag.
    (r"^/workspace/zephyr/subsys/usb/device/usb_device\.c$", "BSD-3-Clause"),
    (r"^/workspace/zephyr/subsys/usb/device/class/cdc_acm\.c$", "BSD-3-Clause"),
]

# (name, source path pattern, license of the component, license file, west path)
COMPONENTS = [
    ("This firmware (keymap, board and modules of this repository)", r"^/workspace/config/", "MIT", "/workspace/config/LICENSE",
     None),
    ("ZMK", r"^/workspace/zmk/", "MIT", "/workspace/zmk/LICENSE", "zmk"),
    ("Zephyr RTOS", r"^/workspace/zephyr/|^/workspace/build/[^/]+/zephyr/", "Apache-2.0",
     "/workspace/zephyr/LICENSE", "zephyr"),
    ("Mbed TLS (used under Apache-2.0)", r"^/workspace/modules/crypto/mbedtls/",
     "Apache-2.0 OR GPL-2.0-or-later", "/workspace/modules/crypto/mbedtls/LICENSE",
     "modules/crypto/mbedtls"),
    ("nrfx and nRF MDK (Nordic Semiconductor)", r"^/workspace/modules/hal/nordic/",
     "BSD-3-Clause", None, "modules/hal/nordic"),
    ("nanopb", r"^/workspace/modules/lib/nanopb/", "Zlib",
     "/workspace/modules/lib/nanopb/LICENSE.txt", "modules/lib/nanopb"),
    ("ZMK Studio messages (generated nanopb code)", r"^/workspace/build/[^/]+/proto/zmk/",
     "MIT", "/workspace/modules/msgs/zmk-studio-messages/LICENSE",
     "modules/msgs/zmk-studio-messages"),
]


def fail(msg):
    sys.exit("third-party-notices: " + msg)


def read(path, limit=None):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read(limit) if limit else f.read()


def license_of(src):
    for pattern, lic in KNOWN:
        if re.search(pattern, src):
            return lic
    m = re.search(r"SPDX-License-Identifier:\s*([^\n*]+?)\s*(\*/)?\s*$", read(src, 4000), re.M)
    return m.group(1).strip() if m else None


def header_comment(src):
    """The first comment block of a source file: its copyright and license."""
    m = re.match(r"\s*/\*(.*?)\*/", read(src, 8000), re.S)
    if not m:
        return None
    lines = [re.sub(r"^\s*\* ?", "", l).rstrip() for l in m.group(1).splitlines()]
    return "\n".join(lines).strip()


def component_of(src):
    for comp in COMPONENTS:
        if re.search(comp[1], src):
            return comp
    return None


def main():
    build_dir, map_file, west_list, out = sys.argv[1:5]
    cache = read(os.path.join(build_dir, "CMakeCache.txt"))
    compiler = re.search(r"^CMAKE_C_COMPILER:\w+=(.*)$", cache, re.M).group(1)
    sdk_target = os.path.dirname(os.path.dirname(compiler))  # .../arm-zephyr-eabi
    sdk_licenses = os.path.join(sdk_target, "share", "licenses")

    # Objects that contribute bytes to the image, from the map file.
    # Skip the list of discarded input sections at the top of the map.
    map_text = read(map_file)
    start = map_text.find("Linker script and memory map")
    if start < 0:
        fail("unexpected map file format")
    map_text = map_text[start:]
    linked, toolchain = set(), set()
    for m in re.finditer(r"0x[0-9a-f]+\s+0x([0-9a-f]+)\s+(\S+?)(?:\((\S+?)\))?$", map_text, re.M):
        if int(m.group(1), 16) == 0:
            continue
        path, member = m.group(2), m.group(3)
        if path.startswith(sdk_target) or "/zephyr-sdk" in path:
            toolchain.add(os.path.basename(path))
        elif member and member.endswith(".obj"):
            linked.add(member)
        elif path.endswith(".obj"):
            linked.add(os.path.basename(path))

    sources_by_obj = {}
    for entry in json.load(open(os.path.join(build_dir, "compile_commands.json"))):
        out_m = re.search(r"-o (\S+)", entry["command"])
        if out_m:
            sources_by_obj.setdefault(os.path.basename(out_m.group(1)), set()).add(entry["file"])

    per_component, extra_notices, problems = {}, {}, []
    for obj in sorted(linked):
        for src in sorted(sources_by_obj.get(obj, ())):
            lic = license_of(src)
            comp = component_of(src)
            if lic not in ALLOWED:
                problems.append(f"{src}: license {lic!r}")
                continue
            if comp is None:
                problems.append(f"{src}: no component for this path")
                continue
            per_component.setdefault(comp[0], set()).add(lic)
            if lic != comp[2] and lic not in ("Apache-2.0", "CC0-1.0"):
                text = header_comment(src)
                if not text:
                    problems.append(f"{src}: {lic} without a notice to reproduce")
                    continue
                extra_notices.setdefault(text, []).append(src.replace("/workspace/", ""))
            elif comp[3] is None:
                extra_notices.setdefault(header_comment(src), []).append(
                    src.replace("/workspace/", ""))
        if obj not in sources_by_obj:
            problems.append(f"{obj}: no source found")
    unknown_tc = toolchain - {"libc.a", "libm.a", "libgcc.a"}
    if unknown_tc:
        problems.append(f"toolchain libraries not covered: {sorted(unknown_tc)}")
    if problems:
        fail("linked code not covered by the notice:\n  " + "\n  ".join(problems))

    shas = {}
    for line in read(west_list).splitlines():
        parts = line.split()
        if len(parts) == 3:
            shas[parts[1]] = parts[2]


    o = []
    o.append("Third-party notices for this firmware image\n"
             "===========================================\n\n"
             "The firmware image contains the following components. Their license\n"
             "texts and notices follow. Generated at build time from the linked\n"
             "object files (scripts/third-party-notices.py).\n")
    for name, _, lic, lic_file, west_path in COMPONENTS:
        if name in per_component:
            extra = sorted(per_component[name] - {lic})
            rev = shas.get(west_path, "") if west_path else ""
            o.append(f"- {name}: {lic}"
                     + (f" (some files: {', '.join(extra)})" if extra else "")
                     + (f", commit {rev}" if rev else ""))
    if {"libc.a", "libm.a"} & toolchain:
        o.append("- picolibc (C library of the Zephyr SDK): BSD-style licenses, see below")
    if "libgcc.a" in toolchain:
        o.append("- libgcc (GCC runtime library): GPL-3.0-or-later WITH GCC-exception-3.1")
    o.append("")

    def section(title, text):
        o.append("\n" + "=" * 78 + "\n" + title + "\n" + "=" * 78 + "\n\n" + text.strip() + "\n")

    for name, _, lic, lic_file, _ in COMPONENTS:
        if name in per_component and lic_file:
            section(f"{name}: {lic}", read(lic_file))
    for text, files in extra_notices.items():
        section("Notice of: " + ", ".join(sorted(files)), text)
    if {"libc.a", "libm.a"} & toolchain:
        for f in ("COPYING.picolibc", "COPYING.NEWLIB"):
            path = os.path.join(sdk_licenses, "picolibc", f)
            if not os.path.isfile(path):
                fail(f"missing {path}")
            section(f"picolibc: {f}", read(path))
    if "libgcc.a" in toolchain:
        path = os.path.join(sdk_licenses, "gcc", "COPYING.RUNTIME")
        if not os.path.isfile(path):
            fail(f"missing {path}")
        section("libgcc: GCC Runtime Library Exception", read(path))

    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(o))
    print(f"third-party notices: {len(linked)} objects checked, "
          f"{len(extra_notices)} per-file notices, written to {out}")


if __name__ == "__main__":
    main()
