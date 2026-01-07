#!/usr/bin/env python3
"""
LeShuffler ST-LINK Flasher
Flash bootloader and firmware via ST-LINK without opening STM32CubeProgrammer GUI.

Requires: STM32CubeProgrammer installed (uses STM32_Programmer_CLI.exe)

Memory Map:
  - Bootloader: 0x08000000 (up to 128KB)
  - Application: 0x08020000 (up to ~900KB)

Usage:
  1. Connect ST-LINK to device
  2. Place Bootloader_E.bin and LeShuffler.bin in same folder as this script
  3. Run: python stlink_flasher.py

  Or with custom paths:
    python stlink_flasher.py --bootloader path/to/bootloader.bin --firmware path/to/firmware.bin
"""

import subprocess
import sys
import os
import argparse
from pathlib import Path

def get_app_dir():
    """Get the application directory (works for both script and PyInstaller exe)"""
    if getattr(sys, 'frozen', False):
        # Running as compiled exe
        return Path(sys.executable).parent.resolve()
    else:
        # Running as script
        return Path(__file__).parent.resolve()

# Memory addresses
BOOTLOADER_ADDRESS = 0x08000000
APPLICATION_ADDRESS = 0x08020000

# Common STM32CubeProgrammer installation paths
PROGRAMMER_PATHS = [
    r"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
    r"C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
    r"C:\ST\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
]

def find_programmer():
    """Find STM32_Programmer_CLI.exe from STM32CubeProgrammer installation"""
    # Check common installation paths
    for path in PROGRAMMER_PATHS:
        if os.path.exists(path):
            return path

    # Check PATH environment
    try:
        result = subprocess.run(
            ["where", "STM32_Programmer_CLI.exe"],
            capture_output=True, text=True, timeout=5
        )
        if result.returncode == 0:
            return result.stdout.strip().split('\n')[0]
    except:
        pass

    return None

def run_programmer(cli_path, args, description):
    """Run STM32_Programmer_CLI with given arguments"""
    cmd = [cli_path] + args
    print(f"\n{'='*60}")
    print(f"  {description}")
    print(f"{'='*60}")
    print(f"Command: {' '.join(cmd)}\n")

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        print(result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        return result.returncode == 0
    except subprocess.TimeoutExpired:
        print("ERROR: Command timed out")
        return False
    except Exception as e:
        print(f"ERROR: {e}")
        return False

def check_connection(cli_path):
    """Check ST-LINK connection"""
    print("\nChecking ST-LINK connection...")
    return run_programmer(cli_path, ["-c", "port=SWD", "-q"], "Connecting to target via ST-LINK")

def erase_chip(cli_path):
    """Full chip erase"""
    return run_programmer(cli_path, ["-c", "port=SWD", "-e", "all", "-q"], "Erasing entire flash")

def flash_file(cli_path, file_path, address, description):
    """Flash a binary file to specified address"""
    args = [
        "-c", "port=SWD",
        "-w", str(file_path), hex(address),
        "-v",  # Verify after write
        "-q"   # Quiet mode (less verbose)
    ]
    return run_programmer(cli_path, args, description)

def reset_device(cli_path):
    """Reset the device"""
    return run_programmer(cli_path, ["-c", "port=SWD", "-hardRst", "-q"], "Resetting device")

def set_rdp_level(cli_path, level):
    """Set Read Protection level (0, 1, or 2)"""
    if level == 0:
        return run_programmer(cli_path, ["-c", "port=SWD", "-ob", "RDP=0xAA", "-q"], "Setting RDP Level 0 (unprotected)")
    elif level == 1:
        return run_programmer(cli_path, ["-c", "port=SWD", "-ob", "RDP=0xBB", "-q"], "Setting RDP Level 1 (read protected)")
    elif level == 2:
        print("\n" + "!"*60)
        print("  WARNING: RDP Level 2 is PERMANENT and IRREVERSIBLE!")
        print("  The device can NEVER be debugged or read again!")
        print("!"*60)
        confirm = input("\nType 'PERMANENT' to proceed: ")
        if confirm != "PERMANENT":
            print("Aborted.")
            return False
        return run_programmer(cli_path, ["-c", "port=SWD", "-ob", "RDP=0xCC", "-q"], "Setting RDP Level 2 (PERMANENT)")
    else:
        print(f"Invalid RDP level: {level}")
        return False

def main():
    parser = argparse.ArgumentParser(
        description="LeShuffler ST-LINK Flasher - Flash bootloader and firmware via ST-LINK",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  Flash both bootloader and firmware:
    python stlink_flasher.py

  Flash only firmware (bootloader already installed):
    python stlink_flasher.py --firmware-only

  Flash and set RDP Level 1 (production):
    python stlink_flasher.py --rdp 1

  Custom file paths:
    python stlink_flasher.py --bootloader my_bootloader.bin --firmware my_app.bin
"""
    )
    parser.add_argument("--bootloader", type=str, help="Path to bootloader binary (default: Bootloader_E.bin)")
    parser.add_argument("--firmware", type=str, help="Path to firmware binary (default: LeShuffler.bin)")
    parser.add_argument("--firmware-only", action="store_true", help="Flash only firmware, skip bootloader")
    parser.add_argument("--bootloader-only", action="store_true", help="Flash only bootloader, skip firmware")
    parser.add_argument("--rdp", type=int, choices=[0, 1, 2], help="Set Read Protection level after flashing")
    parser.add_argument("--no-erase", action="store_true", help="Skip full chip erase (use for partial updates)")
    parser.add_argument("--no-reset", action="store_true", help="Don't reset device after flashing")
    parser.add_argument("--programmer", type=str, help="Path to STM32_Programmer_CLI.exe")

    args = parser.parse_args()

    print("="*60)
    print("  LeShuffler ST-LINK Flasher")
    print("="*60)

    # Find programmer
    cli_path = args.programmer or find_programmer()
    if not cli_path:
        print("\nERROR: STM32_Programmer_CLI.exe not found!")
        print("\nPlease install STM32CubeProgrammer from:")
        print("  https://www.st.com/en/development-tools/stm32cubeprog.html")
        print("\nOr specify the path with --programmer")
        return 1

    print(f"\nUsing programmer: {cli_path}")

    # Get application directory for default file paths
    script_dir = get_app_dir()

    # Determine files to flash
    bootloader_path = None
    firmware_path = None

    if not args.firmware_only:
        bootloader_path = Path(args.bootloader) if args.bootloader else None
        if not bootloader_path:
            # Look for bootloader in common locations
            candidates = [
                script_dir / "Bootloader_E.bin",
                script_dir / "bootloader.bin",
                script_dir / ".." / "Bootloader_E" / "Debug" / "Bootloader_E.bin",
            ]
            for candidate in candidates:
                if candidate.exists():
                    bootloader_path = candidate
                    break

        if bootloader_path and not bootloader_path.exists():
            print(f"\nERROR: Bootloader file not found: {bootloader_path}")
            return 1
        elif not bootloader_path and not args.bootloader_only:
            print("\nWARNING: No bootloader file found. Use --firmware-only to skip bootloader.")
            print("         Or specify path with --bootloader")

    if not args.bootloader_only:
        firmware_path = Path(args.firmware) if args.firmware else script_dir / "LeShuffler.bin"
        if not firmware_path.exists():
            print(f"\nERROR: Firmware file not found: {firmware_path}")
            return 1

    # Show what will be flashed
    print("\nFiles to flash:")
    if bootloader_path:
        size_kb = bootloader_path.stat().st_size / 1024
        print(f"  Bootloader: {bootloader_path} ({size_kb:.1f} KB) -> {hex(BOOTLOADER_ADDRESS)}")
    if firmware_path:
        size_kb = firmware_path.stat().st_size / 1024
        print(f"  Firmware:   {firmware_path} ({size_kb:.1f} KB) -> {hex(APPLICATION_ADDRESS)}")

    if args.rdp is not None:
        print(f"\nRDP Level: {args.rdp} will be set after flashing")
        if args.rdp == 2:
            print("  WARNING: Level 2 is PERMANENT!")

    # Confirm
    print("\n" + "-"*60)
    input("Press Enter to start flashing (Ctrl+C to abort)...")

    # Check connection
    if not check_connection(cli_path):
        print("\nERROR: Could not connect to target!")
        print("Make sure:")
        print("  1. ST-LINK is connected to PC via USB")
        print("  2. ST-LINK is connected to target board (SWD pins)")
        print("  3. Target board is powered")
        return 1

    # Erase chip
    if not args.no_erase:
        if not erase_chip(cli_path):
            print("\nERROR: Chip erase failed!")
            return 1

    # Flash bootloader
    if bootloader_path:
        if not flash_file(cli_path, bootloader_path, BOOTLOADER_ADDRESS,
                         f"Flashing bootloader to {hex(BOOTLOADER_ADDRESS)}"):
            print("\nERROR: Bootloader flash failed!")
            return 1
        print("Bootloader flashed successfully!")

    # Flash firmware
    if firmware_path:
        if not flash_file(cli_path, firmware_path, APPLICATION_ADDRESS,
                         f"Flashing firmware to {hex(APPLICATION_ADDRESS)}"):
            print("\nERROR: Firmware flash failed!")
            return 1
        print("Firmware flashed successfully!")

    # Set RDP if requested
    if args.rdp is not None:
        if not set_rdp_level(cli_path, args.rdp):
            print("\nERROR: Failed to set RDP level!")
            return 1

    # Reset device
    if not args.no_reset:
        reset_device(cli_path)

    print("\n" + "="*60)
    print("  FLASHING COMPLETE!")
    print("="*60)

    if bootloader_path and firmware_path:
        print("\nDevice is ready to use.")
    elif bootloader_path:
        print("\nBootloader installed. You can now update firmware via USB.")
    elif firmware_path:
        print("\nFirmware updated.")

    return 0

if __name__ == "__main__":
    sys.exit(main())
