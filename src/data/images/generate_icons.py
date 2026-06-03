#!/usr/bin/env python3
"""
Generate macOS TIFF icons, iconset PNGs, ICNS, and product_icon PNG
from SVG source files.

Usage:
    python3 generate_icons.py

Requirements:
    - rsvg-convert  (Arch: sudo pacman -S librsvg)
    - Pillow        (pip install pillow)
"""

import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

SCRIPT_DIR = Path(__file__).resolve().parent
MAC_DIR = SCRIPT_DIR / "mac"
ICONSET_DIR = MAC_DIR / "product_icon.iconset"
LOGO_SQUARE_SVG = (
    SCRIPT_DIR.parent.parent
    / "unix"
    / "ibus"
    / "toolbar_icons"
    / "logo_square_light.svg"
)
# Candidate footer logo height at default 14 pt text (square, matches legacy bar height).
CANDIDATE_WINDOW_LOGO_SIZE = 19

# SVG basenames that map to mac/*.tiff (rendered at 32×32 RGBA).
# Keep this aligned with the historical Google Mozc assets, which are 32×32
# TIFFs at 144 dpi.
MODE_ICONS = [
    "direct",
    "full_ascii",
    "full_katakana",
    "half_ascii",
    "half_katakana",
    "hiragana",
]

# (filename, pixel size) for every PNG in the iconset
ICONSET_SIZES = [
    ("icon_16x16.png",        16),
    ("icon_16x16at2x.png",    32),
    ("icon_32x32.png",        32),
    ("icon_32x32at2x.png",    64),
    ("icon_128x128.png",     128),
    ("icon_128x128at2x.png", 256),
    ("icon_256x256.png",     256),
    ("icon_256x256at2x.png", 512),
    ("icon_512x512.png",     512),
    ("icon_512x512at2x.png", 1024),
]

# Maps iconset filenames → ICNS OSType codes used inside the .icns container.
# Each PNG is stored verbatim in the ICNS under its type code.
ICONSET_TO_ICNS_TYPE = {
    "icon_16x16.png":        b"icp4",
    "icon_16x16at2x.png":    b"ic11",
    "icon_32x32.png":        b"icp5",
    "icon_32x32at2x.png":    b"ic12",
    "icon_128x128.png":      b"ic07",
    "icon_128x128at2x.png":  b"ic13",
    "icon_256x256.png":      b"ic08",
    "icon_256x256at2x.png":  b"ic14",
    "icon_512x512.png":      b"ic09",
    "icon_512x512at2x.png":  b"ic10",
}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def svg_to_png(svg_path: Path, png_path: Path, size: int) -> None:
    """Render an SVG to a square PNG at *size*×*size* using rsvg-convert."""
    subprocess.run(
        [
            "rsvg-convert",
            "--width", str(size),
            "--height", str(size),
            "--output", str(png_path),
            str(svg_path),
        ],
        check=True,
    )


def png_to_tiff(png_path: Path, tiff_path: Path) -> None:
    """Convert a PNG to an uncompressed RGBA TIFF."""
    img = Image.open(png_path).convert("RGBA")
    # Match legacy macOS mode icons:
    # - uncompressed RGBA TIFF
    # - 144 dpi
    img.save(tiff_path, format="TIFF", compression="raw", dpi=(144, 144))


def build_icns(iconset_dir: Path, icns_path: Path) -> None:
    """
    Assemble an ICNS file from the PNGs in *iconset_dir*.

    The ICNS format is straightforward:
      - 4-byte magic  b'icns'
      - 4-byte total file length (big-endian uint32)
      - repeated entries, each:
          4-byte OSType  +  4-byte entry length  +  raw PNG bytes
    """
    entries = []
    for filename, type_code in ICONSET_TO_ICNS_TYPE.items():
        png_path = iconset_dir / filename
        if not png_path.exists():
            print(f"  WARNING: {filename} missing, skipping in ICNS")
            continue
        png_data = png_path.read_bytes()
        entry_length = len(png_data) + 8          # 4 type + 4 length + data
        entries.append(type_code + struct.pack(">I", entry_length) + png_data)

    body = b"".join(entries)
    header = b"icns" + struct.pack(">I", len(body) + 8)
    icns_path.write_bytes(header + body)


# ---------------------------------------------------------------------------
# Generation steps
# ---------------------------------------------------------------------------

def generate_candidate_window_logo_tiff() -> None:
    """logo_square_light.svg → candidate_window_logo.tiff (footer branding)."""
    print("Generating candidate_window_logo.tiff …")
    tiff_path = MAC_DIR / "candidate_window_logo.tiff"
    if not LOGO_SQUARE_SVG.exists():
        print(f"  WARNING: {LOGO_SQUARE_SVG.name} not found, skipping")
        return
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmp:
        tmp_png = Path(tmp.name)
    try:
        svg_to_png(LOGO_SQUARE_SVG, tmp_png, CANDIDATE_WINDOW_LOGO_SIZE)
        png_to_tiff(tmp_png, tiff_path)
        print(
            f"  {LOGO_SQUARE_SVG.name} → {tiff_path.relative_to(SCRIPT_DIR)}"
            f"  ({CANDIDATE_WINDOW_LOGO_SIZE}×{CANDIDATE_WINDOW_LOGO_SIZE})"
        )
    finally:
        tmp_png.unlink(missing_ok=True)


def generate_marinamoji_mode_tiff() -> None:
    """icon.svg → marinamoji.tiff (32×32 menu icon for the visible input source)."""
    print("Generating marinamoji.tiff …")
    icon_svg = SCRIPT_DIR / "icon.svg"
    tiff_path = MAC_DIR / "marinamoji.tiff"
    if not icon_svg.exists():
        print(f"  WARNING: {icon_svg.name} not found, skipping")
        return
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmp:
        tmp_png = Path(tmp.name)
    try:
        svg_to_png(icon_svg, tmp_png, 32)
        png_to_tiff(tmp_png, tiff_path)
        print(f"  icon.svg → {tiff_path.relative_to(SCRIPT_DIR)}")
    finally:
        tmp_png.unlink(missing_ok=True)


def generate_mode_tiffs() -> None:
    """SVG → 32×32 RGBA TIFF for each input-mode icon."""
    print("Generating input-mode TIFFs …")
    for name in MODE_ICONS:
        svg_path = SCRIPT_DIR / f"{name}.svg"
        tiff_path = MAC_DIR / f"{name}.tiff"
        if not svg_path.exists():
            print(f"  WARNING: {svg_path.name} not found, skipping")
            continue
        with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmp:
            tmp_png = Path(tmp.name)
        try:
            svg_to_png(svg_path, tmp_png, 32)
            png_to_tiff(tmp_png, tiff_path)
            print(f"  {svg_path.name} → {tiff_path.relative_to(SCRIPT_DIR)}")
        finally:
            tmp_png.unlink(missing_ok=True)


def generate_iconset() -> None:
    """icon.svg → every PNG in product_icon.iconset/."""
    print("Generating iconset PNGs …")
    icon_svg = SCRIPT_DIR / "icon.svg"
    if not icon_svg.exists():
        sys.exit(f"ERROR: {icon_svg} not found")

    ICONSET_DIR.mkdir(parents=True, exist_ok=True)
    for filename, size in ICONSET_SIZES:
        out_path = ICONSET_DIR / filename
        svg_to_png(icon_svg, out_path, size)
        print(f"  icon.svg → {out_path.relative_to(SCRIPT_DIR)}  ({size}×{size})")


def generate_icns() -> None:
    """Assemble product_icon.icns from the iconset PNGs."""
    print("Generating ICNS …")
    icns_path = MAC_DIR / "product_icon.icns"

    if sys.platform == "darwin":
        # On macOS, iconutil produces the most "official" ICNS.
        # It expects @2x in filenames, so temporarily symlink if needed.
        subprocess.run(
            ["iconutil", "--convert", "icns",
             "--output", str(icns_path), str(ICONSET_DIR)],
            check=True,
        )
    else:
        build_icns(ICONSET_DIR, icns_path)

    print(f"  → {icns_path.relative_to(SCRIPT_DIR)}")


def generate_product_icon_png() -> None:
    """icon.svg → product_icon_32bpp-128.png (128×128 RGBA)."""
    print("Generating product_icon_32bpp-128.png …")
    icon_svg = SCRIPT_DIR / "icon.svg"
    out_path = SCRIPT_DIR / "product_icon_32bpp-128.png"
    svg_to_png(icon_svg, out_path, 128)
    print(f"  icon.svg → {out_path.name}  (128×128)")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    try:
        subprocess.run(
            ["rsvg-convert", "--version"], capture_output=True, check=True,
        )
    except FileNotFoundError:
        sys.exit(
            "ERROR: rsvg-convert not found.  Install librsvg:\n"
            "  Arch:   sudo pacman -S librsvg\n"
            "  Debian: sudo apt install librsvg2-bin\n"
            "  macOS:  brew install librsvg"
        )

    generate_mode_tiffs()
    generate_candidate_window_logo_tiff()
    generate_marinamoji_mode_tiff()
    generate_iconset()
    generate_icns()
    generate_product_icon_png()
    print("\nDone ✓")


if __name__ == "__main__":
    main()
