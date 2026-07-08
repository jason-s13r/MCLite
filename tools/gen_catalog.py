#!/usr/bin/env python3
"""Generate catalog.json for the Mesh America Device Configurator (issue #42).

Emits a static "provider catalog" matching the MeshCore config.json schema
(https://apps.meshamerica.com). It points at MCLite's GitHub Release assets,
which are served over HTTPS with `Access-Control-Allow-Origin: *`, and the
catalog itself is meant to be served from raw.githubusercontent.com (also CORS
`*`). No server, no keys — Mesh America just reads this one file live.

Device names follow the official MeshCore catalog (flasher.meshcore.io):
  - "LilyGo T-Deck"        exists officially  -> MCLite FOLDS into that tile as
                           an extra firmware option (the T-Deck Plus is the GPS
                           variant of the same board; GPS features are simply
                           inactive on a base T-Deck).
  - "LilyGo T-Watch Ultra" is NOT in the official catalog -> a NEW device tile,
                           badged as MCLite.

Regenerated on each release by .github/workflows/build-firmware.yml.

Usage:
    tools/gen_catalog.py [version] [notes]
      version : e.g. "0.4.1" or "v0.4.1" (default: MCLITE_VERSION from defaults.h)
      notes   : per-version release notes (default: a link to the release)
"""
import json
import os
import re
import sys

REPO = "laserir/MCLite"
HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULTS_H = os.path.join(HERE, "..", "firmware", "src", "config", "defaults.h")


def read_version_from_defaults():
    with open(DEFAULTS_H) as f:
        m = re.search(r'#define\s+MCLITE_VERSION\s+"([^"]+)"', f.read())
    if not m:
        raise SystemExit("could not find MCLITE_VERSION in defaults.h")
    return m.group(1)


def asset_url(version, filename):
    return f"https://github.com/{REPO}/releases/download/v{version}/{filename}"


def firmware_option(version, notes, merged_bin):
    return {
        "role": "guiSD",              # on-device GUI firmware, settings on an SD card
        "title": "MCLite",
        "version": {
            f"v{version}": {
                "notes": notes,
                "files": [
                    {
                        "type": "flash-wipe",   # merged bootloader + app, written at 0x0
                        "name": merged_bin,
                        "url": asset_url(version, merged_bin),
                        "title": "Full install (bootloader + firmware)",
                    }
                ],
            }
        },
    }


def build_catalog(version, notes):
    return {
        "description": (
            "Lightweight on-device MeshCore communicator for the LilyGo T-Deck "
            "Plus and T-Watch Ultra: encrypted DMs, channels, rooms, SOS, GPS/map, "
            "plus BLE/WiFi/USB companion. Runs standalone; configured from an "
            "SD-card config.json (offline config tool)."
        ),
        "maker": {"mclite": {"name": "MCLite"}},
        "device": [
            {
                "maker": "mclite",
                "name": "LilyGo T-Deck",          # FOLDS into the official T-Deck tile
                "type": "esp32",
                "firmware": [
                    firmware_option(version, notes, f"mclite-v{version}.bin")
                ],
            },
            {
                "maker": "mclite",
                "name": "LilyGo T-Watch Ultra",    # NEW device tile (not in official catalog)
                "type": "esp32",
                "firmware": [
                    firmware_option(version, notes, f"mclite-watch-v{version}.bin")
                ],
            },
        ],
    }


def main():
    version = (sys.argv[1] if len(sys.argv) > 1 else read_version_from_defaults()).lstrip("v")
    notes = sys.argv[2] if len(sys.argv) > 2 else (
        f"Full release notes: https://github.com/{REPO}/releases/tag/v{version}"
    )
    catalog = build_catalog(version, notes)
    out = os.path.join(HERE, "..", "catalog.json")
    with open(out, "w") as f:
        json.dump(catalog, f, indent=2)
        f.write("\n")
    print(f"wrote {os.path.normpath(out)} for v{version}")


if __name__ == "__main__":
    main()
