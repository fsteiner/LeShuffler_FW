#!/usr/bin/env python3
"""
Debug script to capture hash from bootloader ENC_END response.
Run this while watching serial port during encrypted update.
"""

import serial
import serial.tools.list_ports
import sys
import time
import os
import hashlib
import struct

# SFU constants
SFU_HEADER_SIZE = 100
SFU_MAGIC = 0x5546534C

def compute_expected_hash(sfu_path):
    """Compute expected hash from .sfu file"""
    if not os.path.exists(sfu_path):
        return None

    with open(sfu_path, 'rb') as f:
        data = f.read()

    if len(data) < SFU_HEADER_SIZE:
        return None

    # Parse header to get firmware_size
    magic, version, firmware_size, original_size = struct.unpack('<IIII', data[0:16])

    if magic != SFU_MAGIC:
        return None

    # Extract encrypted data and compute hash
    encrypted_data = data[SFU_HEADER_SIZE:SFU_HEADER_SIZE + firmware_size]
    return hashlib.sha256(encrypted_data).hexdigest()

# Default expected hash - will be overwritten if .sfu file found
EXPECTED_HASH = None

def list_ports():
    """List available serial ports."""
    ports = serial.tools.list_ports.comports()
    print("\nAvailable ports:")
    for i, port in enumerate(ports):
        print(f"  {i}: {port.device} - {port.description}")
    return ports

def capture_hash(port_name):
    """Capture debug hash from bootloader."""
    print(f"\nConnecting to {port_name}...")

    try:
        ser = serial.Serial(port_name, 115200, timeout=0.1)
    except Exception as e:
        print(f"Error opening port: {e}")
        return

    print("Listening for debug hash (0xDB marker)...")
    print("Run the encrypted updater now.\n")

    buffer = bytearray()

    try:
        while True:
            data = ser.read(64)
            if data:
                buffer.extend(data)

                # Look for debug marker (0xDB)
                while len(buffer) >= 33:
                    # Find 0xDB marker
                    try:
                        idx = buffer.index(0xDB)
                    except ValueError:
                        # No marker found, keep last 32 bytes in case marker at boundary
                        if len(buffer) > 32:
                            buffer = buffer[-32:]
                        break

                    # Check if we have enough bytes after marker
                    if len(buffer) - idx >= 33:
                        # Extract hash
                        computed_hash = bytes(buffer[idx+1:idx+33]).hex()

                        print("="*60)
                        print("DEBUG HASH RECEIVED FROM BOOTLOADER")
                        print("="*60)
                        print(f"Computed: {computed_hash}")

                        if EXPECTED_HASH:
                            print(f"Expected: {EXPECTED_HASH}")
                            print()

                            if computed_hash == EXPECTED_HASH:
                                print("MATCH! Hashes are identical.")
                            else:
                                print("MISMATCH! Hashes differ.")
                                # Show byte-by-byte comparison
                                print("\nByte comparison:")
                                comp_bytes = bytes.fromhex(computed_hash)
                                exp_bytes = bytes.fromhex(EXPECTED_HASH)
                                for i in range(32):
                                    if comp_bytes[i] != exp_bytes[i]:
                                        print(f"  Byte {i:2d}: computed=0x{comp_bytes[i]:02x}, expected=0x{exp_bytes[i]:02x} <-- DIFF")
                                    else:
                                        print(f"  Byte {i:2d}: 0x{comp_bytes[i]:02x}")
                        else:
                            print("(No expected hash available for comparison)")

                        print("="*60)

                        # Remove processed data
                        buffer = buffer[idx+33:]
                    else:
                        break

                # Also display any standard responses (0xAA header)
                for byte in data:
                    if byte == 0xAA:
                        print(f"[Response packet start: 0xAA]")

    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        ser.close()

def main():
    global EXPECTED_HASH

    # Try to find .sfu file and compute expected hash
    script_dir = os.path.dirname(os.path.abspath(__file__))
    sfu_path = os.path.join(script_dir, "LeShuffler.sfu")

    if os.path.exists(sfu_path):
        EXPECTED_HASH = compute_expected_hash(sfu_path)
        if EXPECTED_HASH:
            print(f"Loaded expected hash from: {sfu_path}")
            print(f"Expected: {EXPECTED_HASH}")
        else:
            print(f"Warning: Could not parse {sfu_path}")
    else:
        print(f"Warning: No .sfu file found at {sfu_path}")
        print("Comparison will not be available.")

    if len(sys.argv) > 1:
        port_name = sys.argv[1]
    else:
        ports = list_ports()
        if not ports:
            print("No serial ports found!")
            return

        try:
            choice = int(input("\nSelect port number: "))
            port_name = ports[choice].device
        except (ValueError, IndexError):
            print("Invalid selection")
            return

    capture_hash(port_name)

if __name__ == "__main__":
    main()
