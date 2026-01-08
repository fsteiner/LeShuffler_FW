#!/usr/bin/env python3
"""
LeShuffler ST-LINK Standalone Flasher

Flashes bootloader and/or firmware using stlink tools (st-flash.exe).
No STM32CubeProgrammer installation required.

For end users who need to recover/reflash their device via ST-LINK.

Requirements:
- ST-LINK V2 or V3 adapter
- st-flash.exe in same folder (or in PATH)

Download stlink tools from:
https://github.com/stlink-org/stlink/releases

Usage:
    python stlink_standalone_flasher.py              # Flash both bootloader + firmware
    python stlink_standalone_flasher.py --firmware   # Firmware only
    python stlink_standalone_flasher.py --bootloader # Bootloader only
    python stlink_standalone_flasher.py --erase      # Mass erase only
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path

# Flash addresses
BOOTLOADER_ADDR = 0x08000000
FIRMWARE_ADDR = 0x08020000

# Default filenames (in same folder as script/exe)
BOOTLOADER_FILE = "LeShuffler_Bootloader_E.bin"
FIRMWARE_FILE = "LeShuffler.bin"


def get_app_dir():
    """Get directory where script/exe is located"""
    if getattr(sys, 'frozen', False):
        return Path(sys.executable).parent
    return Path(__file__).parent


def find_st_flash():
    """Find st-flash executable"""
    app_dir = get_app_dir()

    # Check in app directory first
    local_path = app_dir / "st-flash.exe"
    if local_path.exists():
        return str(local_path)

    # Check without .exe extension (for cross-platform)
    local_path_noext = app_dir / "st-flash"
    if local_path_noext.exists():
        return str(local_path_noext)

    # Check in PATH
    try:
        result = subprocess.run(["where", "st-flash"], capture_output=True, text=True)
        if result.returncode == 0:
            return result.stdout.strip().split('\n')[0]
    except:
        pass

    # Try running directly (might be in PATH on Unix)
    try:
        result = subprocess.run(["st-flash", "--version"], capture_output=True, text=True)
        if result.returncode == 0:
            return "st-flash"
    except:
        pass

    return None


def run_st_flash(args, st_flash_path):
    """Run st-flash with given arguments"""
    cmd = [st_flash_path] + args
    print(f"Running: {' '.join(cmd)}")

    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        print(result.stdout)
        if result.stderr:
            print(result.stderr)
        return result.returncode == 0
    except Exception as e:
        print(f"Error running st-flash: {e}")
        return False


def probe_device(st_flash_path):
    """Check if ST-LINK and device are connected"""
    print("Probing for ST-LINK and device...")
    cmd = [st_flash_path, "--probe"]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
        output = result.stdout + result.stderr
        print(output)

        if "STM32H7" in output or "0x450" in output:  # 0x450 is STM32H7 chip ID
            print("STM32H7 device detected!")
            return True
        elif "Found" in output and "device" in output.lower():
            print("Device detected (may not be STM32H7)")
            return True
        else:
            print("No device detected. Check ST-LINK connection.")
            return False
    except subprocess.TimeoutExpired:
        print("Timeout waiting for device. Check ST-LINK connection.")
        return False
    except Exception as e:
        print(f"Error probing device: {e}")
        return False


def mass_erase(st_flash_path):
    """Perform mass erase"""
    print("\n" + "="*50)
    print("MASS ERASE")
    print("="*50)
    return run_st_flash(["erase"], st_flash_path)


def flash_file(st_flash_path, filepath, address):
    """Flash a binary file to specified address"""
    print(f"\nFlashing {filepath} to 0x{address:08X}...")

    if not os.path.exists(filepath):
        print(f"ERROR: File not found: {filepath}")
        return False

    file_size = os.path.getsize(filepath)
    print(f"File size: {file_size:,} bytes")

    # st-flash write <file> <address>
    success = run_st_flash(["write", filepath, f"0x{address:08X}"], st_flash_path)

    if success:
        print(f"Successfully flashed {filepath}")
    else:
        print(f"FAILED to flash {filepath}")

    return success


def reset_device(st_flash_path):
    """Reset the device"""
    print("\nResetting device...")
    return run_st_flash(["reset"], st_flash_path)


def main():
    parser = argparse.ArgumentParser(
        description="LeShuffler ST-LINK Standalone Flasher",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                    Flash bootloader + firmware
  %(prog)s --firmware         Flash firmware only
  %(prog)s --bootloader       Flash bootloader only
  %(prog)s --erase            Mass erase only

Required files (in same folder):
  - st-flash.exe              ST-LINK tool
  - LeShuffler_Bootloader_E.bin   Bootloader binary
  - LeShuffler.bin            Firmware binary
        """
    )

    parser.add_argument('--firmware', '-f', action='store_true',
                        help='Flash firmware only')
    parser.add_argument('--bootloader', '-b', action='store_true',
                        help='Flash bootloader only')
    parser.add_argument('--erase', '-e', action='store_true',
                        help='Mass erase only (no flashing)')
    parser.add_argument('--no-reset', action='store_true',
                        help='Do not reset device after flashing')
    parser.add_argument('--bootloader-file', default=BOOTLOADER_FILE,
                        help=f'Bootloader file (default: {BOOTLOADER_FILE})')
    parser.add_argument('--firmware-file', default=FIRMWARE_FILE,
                        help=f'Firmware file (default: {FIRMWARE_FILE})')

    args = parser.parse_args()

    print("="*50)
    print("LeShuffler ST-LINK Standalone Flasher")
    print("="*50)

    # Find st-flash
    st_flash = find_st_flash()
    if not st_flash:
        print("\nERROR: st-flash not found!")
        print("\nPlease download stlink tools from:")
        print("https://github.com/stlink-org/stlink/releases")
        print("\nPlace st-flash.exe in the same folder as this program.")
        input("\nPress Enter to exit...")
        return 1

    print(f"Using: {st_flash}")

    # Get file paths
    app_dir = get_app_dir()
    bootloader_path = app_dir / args.bootloader_file
    firmware_path = app_dir / args.firmware_file

    # Determine what to flash
    flash_bootloader = args.bootloader or (not args.firmware and not args.erase)
    flash_firmware = args.firmware or (not args.bootloader and not args.erase)

    # Check files exist
    if flash_bootloader and not bootloader_path.exists():
        print(f"\nERROR: Bootloader not found: {bootloader_path}")
        input("\nPress Enter to exit...")
        return 1

    if flash_firmware and not firmware_path.exists():
        print(f"\nERROR: Firmware not found: {firmware_path}")
        input("\nPress Enter to exit...")
        return 1

    # Probe device
    if not probe_device(st_flash):
        print("\nMake sure:")
        print("  1. ST-LINK is connected to USB")
        print("  2. ST-LINK is connected to the device (SWD pins)")
        print("  3. Device is powered")
        input("\nPress Enter to exit...")
        return 1

    # Confirm action
    print("\n" + "-"*50)
    print("Actions to perform:")
    if args.erase:
        print("  - Mass erase flash")
    if flash_bootloader:
        print(f"  - Flash bootloader: {args.bootloader_file}")
    if flash_firmware:
        print(f"  - Flash firmware: {args.firmware_file}")
    if not args.no_reset:
        print("  - Reset device")
    print("-"*50)

    confirm = input("\nProceed? (y/N): ").strip().lower()
    if confirm != 'y':
        print("Cancelled.")
        input("\nPress Enter to exit...")
        return 0

    success = True

    # Mass erase if requested or flashing bootloader
    if args.erase or flash_bootloader:
        if not mass_erase(st_flash):
            print("\nERROR: Mass erase failed!")
            success = False

    # Flash bootloader
    if success and flash_bootloader:
        if not flash_file(st_flash, str(bootloader_path), BOOTLOADER_ADDR):
            success = False

    # Flash firmware
    if success and flash_firmware:
        if not flash_file(st_flash, str(firmware_path), FIRMWARE_ADDR):
            success = False

    # Reset device
    if success and not args.no_reset:
        reset_device(st_flash)

    print("\n" + "="*50)
    if success:
        print("COMPLETE - All operations successful!")
    else:
        print("FAILED - Some operations did not complete")
    print("="*50)

    input("\nPress Enter to exit...")
    return 0 if success else 1


if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nCancelled by user")
        sys.exit(1)
    except Exception as e:
        print(f"\nUnexpected error: {e}")
        input("\nPress Enter to exit...")
        sys.exit(1)
