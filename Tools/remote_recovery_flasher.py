#!/usr/bin/env python3
"""
LeShuffler Remote Recovery Flasher

Self-deleting flasher for remote support sessions.
Flashes bootloader + sets RDP Level 1, then deletes itself and all binaries.

SECURITY: This tool is designed for remote recovery sessions where the
support technician controls the process. The bootloader binary and this
tool are deleted after successful flashing.

Usage (by support technician via remote session):
    python remote_recovery_flasher.py

Requirements:
    - OpenOCD installed or openocd.exe in same folder
    - ST-LINK connected to device
"""

import os
import sys
import subprocess
import tempfile
import shutil
import time
import base64
import atexit
from pathlib import Path

# ============================================================================
# EMBEDDED BOOTLOADER BINARY (Base64 encoded)
# This is populated during the build process - DO NOT commit with real data
# ============================================================================
BOOTLOADER_B64 = """
REPLACE_WITH_BASE64_ENCODED_BOOTLOADER
"""

# Flash configuration
BOOTLOADER_ADDR = 0x08000000
RDP_LEVEL_1_VALUE = "0x0000BB00"
RDP_REG_OFFSET = "0x01C"

# Temp directory for extraction
TEMP_DIR = None


def get_app_dir():
    """Get directory where script/exe is located"""
    if getattr(sys, 'frozen', False):
        return Path(sys.executable).parent
    return Path(__file__).parent


def secure_delete_file(filepath):
    """Overwrite file with random data before deleting (unrecoverable)"""
    try:
        if not os.path.exists(filepath):
            return True

        # Get file size
        file_size = os.path.getsize(filepath)

        # Overwrite with random data 3 times
        for pass_num in range(3):
            with open(filepath, 'wb') as f:
                f.write(os.urandom(file_size))
                f.flush()
                os.fsync(f.fileno())

        # Then delete
        os.remove(filepath)
        return True
    except Exception as e:
        print(f"[WARN] Secure delete failed for {filepath}: {e}")
        # Fall back to regular delete
        try:
            os.remove(filepath)
        except:
            pass
        return False


def secure_delete_directory(dirpath):
    """Securely delete all files in directory, then remove directory"""
    if not os.path.exists(dirpath):
        return

    for root, dirs, files in os.walk(dirpath):
        for fname in files:
            fpath = os.path.join(root, fname)
            secure_delete_file(fpath)

    # Remove empty directories
    shutil.rmtree(dirpath, ignore_errors=True)


def cleanup_files():
    """Delete all sensitive files - called on exit"""
    global TEMP_DIR

    print("\n" + "="*50)
    print("SECURE CLEANUP - Removing all sensitive files...")
    print("="*50)

    # Securely delete temp directory with bootloader
    if TEMP_DIR and os.path.exists(TEMP_DIR):
        try:
            secure_delete_directory(TEMP_DIR)
            print(f"[OK] Securely deleted temp directory: {TEMP_DIR}")
        except Exception as e:
            print(f"[WARN] Could not delete temp dir: {e}")

    # Delete this script/exe itself
    app_path = Path(sys.executable if getattr(sys, 'frozen', False) else __file__)

    if sys.platform == 'win32':
        # On Windows, can't delete running exe directly
        # Create a batch file to delete after exit
        batch_content = f'''@echo off
ping 127.0.0.1 -n 2 > nul
del /f /q "{app_path}"
del /f /q "%~f0"
'''
        batch_path = app_path.parent / "_cleanup.bat"
        try:
            with open(batch_path, 'w') as f:
                f.write(batch_content)
            subprocess.Popen(
                ['cmd', '/c', str(batch_path)],
                creationflags=subprocess.CREATE_NO_WINDOW if hasattr(subprocess, 'CREATE_NO_WINDOW') else 0
            )
            print(f"[OK] Scheduled deletion of: {app_path}")
        except Exception as e:
            print(f"[WARN] Could not schedule self-deletion: {e}")
    else:
        # On Unix, can delete running script
        try:
            os.remove(app_path)
            print(f"[OK] Deleted: {app_path}")
        except Exception as e:
            print(f"[WARN] Could not delete: {e}")

    print("\nCleanup complete. All sensitive files removed.")


def find_openocd():
    """Find OpenOCD executable"""
    app_dir = get_app_dir()

    # Check in app directory
    if sys.platform == 'win32':
        local_path = app_dir / "openocd.exe"
    else:
        local_path = app_dir / "openocd"

    if local_path.exists():
        return str(local_path)

    # Check in PATH
    try:
        result = subprocess.run(
            ["where" if sys.platform == 'win32' else "which", "openocd"],
            capture_output=True, text=True
        )
        if result.returncode == 0:
            return result.stdout.strip().split('\n')[0]
    except:
        pass

    return None


def extract_bootloader():
    """Extract bootloader from embedded base64 to temp file"""
    global TEMP_DIR

    if "REPLACE_WITH" in BOOTLOADER_B64:
        print("ERROR: Bootloader not embedded. This is a template file.")
        print("Run the build script to embed the bootloader binary.")
        return None

    TEMP_DIR = tempfile.mkdtemp(prefix="lsrecovery_")
    bootloader_path = Path(TEMP_DIR) / "bootloader.bin"

    try:
        bootloader_data = base64.b64decode(BOOTLOADER_B64.strip())
        with open(bootloader_path, 'wb') as f:
            f.write(bootloader_data)
        print(f"[OK] Extracted bootloader ({len(bootloader_data):,} bytes)")
        return str(bootloader_path)
    except Exception as e:
        print(f"ERROR: Failed to extract bootloader: {e}")
        return None


def run_openocd(openocd_path, commands):
    """Run OpenOCD with given commands"""
    # Build command line
    cmd = [openocd_path, "-f", "interface/stlink.cfg", "-f", "target/stm32h7x.cfg"]
    for c in commands:
        cmd.extend(["-c", c])

    print(f"Running: {' '.join(cmd[:6])}...")  # Don't print full command (sensitive paths)

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)

        # OpenOCD outputs to stderr
        output = result.stderr + result.stdout

        if "Error" in output or result.returncode != 0:
            print(f"OpenOCD output:\n{output}")
            return False

        return True
    except subprocess.TimeoutExpired:
        print("ERROR: OpenOCD timeout")
        return False
    except Exception as e:
        print(f"ERROR: {e}")
        return False


def probe_device(openocd_path):
    """Check if device is connected"""
    print("\nProbing for ST-LINK and device...")

    commands = [
        "init",
        "reset halt",
        "exit"
    ]

    return run_openocd(openocd_path, commands)


def flash_bootloader(openocd_path, bootloader_path):
    """Flash bootloader to device"""
    print(f"\nFlashing bootloader to 0x{BOOTLOADER_ADDR:08X}...")

    commands = [
        "init",
        "reset halt",
        f"program {bootloader_path} 0x{BOOTLOADER_ADDR:08X} verify",
        "exit"
    ]

    return run_openocd(openocd_path, commands)


def set_rdp_level_1(openocd_path):
    """Set Read Protection Level 1"""
    print("\nSetting RDP Level 1...")

    commands = [
        "init",
        "reset halt",
        f"stm32h7x option_write 0 {RDP_REG_OFFSET} {RDP_LEVEL_1_VALUE}",
        "reset exit"
    ]

    return run_openocd(openocd_path, commands)


def main():
    print("="*60)
    print("LeShuffler Remote Recovery Flasher")
    print("="*60)
    print()
    print("WARNING: This tool will:")
    print("  1. Flash the bootloader to the device")
    print("  2. Set RDP Level 1 (read protection)")
    print("  3. DELETE ITSELF and all extracted files")
    print()
    print("This is a ONE-TIME use tool for remote recovery sessions.")
    print()

    # Register cleanup on exit (success or failure)
    atexit.register(cleanup_files)

    # Find OpenOCD
    openocd = find_openocd()
    if not openocd:
        print("ERROR: OpenOCD not found!")
        print("Please ensure openocd.exe is in the same folder or installed in PATH.")
        input("\nPress Enter to exit (files will be cleaned up)...")
        return 1

    print(f"[OK] Found OpenOCD: {openocd}")

    # Extract bootloader
    bootloader_path = extract_bootloader()
    if not bootloader_path:
        input("\nPress Enter to exit...")
        return 1

    # Confirm with technician
    print()
    confirm = input("Connect ST-LINK and power device, then type 'FLASH' to proceed: ").strip()
    if confirm != "FLASH":
        print("Cancelled.")
        input("\nPress Enter to exit (files will be cleaned up)...")
        return 0

    # Probe device
    if not probe_device(openocd):
        print("\nERROR: Could not connect to device!")
        print("Check ST-LINK connection and device power.")
        input("\nPress Enter to exit (files will be cleaned up)...")
        return 1

    print("[OK] Device connected")

    # Flash bootloader
    if not flash_bootloader(openocd, bootloader_path):
        print("\nERROR: Failed to flash bootloader!")
        input("\nPress Enter to exit (files will be cleaned up)...")
        return 1

    print("[OK] Bootloader flashed successfully")

    # Set RDP Level 1
    if not set_rdp_level_1(openocd):
        print("\nWARNING: Failed to set RDP Level 1")
        print("Bootloader was flashed but device is NOT protected.")
        print("You may need to set RDP manually.")
    else:
        print("[OK] RDP Level 1 set")

    print("\n" + "="*60)
    print("SUCCESS!")
    print("="*60)
    print()
    print("Next steps:")
    print("  1. Disconnect ST-LINK")
    print("  2. Connect device via USB")
    print("  3. Enter bootloader mode (Settings > Maintenance > Firmware Update)")
    print("  4. Run LeShuffler_Updater.exe with the .sfu file")
    print()

    input("Press Enter to exit (files will be cleaned up)...")
    return 0


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
