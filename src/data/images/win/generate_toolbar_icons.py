#!/usr/bin/env python3
"""
Generate Windows toolbar PNG icons from the shared marinaMoji SVG source
files in src/unix/ibus/toolbar_icons/.

The Windows renderer has no SVG (or, before this script, PNG) decoding
capability at all, so icons are pre-rendered here at build time and shipped
as loose PNG files (see ToolbarWindow / win32_image_util.h's
LoadPngFileToHBitmap), the same way mac and Linux load their icon files from
a directory at runtime -- just PNG instead of SVG.

Usage:
    python3 generate_toolbar_icons.py

Requirements:
    pip install resvg-py pillow
"""

import re
import sys
from pathlib import Path

import resvg_py
from PIL import Image

SCRIPT_DIR = Path(__file__).resolve().parent
SRC_ICON_DIR = SCRIPT_DIR.parent.parent.parent / "unix" / "ibus" / "toolbar_icons"
OUT_DIR = SCRIPT_DIR / "toolbar_icons"

# Rendered at each of these heights (px), matching the renderer's DPI scale
# steps (100% / 150% / 200%).
SIZES = (24, 36, 48)

# Square icons: mode buttons, shin/kyu, symbols, dict, settings, shortcuts.
SQUARE_ICON_NAMES = [
    "toolbar_roman_light", "toolbar_roman_dark",
    "toolbar_roman_light_lock", "toolbar_roman_dark_lock",
    "toolbar_hira_light", "toolbar_hira_dark",
    "toolbar_hira_light_lock", "toolbar_hira_dark_lock",
    "toolbar_kata_light", "toolbar_kata_dark",
    "toolbar_kata_light_lock", "toolbar_kata_dark_lock",
    "toolbar_kata_half_light", "toolbar_kata_half_dark",
    "toolbar_roma_full_light", "toolbar_roma_full_dark",
    "toolbar_roma_half_light", "toolbar_roma_half_dark",
    "toolbar_shin_light", "toolbar_shin_dark",
    "toolbar_kyu_light", "toolbar_kyu_dark",
    "toolbar_symbols_light", "toolbar_symbols_dark",
    "toolbar_dict_light", "toolbar_dict_dark",
    "toolbar_settings_light", "toolbar_settings_dark",
    "toolbar_shortcuts_light", "toolbar_shortcuts_dark",
]

# Wide logo: aspect ratio preserved, height matches icon size.
LOGO_ICON_NAMES = ["logo_long_light", "logo_long_dark"]

_MM_ATTR_RE = re.compile(r'(width|height)="([0-9.]+)mm"')


def _strip_mm_units(svg_text: str) -> str:
    """resvg's usvg parser rejects this repo's `width="30mm"`-style physical
    units (paired with a matching viewBox) as "SVG has an invalid size".
    Since the viewBox already defines the real (unitless) coordinate space,
    it's safe to just drop the `mm` suffix here at render time."""
    return _MM_ATTR_RE.sub(r'\1="\2"', svg_text)


def _svg_natural_size(svg_text: str) -> tuple[float, float]:
    match_w = re.search(r'width="([0-9.]+)"', svg_text)
    match_h = re.search(r'height="([0-9.]+)"', svg_text)
    if not match_w or not match_h:
        raise ValueError("could not find width/height in SVG")
    return float(match_w.group(1)), float(match_h.group(1))


def render_svg(svg_path: Path, width: int, height: int) -> bytes:
    svg_text = _strip_mm_units(svg_path.read_text(encoding="utf-8"))
    return bytes(
        resvg_py.svg_to_bytes(svg_string=svg_text, width=width, height=height)
    )


def generate_square_icons() -> None:
    print("Generating square toolbar icons...")
    for name in SQUARE_ICON_NAMES:
        svg_path = SRC_ICON_DIR / f"{name}.svg"
        if not svg_path.exists():
            print(f"  WARNING: {svg_path.name} not found, skipping")
            continue
        for size in SIZES:
            png_bytes = render_svg(svg_path, size, size)
            out_path = OUT_DIR / f"{name}_{size}.png"
            out_path.write_bytes(png_bytes)
        print(f"  {svg_path.name} -> {name}_{{{','.join(map(str, SIZES))}}}.png")


def generate_logo_icons() -> None:
    print("Generating logo icons...")
    for name in LOGO_ICON_NAMES:
        svg_path = SRC_ICON_DIR / f"{name}.svg"
        if not svg_path.exists():
            print(f"  WARNING: {svg_path.name} not found, skipping")
            continue
        natural_w, natural_h = _svg_natural_size(
            _strip_mm_units(svg_path.read_text(encoding="utf-8"))
        )
        aspect = natural_w / natural_h
        for size in SIZES:
            width = round(size * aspect)
            png_bytes = render_svg(svg_path, width, size)
            out_path = OUT_DIR / f"{name}_{size}.png"
            out_path.write_bytes(png_bytes)
        print(f"  {svg_path.name} -> {name}_{{{','.join(map(str, SIZES))}}}.png")


def verify_pngs() -> None:
    print("Verifying generated PNGs...")
    count = 0
    for png_path in sorted(OUT_DIR.glob("*.png")):
        with Image.open(png_path) as im:
            im.load()
            if im.mode != "RGBA":
                sys.exit(f"ERROR: {png_path.name} is not RGBA (got {im.mode})")
        count += 1
    print(f"  {count} PNGs OK")


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    generate_square_icons()
    generate_logo_icons()
    verify_pngs()
    print("\nDone.")


if __name__ == "__main__":
    main()
