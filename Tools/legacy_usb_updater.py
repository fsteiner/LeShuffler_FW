#!/usr/bin/env python3
"""
LeShuffler Legacy USB Updater (Self-Erasing)

Self-deleting firmware updater for legacy bootloaders (v1.x/v2.x).
Embeds firmware binary, performs USB update, then deletes itself.

SECURITY: This tool provides soft protection against casual firmware copying.
The firmware binary and this tool are deleted after successful update.

Usage (by end user):
    LeShuffler_Legacy_Updater.exe

Requirements:
    - Device in bootloader mode (Settings > Maintenance > Firmware Update)
    - USB connection to PC
"""

import os
import sys
import time
import struct
import tempfile
import base64
import atexit

# Handle pyserial import for PyInstaller
try:
    import serial
    import serial.tools.list_ports
except ImportError as e:
    print(f"ERROR: Failed to import serial library: {e}")
    print("Please install pyserial: pip install pyserial")
    input("\nPress Enter to exit...")
    sys.exit(1)

# ============================================================================
# EMBEDDED FIRMWARE BINARY (Base64 encoded)
# This is populated during the build process - DO NOT commit with real data
# ============================================================================
FIRMWARE_B64 = """
REPLACE_WITH_BASE64_ENCODED_FIRMWARE
"""

# Protocol constants (must match bootloader)
PACKET_TYPE_START = 0x01
PACKET_TYPE_DATA = 0x02
PACKET_TYPE_END = 0x03
PACKET_TYPE_STATUS = 0x04

RESPONSE_ACK = 0x06
RESPONSE_NAK = 0x15
RESPONSE_COMPLETE = 0x07
RESPONSE_ERROR = 0x08

CHUNK_SIZE = 256
FIRMWARE_BASE_ADDR = 0x08020000

# Cleanup flag
cleanup_enabled = True
update_successful = False


def get_app_dir():
    """Get directory where script/exe is located"""
    if getattr(sys, 'frozen', False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))


def secure_delete_file(filepath):
    """Overwrite file with random data before deleting (unrecoverable)"""
    try:
        if not os.path.exists(filepath):
            return True

        file_size = os.path.getsize(filepath)

        # Overwrite with random data 3 times
        for _ in range(3):
            with open(filepath, 'wb') as f:
                f.write(os.urandom(file_size))
                f.flush()
                os.fsync(f.fileno())

        os.remove(filepath)
        return True
    except Exception as e:
        print(f"[WARN] Secure delete failed for {filepath}: {e}")
        try:
            os.remove(filepath)
        except:
            pass
        return False


def cleanup_on_exit():
    """Delete sensitive files on exit"""
    global cleanup_enabled, update_successful

    if not cleanup_enabled:
        return

    print("\n" + "=" * 50)
    if update_successful:
        print("SECURE CLEANUP - Removing updater...")
    else:
        print("CLEANUP - Update incomplete, removing updater anyway...")
    print("=" * 50)

    # Delete this script/exe itself
    app_path = sys.executable if getattr(sys, 'frozen', False) else __file__

    if sys.platform == 'win32':
        # On Windows, can't delete running exe directly
        # Create a batch file to delete after exit
        import subprocess
        batch_content = f'''@echo off
ping 127.0.0.1 -n 2 > nul
del /f /q "{app_path}"
del /f /q "%~f0"
'''
        batch_path = os.path.join(os.path.dirname(app_path), "_cleanup.bat")
        try:
            with open(batch_path, 'w') as f:
                f.write(batch_content)
            subprocess.Popen(
                ['cmd', '/c', batch_path],
                creationflags=subprocess.CREATE_NO_WINDOW if hasattr(subprocess, 'CREATE_NO_WINDOW') else 0
            )
            print(f"[OK] Scheduled deletion of updater")
        except Exception as e:
            print(f"[WARN] Could not schedule self-deletion: {e}")
    else:
        # On Unix, can delete running script
        try:
            os.remove(app_path)
            print(f"[OK] Deleted updater")
        except Exception as e:
            print(f"[WARN] Could not delete: {e}")

    print("\nCleanup complete.")


def extract_firmware():
    """Extract firmware from embedded base64"""
    if "REPLACE_WITH" in FIRMWARE_B64:
        print("ERROR: Firmware not embedded. This is a template file.")
        print("Run the build script to embed the firmware binary.")
        return None

    try:
        firmware_data = base64.b64decode(FIRMWARE_B64.strip())
        print(f"[OK] Firmware loaded ({len(firmware_data):,} bytes)")
        return firmware_data
    except Exception as e:
        print(f"ERROR: Failed to decode firmware: {e}")
        return None


def list_serial_ports():
    """List available serial ports"""
    ports = serial.tools.list_ports.comports()
    return [(p.device, p.description) for p in ports]


def select_port():
    """Let user select a serial port"""
    ports = list_serial_ports()

    if not ports:
        print("ERROR: No serial ports found!")
        print("\nMake sure:")
        print("  1. Device is connected via USB")
        print("  2. Device is in bootloader mode")
        print("     (Settings > Maintenance > Firmware Update)")
        return None

    print("\nAvailable ports:")
    for i, (port, desc) in enumerate(ports):
        print(f"  {i + 1}. {port} - {desc}")

    if len(ports) == 1:
        print(f"\nUsing only available port: {ports[0][0]}")
        return ports[0][0]

    while True:
        try:
            choice = input(f"\nSelect port (1-{len(ports)}): ").strip()
            idx = int(choice) - 1
            if 0 <= idx < len(ports):
                return ports[idx][0]
        except ValueError:
            pass
        print("Invalid selection")


def send_packet(ser, packet_type, address, data):
    """Send a firmware packet and wait for response"""
    length = len(data)

    # Calculate CRC32
    import binascii
    crc = binascii.crc32(data) & 0xFFFFFFFF

    # Build packet: type(4) + address(4) + length(4) + crc(4) + data
    header = struct.pack('<IIII', packet_type, address, length, crc)
    packet = header + data

    ser.write(packet)
    ser.flush()

    # Wait for response
    response = ser.read(1)
    if not response:
        return None
    return response[0]


def perform_update(port, firmware_data):
    """Perform the firmware update"""
    global update_successful

    try:
        ser = serial.Serial(port, 115200, timeout=5)
        time.sleep(0.5)  # Let connection stabilize
    except Exception as e:
        print(f"ERROR: Could not open {port}: {e}")
        return False

    try:
        firmware_size = len(firmware_data)
        total_packets = (firmware_size + CHUNK_SIZE - 1) // CHUNK_SIZE

        print(f"\nFirmware size: {firmware_size:,} bytes")
        print(f"Packets to send: {total_packets}")
        print()

        # Send START packet
        print("Sending START packet...")
        start_data = struct.pack('<I', firmware_size)
        start_data = start_data.ljust(CHUNK_SIZE, b'\x00')

        response = send_packet(ser, PACKET_TYPE_START, FIRMWARE_BASE_ADDR, start_data)

        if response != RESPONSE_ACK:
            print(f"ERROR: START failed (response: {response})")
            print("\nMake sure device is in bootloader mode:")
            print("  Settings > Maintenance > Firmware Update")
            print("  Wait for 3 short beeps + 1 long beep")
            return False

        print("[OK] Flash erased, starting transfer...")

        # Send DATA packets
        bytes_sent = 0
        packet_num = 0

        while bytes_sent < firmware_size:
            chunk = firmware_data[bytes_sent:bytes_sent + CHUNK_SIZE]

            # Pad last chunk if needed
            if len(chunk) < CHUNK_SIZE:
                chunk = chunk.ljust(CHUNK_SIZE, b'\xFF')

            address = FIRMWARE_BASE_ADDR + bytes_sent
            response = send_packet(ser, PACKET_TYPE_DATA, address, chunk)

            if response != RESPONSE_ACK:
                print(f"\nERROR: DATA packet {packet_num} failed")
                return False

            bytes_sent += CHUNK_SIZE
            packet_num += 1

            # Progress bar
            progress = min(bytes_sent, firmware_size)
            pct = progress * 100 // firmware_size
            bar = '=' * (pct // 2) + ' ' * (50 - pct // 2)
            print(f"\r[{bar}] {pct}% ({progress:,}/{firmware_size:,})", end='', flush=True)

        print()  # Newline after progress

        # Send END packet
        print("Sending END packet...")
        end_data = b'\x00' * CHUNK_SIZE
        response = send_packet(ser, PACKET_TYPE_END, 0, end_data)

        if response == RESPONSE_COMPLETE:
            print("\n" + "=" * 50)
            print("UPDATE SUCCESSFUL!")
            print("=" * 50)
            print("\nDevice will restart automatically.")
            update_successful = True
            return True
        else:
            print(f"ERROR: END packet failed (response: {response})")
            return False

    except Exception as e:
        print(f"\nERROR: {e}")
        return False
    finally:
        ser.close()


def main():
    global cleanup_enabled

    print("=" * 60)
    print("LeShuffler Legacy Firmware Updater")
    print("=" * 60)
    print()
    print("This updater will:")
    print("  1. Update firmware via USB")
    print("  2. DELETE ITSELF after completion")
    print()
    print("For devices with legacy bootloader (v1.x/v2.x)")
    print()

    # Register cleanup on exit
    atexit.register(cleanup_on_exit)

    # Extract firmware
    firmware_data = extract_firmware()
    if not firmware_data:
        input("\nPress Enter to exit...")
        return 1

    # Select port
    port = select_port()
    if not port:
        input("\nPress Enter to exit...")
        return 1

    # Confirm
    print()
    confirm = input("Device in bootloader mode? Type 'yes' to proceed: ").strip().lower()
    if confirm != 'yes':
        print("Cancelled.")
        cleanup_enabled = False  # Don't delete if user cancelled
        input("\nPress Enter to exit...")
        return 0

    # Perform update
    success = perform_update(port, firmware_data)

    if not success:
        print("\nUpdate failed. You can try again.")
        cleanup_enabled = False  # Keep updater if failed so user can retry

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
