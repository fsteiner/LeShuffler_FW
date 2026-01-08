#!/usr/bin/env python3
"""
Build script for Remote Recovery Flasher

This script:
1. Reads the bootloader binary
2. Embeds it (base64 encoded) into the flasher script
3. Builds a standalone .exe with PyInstaller

Run this on your development machine ONLY - never distribute this script.

Usage:
    python build_remote_flasher.py

Output:
    dist/LeShuffler_Remote_Recovery.exe  (self-contained, self-deleting)
"""

import os
import sys
import base64
import subprocess
import shutil
from pathlib import Path


def main():
    script_dir = Path(__file__).parent

    # Paths
    bootloader_path = script_dir / "LeShuffler_Bootloader_E.bin"
    template_path = script_dir / "remote_recovery_flasher.py"
    output_script = script_dir / "_remote_flasher_embedded.py"

    print("="*60)
    print("Building Remote Recovery Flasher")
    print("="*60)

    # Check bootloader exists
    if not bootloader_path.exists():
        print(f"ERROR: Bootloader not found: {bootloader_path}")
        print("Copy LeShuffler_Bootloader_E.bin to the Tools folder first.")
        return 1

    # Read and encode bootloader
    print(f"\n[1/4] Reading bootloader: {bootloader_path}")
    with open(bootloader_path, 'rb') as f:
        bootloader_data = f.read()

    bootloader_b64 = base64.b64encode(bootloader_data).decode('ascii')
    print(f"       Size: {len(bootloader_data):,} bytes")
    print(f"       Base64: {len(bootloader_b64):,} chars")

    # Read template
    print(f"\n[2/4] Reading template: {template_path}")
    with open(template_path, 'r') as f:
        template_content = f.read()

    # Embed bootloader
    print("\n[3/4] Embedding bootloader...")
    embedded_content = template_content.replace(
        'REPLACE_WITH_BASE64_ENCODED_BOOTLOADER',
        bootloader_b64
    )

    # Verify replacement worked
    if 'REPLACE_WITH' in embedded_content:
        print("ERROR: Failed to embed bootloader!")
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
        "--name", "LeShuffler_Remote_Recovery",
        "--clean",
        str(output_script)
    ]

    try:
        result = subprocess.run(pyinstaller_cmd, cwd=script_dir)
        if result.returncode != 0:
            print("ERROR: PyInstaller failed!")
            return 1
    except Exception as e:
        print(f"ERROR: {e}")
        return 1

    # Cleanup temp file
    output_script.unlink()
    print(f"\n[OK] Cleaned up: {output_script}")

    # Final output
    exe_path = script_dir / "dist" / "LeShuffler_Remote_Recovery.exe"
    if exe_path.exists():
        size_mb = exe_path.stat().st_size / (1024 * 1024)
        print("\n" + "="*60)
        print("BUILD SUCCESSFUL")
        print("="*60)
        print(f"\nOutput: {exe_path}")
        print(f"Size: {size_mb:.1f} MB")
        print("\nThis exe contains:")
        print("  - Embedded bootloader (encrypted in base64)")
        print("  - Self-deletion on exit")
        print("\nFor remote recovery sessions:")
        print("  1. Transfer this exe to client via remote desktop")
        print("  2. Run it (you control via remote session)")
        print("  3. It flashes, sets RDP1, then deletes itself")
    else:
        print("\nWARNING: Expected output not found!")
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
