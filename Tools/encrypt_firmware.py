#!/usr/bin/env python3
"""
encrypt_firmware.py - Create encrypted .sfu firmware files for LeShuffler

Usage:
    python encrypt_firmware.py input.bin output.sfu [--keys keyfile.json]
    python encrypt_firmware.py --generate-keys keyfile.json

This tool encrypts firmware with AES-256-CBC and signs with ECDSA-P256.
The output .sfu file can be flashed via the encrypted bootloader (v3.0+).
"""

import sys
import os
import json
import struct
import hashlib
import argparse
from binascii import crc32

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature
    from cryptography.hazmat.backends import default_backend
    from cryptography.hazmat.primitives import serialization
except ImportError:
    print("ERROR: cryptography library required")
    print("Install with: pip install cryptography")
    sys.exit(1)

# Constants matching bootloader crypto.h
SFU_MAGIC = 0x5546534C  # "LSFU" in little-endian
AES_KEY_SIZE = 32
AES_IV_SIZE = 16
AES_BLOCK_SIZE = 16
ECDSA_SIG_SIZE = 64


def get_firmware_version():
    """Extract firmware version from version.h (0x00MMmmPP format)"""
    # Look for version.h relative to this script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    version_h = os.path.join(script_dir, '..', 'Core', 'Inc', 'version.h')

    if not os.path.exists(version_h):
        print(f"WARNING: version.h not found at {version_h}")
        return 0x00010000  # Default v1.0.0

    major = minor = patch = 0
    try:
        with open(version_h, 'r') as f:
            for line in f:
                if '#define FW_VERSION_MAJOR' in line:
                    major = int(line.split()[-1])
                elif '#define FW_VERSION_MINOR' in line:
                    minor = int(line.split()[-1])
                elif '#define FW_VERSION_PATCH' in line:
                    patch = int(line.split()[-1])
    except Exception as e:
        print(f"WARNING: Could not parse version.h: {e}")
        return 0x00010000

    version = (major << 16) | (minor << 8) | patch
    return version


def format_version(version):
    """Format version number as string (e.g., 0x00010001 -> 'v1.0.1')"""
    major = (version >> 16) & 0xFF
    minor = (version >> 8) & 0xFF
    patch = version & 0xFF
    return f"v{major}.{minor}.{patch}"

# Default test keys (MUST match crypto_keys.h in bootloader)
DEFAULT_AES_KEY = bytes([
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
])


def pkcs7_pad(data, block_size=AES_BLOCK_SIZE):
    """Add PKCS7 padding to data"""
    padding_len = block_size - (len(data) % block_size)
    return data + bytes([padding_len] * padding_len)


def generate_keys(output_file):
    """Generate new AES and ECDSA key pair"""
    import secrets

    # Generate random AES-256 key
    aes_key = secrets.token_bytes(AES_KEY_SIZE)

    # Generate ECDSA P-256 key pair
    private_key = ec.generate_private_key(ec.SECP256R1(), default_backend())
    public_key = private_key.public_key()

    # Extract raw public key bytes (64 bytes: 32 for X, 32 for Y)
    public_numbers = public_key.public_numbers()
    public_key_bytes = (
        public_numbers.x.to_bytes(32, 'big') +
        public_numbers.y.to_bytes(32, 'big')
    )

    # Extract raw private key bytes
    private_numbers = private_key.private_numbers()
    private_key_bytes = private_numbers.private_value.to_bytes(32, 'big')

    # Save to JSON file
    keys = {
        'aes_key': aes_key.hex(),
        'ecdsa_private_key': private_key_bytes.hex(),
        'ecdsa_public_key': public_key_bytes.hex(),
        'description': 'LeShuffler firmware encryption keys - KEEP PRIVATE KEY SECRET!'
    }

    with open(output_file, 'w') as f:
        json.dump(keys, f, indent=2)

    print(f"Keys generated and saved to: {output_file}")
    print()
    print("=== PUBLIC KEY (for crypto_keys.h) ===")
    print("static const uint8_t ECDSA_PUBLIC_KEY[64] = {")
    for i in range(0, 64, 8):
        line = ", ".join(f"0x{b:02X}" for b in public_key_bytes[i:i+8])
        print(f"    {line},")
    print("};")
    print()
    print("=== AES KEY (for crypto_keys.h) ===")
    print("static const uint8_t AES_KEY[32] = {")
    for i in range(0, 32, 8):
        line = ", ".join(f"0x{b:02X}" for b in aes_key[i:i+8])
        print(f"    {line},")
    print("};")
    print()
    print("WARNING: Update Bootloader_E/Core/Inc/crypto_keys.h with these values!")

    return keys


def load_keys(key_file):
    """Load keys from JSON file"""
    with open(key_file, 'r') as f:
        keys = json.load(f)

    return {
        'aes_key': bytes.fromhex(keys['aes_key']),
        'ecdsa_private_key': bytes.fromhex(keys['ecdsa_private_key']),
        'ecdsa_public_key': bytes.fromhex(keys['ecdsa_public_key'])
    }


def sign_data(data, private_key_bytes):
    """Sign data with ECDSA-P256, return 64-byte raw signature (r || s)"""
    # Reconstruct private key from raw bytes
    private_value = int.from_bytes(private_key_bytes, 'big')
    private_key = ec.derive_private_key(private_value, ec.SECP256R1(), default_backend())

    # Hash the data with SHA-256
    digest = hashlib.sha256(data).digest()

    # Sign the hash
    from cryptography.hazmat.primitives.asymmetric import utils
    signature_der = private_key.sign(
        digest,
        ec.ECDSA(utils.Prehashed(hashes.SHA256()))
    )

    # Convert DER signature to raw r || s format (64 bytes)
    r, s = decode_dss_signature(signature_der)
    signature_raw = r.to_bytes(32, 'big') + s.to_bytes(32, 'big')

    return signature_raw


def encrypt_firmware(input_file, output_file, keys):
    """Encrypt firmware and create .sfu file"""

    # Get firmware version from version.h
    fw_version = get_firmware_version()
    print(f"Firmware version: {format_version(fw_version)}")

    # Read input firmware
    with open(input_file, 'rb') as f:
        firmware = f.read()

    original_size = len(firmware)
    print(f"Input firmware: {input_file}")
    print(f"Original size: {original_size} bytes")

    # Pad firmware to AES block size
    padded_firmware = pkcs7_pad(firmware)
    encrypted_size = len(padded_firmware)
    print(f"Padded size: {encrypted_size} bytes")

    # Generate random IV
    import secrets
    iv = secrets.token_bytes(AES_IV_SIZE)

    # Encrypt with AES-256-CBC
    cipher = Cipher(algorithms.AES(keys['aes_key']), modes.CBC(iv), backend=default_backend())
    encryptor = cipher.encryptor()
    encrypted_firmware = encryptor.update(padded_firmware) + encryptor.finalize()

    print(f"Encrypted size: {len(encrypted_firmware)} bytes")

    # Sign the encrypted data
    signature = sign_data(encrypted_firmware, keys['ecdsa_private_key'])
    print(f"Signature generated ({len(signature)} bytes)")

    # Build SFU header (without CRC first)
    # struct: magic(4) + version(4) + firmware_size(4) + original_size(4) + iv(16) + signature(64) + crc(4)
    header_without_crc = struct.pack('<IIII',
        SFU_MAGIC,
        fw_version,  # Firmware version from version.h
        encrypted_size,
        original_size
    ) + iv + signature

    # Calculate CRC32 of header (excluding CRC field itself)
    header_crc = crc32(header_without_crc) & 0xFFFFFFFF

    # Complete header
    header = header_without_crc + struct.pack('<I', header_crc)

    print(f"Header size: {len(header)} bytes")
    print(f"Header CRC: 0x{header_crc:08X}")

    # Write .sfu file
    with open(output_file, 'wb') as f:
        f.write(header)
        f.write(encrypted_firmware)

    total_size = len(header) + len(encrypted_firmware)
    print(f"Output file: {output_file}")
    print(f"Total .sfu size: {total_size} bytes")
    print()
    print("SUCCESS: Encrypted firmware created!")

    return True


def main():
    parser = argparse.ArgumentParser(
        description='Encrypt firmware for LeShuffler encrypted bootloader',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  Generate new keys:
    python encrypt_firmware.py --generate-keys keys.json

  Encrypt firmware with generated keys:
    python encrypt_firmware.py LeShuffler.bin LeShuffler.sfu --keys keys.json

  Encrypt firmware with default test keys:
    python encrypt_firmware.py LeShuffler.bin LeShuffler.sfu
"""
    )

    parser.add_argument('--generate-keys', metavar='FILE',
                        help='Generate new key pair and save to FILE')
    parser.add_argument('--keys', metavar='FILE',
                        help='Load keys from FILE (JSON format)')
    parser.add_argument('input', nargs='?', help='Input .bin firmware file')
    parser.add_argument('output', nargs='?', help='Output .sfu encrypted file')

    args = parser.parse_args()

    # Key generation mode
    if args.generate_keys:
        generate_keys(args.generate_keys)
        return 0

    # Encryption mode
    if not args.input or not args.output:
        parser.print_help()
        return 1

    if not os.path.exists(args.input):
        print(f"ERROR: Input file not found: {args.input}")
        return 1

    # Load keys
    if args.keys:
        if not os.path.exists(args.keys):
            print(f"ERROR: Key file not found: {args.keys}")
            return 1
        print(f"Loading keys from: {args.keys}")
        keys = load_keys(args.keys)
    else:
        print("WARNING: Using default TEST keys!")
        print("For production, generate keys with: --generate-keys")
        print()
        # We need a private key for signing, but crypto_keys.h only has public key
        # For testing, we need to either:
        # 1. Use a known test key pair
        # 2. Generate and update crypto_keys.h
        print("ERROR: No key file specified and no default private key available.")
        print("Please generate keys first:")
        print("  python encrypt_firmware.py --generate-keys test_keys.json")
        print("Then update Bootloader_E/Core/Inc/crypto_keys.h with the public key.")
        return 1

    # Encrypt firmware
    try:
        encrypt_firmware(args.input, args.output, keys)
        return 0
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())
