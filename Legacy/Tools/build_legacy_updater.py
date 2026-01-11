#!/usr/bin/env python3
"""
Build script for Legacy USB Updater (Self-Erasing)

This script:
1. Reads the firmware binary
2. Embeds it (base64 encoded) into the updater script
3. Builds a standalone .exe with PyInstaller

Run this on your development machine ONLY - never distribute this script.

Usage:
    python build_legacy_updater.py
    python build_legacy_updater.py --firmware path/to/firmware.bin

Output:
    dist/LeShuffler_Legacy_Updater.exe  (self-contained, self-deleting)
"""

import os
import sys
import base64
import subprocess
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description="Build Legacy USB Updater")
    parser.add_argument('--firmware', '-f',
                        help='Path to firmware .bin file (default: LeShuffler.bin in Tools folder)')
    args = parser.parse_args()

    script_dir = Path(__file__).parent

    # Paths
    if args.firmware:
        firmware_path = Path(args.firmware)
    else:
        firmware_path = script_dir / "LeShuffler.bin"

    template_path = script_dir / "LeShuffler_Legacy_Updater.py"
    output_script = script_dir / "_legacy_updater_embedded.py"

    print("=" * 60)
    print("Building Legacy USB Updater (Self-Erasing)")
    print("=" * 60)

    # Check firmware exists
    if not firmware_path.exists():
        print(f"ERROR: Firmware not found: {firmware_path}")
        print("Copy LeShuffler.bin to the Tools folder or use --firmware option.")
        return 1

    # Check template exists
    if not template_path.exists():
        print(f"ERROR: Template not found: {template_path}")
        return 1

    # Read and encode firmware
    print(f"\n[1/4] Reading firmware: {firmware_path}")
    with open(firmware_path, 'rb') as f:
        firmware_data = f.read()

    firmware_b64 = base64.b64encode(firmware_data).decode('ascii')
    print(f"       Size: {len(firmware_data):,} bytes")
    print(f"       Base64: {len(firmware_b64):,} chars")

    # Read template
    print(f"\n[2/4] Reading template: {template_path}")
    with open(template_path, 'r') as f:
        template_content = f.read()

    # Embed firmware
    print("\n[3/4] Embedding firmware...")
    embedded_content = template_content.replace(
        'REPLACE_WITH_BASE64_ENCODED_FIRMWARE',
        firmware_b64
    )

    # Verify replacement worked
    if 'REPLACE_WITH' in embedded_content:
        print("ERROR: Failed to embed firmware!")
        return 1

    # Write embedded script
    with open(output_script, 'w') as f:
        f.write(embedded_content)

    print(f"       Written: {output_script}")

    # Build with PyInstaller
    print("\n[4/4] Building executable with PyInstaller...")

    pyinstaller_cmd = [
        sys.executable, "-m", "PyInstaller",
        "--onefile",
        "--name", "LeShuffler_Legacy_Updater",
        "--collect-all", "serial",  # Include pyserial properly
        "--clean",
        str(output_script)
    ]

    try:
        result = subprocess.run(pyinstaller_cmd, cwd=script_dir)
        if result.returncode != 0:
            print("ERROR: PyInstaller failed!")
            return 1
    except FileNotFoundError:
        print("ERROR: PyInstaller not found!")
        print("Install it with: pip install pyinstaller")
        return 1
    except Exception as e:
        print(f"ERROR: {e}")
        return 1

    # Cleanup temp file
    output_script.unlink()
    print(f"\n[OK] Cleaned up: {output_script}")

    # Final output
    exe_path = script_dir / "dist" / "LeShuffler_Legacy_Updater.exe"
    if exe_path.exists():
        size_mb = exe_path.stat().st_size / (1024 * 1024)
        print("\n" + "=" * 60)
        print("BUILD SUCCESSFUL")
        print("=" * 60)
        print(f"\nOutput: {exe_path}")
        print(f"Size: {size_mb:.1f} MB")
        print("\nThis exe contains:")
        print("  - Embedded firmware (base64 encoded)")
        print("  - Self-deletion on successful update")
        print("\nFor end users with legacy bootloader:")
        print("  1. Put device in bootloader mode")
        print("     (Settings > Maintenance > Firmware Update)")
        print("  2. Wait for 3 beeps + 1 long beep")
        print("  3. Run this exe")
        print("  4. After success, exe deletes itself")
    else:
        print("\nWARNING: Expected output not found!")
        # Check for .exe without extension (cross-platform)
        exe_path_noext = script_dir / "dist" / "LeShuffler_Legacy_Updater"
        if exe_path_noext.exists():
            print(f"Found: {exe_path_noext}")
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
