#!/usr/bin/env python3
"""
debug_verify_sfu.py - Verify .sfu file signature locally

This script helps debug signature verification issues by:
1. Computing the SHA-256 hash of the encrypted data
2. Verifying the ECDSA signature using the same keys as the bootloader
3. Showing the hash bytes (to compare with bootloader debug output)
"""

import sys
import struct
import hashlib
import json
import os

try:
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.asymmetric.utils import encode_dss_signature
    from cryptography.hazmat.backends import default_backend
except ImportError:
    print("ERROR: cryptography library required")
    print("Install with: pip install cryptography")
    sys.exit(1)

SFU_MAGIC = 0x5546534C  # "LSFU"
SFU_HEADER_SIZE = 100

def main():
    if len(sys.argv) < 2:
        # Default to LeShuffler.sfu in same directory
        script_dir = os.path.dirname(os.path.abspath(__file__))
        sfu_path = os.path.join(script_dir, "LeShuffler.sfu")
    else:
        sfu_path = sys.argv[1]

    # Load keys
    keys_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "test_keys.json")
    if not os.path.exists(keys_path):
        print(f"ERROR: Keys file not found: {keys_path}")
        return 1

    with open(keys_path, 'r') as f:
        keys = json.load(f)

    print(f"Keys file: {keys_path}")
    print(f"SFU file: {sfu_path}")
    print("=" * 60)

    # Load SFU file
    if not os.path.exists(sfu_path):
        print(f"ERROR: SFU file not found: {sfu_path}")
        return 1

    with open(sfu_path, 'rb') as f:
        data = f.read()

    print(f"Total file size: {len(data)} bytes")

    # Parse header
    magic, version, firmware_size, original_size = struct.unpack('<IIII', data[0:16])
    iv = data[16:32]
    signature = data[32:96]
    header_crc = struct.unpack('<I', data[96:100])[0]

    print()
    print("=== HEADER ===")
    print(f"Magic: 0x{magic:08X} ({'OK' if magic == SFU_MAGIC else 'WRONG!'})")
    print(f"Version: 0x{version:04X}")
    print(f"Encrypted size: {firmware_size} bytes")
    print(f"Original size: {original_size} bytes")
    print(f"Header CRC: 0x{header_crc:08X}")
    print(f"IV: {iv.hex()}")
    print(f"Signature: {signature.hex()[:32]}...{signature.hex()[-32:]}")

    # Extract encrypted data
    encrypted_data = data[SFU_HEADER_SIZE:SFU_HEADER_SIZE + firmware_size]
    print()
    print(f"=== ENCRYPTED DATA ===")
    print(f"Extracted size: {len(encrypted_data)} bytes")

    # Compute SHA-256 hash
    hash_obj = hashlib.sha256()
    hash_obj.update(encrypted_data)
    computed_hash = hash_obj.digest()

    print()
    print("=== SHA-256 HASH ===")
    print(f"Computed hash: {computed_hash.hex()}")
    print()
    print("Hash bytes (for comparison with bootloader):")
    print("  ", ", ".join(f"0x{b:02X}" for b in computed_hash[:16]))
    print("  ", ", ".join(f"0x{b:02X}" for b in computed_hash[16:]))

    # Verify signature
    print()
    print("=== SIGNATURE VERIFICATION ===")

    public_key_bytes = bytes.fromhex(keys['ecdsa_public_key'])
    print(f"Public key: {public_key_bytes.hex()[:32]}...")

    # Reconstruct public key
    x = int.from_bytes(public_key_bytes[:32], 'big')
    y = int.from_bytes(public_key_bytes[32:], 'big')

    from cryptography.hazmat.primitives.asymmetric.ec import EllipticCurvePublicNumbers
    public_numbers = EllipticCurvePublicNumbers(x, y, ec.SECP256R1())
    public_key = public_numbers.public_key(default_backend())

    # Extract r and s from signature
    r = int.from_bytes(signature[:32], 'big')
    s = int.from_bytes(signature[32:], 'big')
    signature_der = encode_dss_signature(r, s)

    print(f"Signature r: {signature[:32].hex()}")
    print(f"Signature s: {signature[32:].hex()}")

    # Verify using prehashed algorithm
    from cryptography.hazmat.primitives.asymmetric import utils
    try:
        public_key.verify(
            signature_der,
            computed_hash,
            ec.ECDSA(utils.Prehashed(hashes.SHA256()))
        )
        print()
        print("SIGNATURE VALID!")
        print()
        print("If bootloader reports signature failure, the issue is in the bootloader's")
        print("ECDSA verification or hash computation, not in the .sfu file.")
    except Exception as e:
        print()
        print(f"SIGNATURE INVALID: {e}")
        print()
        print("The .sfu file signature is wrong - recreate with correct keys.")
        return 1

    # Additional debug: show what bootloader should receive
    print()
    print("=== BOOTLOADER DEBUG INFO ===")
    print(f"Number of ENC_DATA packets: {(firmware_size + 255) // 256}")
    print(f"Last packet size: {firmware_size % 256 if firmware_size % 256 != 0 else 256} bytes")

    # Show first and last packet data for verification
    print()
    print("First 16 bytes of encrypted data:")
    print("  ", " ".join(f"{b:02X}" for b in encrypted_data[:16]))
    print()
    print("Last 16 bytes of encrypted data:")
    print("  ", " ".join(f"{b:02X}" for b in encrypted_data[-16:]))

    return 0


if __name__ == '__main__':
    sys.exit(main())
