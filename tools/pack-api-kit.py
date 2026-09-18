#!/usr/bin/env python3
"""Package the SLIF NG integration kit -> Release/SLIFNG-API-<version>+.zip

The kit is what another mod needs to integrate with SLIF NG, and nothing else:
the C++ query header, the three Papyrus scripts a consumer compiles against,
and a README.

Versions in VERSIONS.txt are read straight out of the sources that define them
(the Papyrus API version from Papyrus.cpp, the query interface version from
SLIFNG_API.h, the mod version from SLIFNG_Version.psc), so they cannot drift
from what the mod reports at runtime.

Usage:  python tools/pack-api-kit.py
"""
import os
import re
import shutil
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "dist", "Core", "source", "scripts")
KIT = os.path.join(ROOT, "tools", "api-kit")
RELEASE = os.path.join(ROOT, "Release")

# Consumer-facing scripts. Verified self-contained: a consumer compiles against
# these three plus PapyrusUtil and the vanilla sources, nothing else.
PAPYRUS = ["SLIF_Main.psc", "SLIF_Morph.psc", "SLIFNG.psc"]


def grab(path, pattern, what):
    with open(path, encoding="utf-8-sig", errors="replace") as fh:
        m = re.search(pattern, fh.read(), re.M)
    if not m:
        raise SystemExit("pack-api-kit: could not read %s from %s" % (what, path))
    return m.group(1)


def main():
    version = grab(os.path.join(SRC, "SLIFNG_Version.psc"),
                   r'GetVersionString\(\)\s*Global\s*\n\s*Return\s*"([^"]+)"',
                   "GetVersionString()")
    packed = grab(os.path.join(SRC, "SLIFNG_Version.psc"),
                  r"GetVersion\(\)\s*Global\s*\n\s*Return\s*(\d+)", "GetVersion()")
    papyrus_api = grab(os.path.join(ROOT, "src", "Papyrus.cpp"),
                       r"kApiVersion\s*=\s*(\d+)", "kApiVersion")
    query_api = grab(os.path.join(ROOT, "src", "API", "SLIFNG_API.h"),
                     r"kQueryVersion\s*=\s*(\d+)", "kQueryVersion")

    staging = os.path.join(ROOT, "build", "api-kit")
    shutil.rmtree(staging, ignore_errors=True)
    os.makedirs(os.path.join(staging, "cpp"))
    os.makedirs(os.path.join(staging, "papyrus"))

    shutil.copy2(os.path.join(ROOT, "src", "API", "SLIFNG_API.h"),
                 os.path.join(staging, "cpp"))
    for name in PAPYRUS:
        shutil.copy2(os.path.join(SRC, name), os.path.join(staging, "papyrus"))
    shutil.copy2(os.path.join(KIT, "README.md"), staging)

    with open(os.path.join(staging, "VERSIONS.txt"), "w", newline="\n") as fh:
        fh.write("\n".join([
            "SLIF NG integration kit",
            "",
            "Mod version           : %s  (SLIFNG_Version.GetVersion() == %s)" % (version, packed),
            "Papyrus API version   : %s  (SLIFNG.GetVersion(), 0 when SLIF NG is absent)"
            % papyrus_api,
            "C++ query interface   : %s  (Version(); IQueryInterface<N> up to this)" % query_api,
            "",
            "Gate an optional integration on those, NOT on the mod version.",
            "",
            "The write surface (SLIF_Main.inflate / SLIF_Morph.morph and the",
            "unregister calls) is FROZEN by the compatibility contract, so it needs",
            "no gate at all: it is the same on plain SLIF SE and on SLIF NG.",
            "The read/enumeration surface is SLIF NG only - gate that on",
            "SLIFNG.GetVersion() (Papyrus) or the query interface version (C++).",
            "",
            "Packaged from SLIF NG %s, and good for that version and later: the" % version,
            "pinned surface cannot change and the query interface only grows.",
            "",
        ]))

    os.makedirs(RELEASE, exist_ok=True)
    out = os.path.join(RELEASE, "SLIFNG-API-%s+.zip" % version)
    if os.path.exists(out):
        os.remove(out)
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
        for base, _, files in os.walk(staging):
            for name in sorted(files):
                full = os.path.join(base, name)
                zf.write(full, os.path.relpath(full, staging))

    print("%s  (%.1f KB)" % (os.path.relpath(out, ROOT), os.path.getsize(out) / 1024.0))


if __name__ == "__main__":
    main()
