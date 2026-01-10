#!/usr/bin/env python3
"""
LeShuffler Firmware Updater v3.0 (Encrypted Only)
Updates firmware using encrypted .sfu files (requires bootloader v3.0+).

File format:
- .sfu: Encrypted firmware created by encrypt_firmware.py
        Uses AES-256-CBC encryption + ECDSA-P256 signature

Usage:
    python firmware_updater.py                    # Interactive mode (uses LeShuffler.sfu)
    python firmware_updater.py COM5              # Direct mode
    python firmware_updater.py --list            # List ports
    python firmware_updater.py --file X.sfu      # Specific file
"""

import sys
import struct
import time
import os
import platform
import glob

try:
    import serial
    import serial.tools.list_ports
except ImportError as e:
    print("\n" + "=" * 60)
    print("  ERROR: Required module 'pyserial' not found!")
    print("=" * 60)
    print(f"  Details: {e}")
    print("")
    print("  To install, run:")
    print("    pip install pyserial")
    print("=" * 60)
    input("\n  Press Enter to exit...")
    sys.exit(1)

# ============================================================================
# CONFIGURATION
# ============================================================================

FIRMWARE_FILENAME = "LeShuffler.sfu"  # Default encrypted firmware file
APPLICATION_ADDRESS = 0x08020000
MAX_FIRMWARE_SIZE = 1024 * 1024  # 1MB max

# Protocol constants
PACKET_TYPE_STATUS = 0x04      # Query bootloader status (version detection)
PACKET_TYPE_ENC_START = 0x10   # Contains SFU header
PACKET_TYPE_ENC_DATA = 0x11    # Contains encrypted data chunk
PACKET_TYPE_ENC_END = 0x12     # Triggers signature verification

PACKET_DATA_SIZE = 256
STATUS_RESPONSE_SIZE = 24

# SFU Header constants (must match crypto.h)
SFU_MAGIC = 0x5546534C  # "LSFU" in little-endian
SFU_HEADER_SIZE = 100   # 4+4+4+4+16+64+4 = 100 bytes

# Timing
PACKET_DELAY_MS = 50
MAX_RECONNECT_WAIT = 15
MAX_RESTART_ATTEMPTS = 15  # More retries for flaky macOS USB


def get_executable_dir():
    if getattr(sys, 'frozen', False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))


def get_firmware_path(filename=None):
    """Get firmware path for .sfu file"""
    if filename:
        if os.path.isabs(filename):
            return filename
        return os.path.join(get_executable_dir(), filename)

    # Default: look for LeShuffler.sfu
    return os.path.join(get_executable_dir(), "LeShuffler.sfu")


def list_serial_ports():
    ports = []
    for port in serial.tools.list_ports.comports():
        info = {
            'device': port.device,
            'description': port.description or 'Unknown',
            'hwid': port.hwid or '',
            'is_stm32': False
        }
        desc_lower = info['description'].lower()
        hwid_lower = info['hwid'].lower()
        if any(x in desc_lower for x in ['stm', 'stmicroelectronics', 'usb serial', 'com port']):
            info['is_stm32'] = True
        if 'vid:pid=0483' in hwid_lower:
            info['is_stm32'] = True
        ports.append(info)

    if platform.system() == 'Darwin':
        # On Mac, prefer cu.* over tty.* for outgoing connections
        # cu.* doesn't wait for carrier detect, better for firmware updates
        usbmodem_ports = glob.glob('/dev/cu.usbmodem*')
        existing_devices = [p['device'] for p in ports]
        for device in usbmodem_ports:
            if device not in existing_devices:
                ports.append({
                    'device': device,
                    'description': 'USB Modem',
                    'hwid': '',
                    'is_stm32': True
                })

        # Filter out tty.* duplicates when cu.* exists for same device
        # e.g., if cu.usbmodem1234 exists, remove tty.usbmodem1234
        cu_ports = set(p['device'] for p in ports if '/dev/cu.' in p['device'])
        ports = [p for p in ports if not (
            '/dev/tty.' in p['device'] and
            p['device'].replace('/dev/tty.', '/dev/cu.') in cu_ports
        )]

    ports.sort(key=lambda p: (not p['is_stm32'], p['device']))
    return ports


def display_port_menu(ports):
    print("\n" + "=" * 60)
    print("  AVAILABLE SERIAL PORTS")
    print("=" * 60)

    if not ports:
        print("\n  No serial ports found!")
        return None

    for i, port in enumerate(ports, 1):
        marker = " [LeShuffler]" if port['is_stm32'] else ""
        print(f"\n  [{i}] {port['device']}")
        print(f"      {port['description']}{marker}")

    print(f"\n  [R] Refresh port list")
    print(f"  [Q] Quit")
    print("\n" + "-" * 60)

    while True:
        choice = input("  Select port number: ").strip().upper()
        if choice == 'Q':
            return None
        elif choice == 'R':
            return 'REFRESH'
        elif choice.isdigit():
            num = int(choice)
            if 1 <= num <= len(ports):
                return ports[num - 1]['device']
        print(f"  Invalid input. Enter 1-{len(ports)}, R, or Q")


def select_port_interactive():
    while True:
        ports = list_serial_ports()

        # Auto-select if exactly one LeShuffler device found
        leshuffler_ports = [p for p in ports if p['is_stm32']]
        if len(leshuffler_ports) == 1:
            port = leshuffler_ports[0]
            print("\n" + "=" * 60)
            print("  DEVICE DETECTED")
            print("=" * 60)
            print(f"\n  Auto-selected: {port['device']}")
            print(f"                 {port['description']} [LeShuffler]")
            print("")
            return port['device']

        # Multiple devices or none detected - show menu
        result = display_port_menu(ports)
        if result == 'REFRESH':
            print("\n  Scanning for ports...")
            time.sleep(0.5)
            continue
        return result


class SFUFile:
    """Parser for .sfu encrypted firmware files"""

    def __init__(self, filepath):
        self.filepath = filepath
        self.header = None
        self.encrypted_data = None
        self.valid = False
        self.error = None

    def load(self):
        """Load and validate SFU file"""
        try:
            with open(self.filepath, 'rb') as f:
                data = f.read()

            if len(data) < SFU_HEADER_SIZE:
                self.error = "File too small to contain SFU header"
                return False

            # Parse header
            magic, version, firmware_size, original_size = struct.unpack('<IIII', data[0:16])

            if magic != SFU_MAGIC:
                self.error = f"Invalid magic: 0x{magic:08X} (expected 0x{SFU_MAGIC:08X})"
                return False

            iv = data[16:32]
            signature = data[32:96]
            header_crc = struct.unpack('<I', data[96:100])[0]

            self.header = {
                'magic': magic,
                'version': version,
                'firmware_size': firmware_size,
                'original_size': original_size,
                'iv': iv,
                'signature': signature,
                'header_crc': header_crc,
                'raw': data[0:SFU_HEADER_SIZE]
            }

            # Extract encrypted data
            self.encrypted_data = data[SFU_HEADER_SIZE:SFU_HEADER_SIZE + firmware_size]

            if len(self.encrypted_data) != firmware_size:
                self.error = f"Encrypted data size mismatch: got {len(self.encrypted_data)}, expected {firmware_size}"
                return False

            self.valid = True
            return True

        except FileNotFoundError:
            self.error = "File not found"
            return False
        except Exception as e:
            self.error = str(e)
            return False

    def get_version_string(self):
        """Get firmware version as string (e.g., 'v1.0.1')"""
        if not self.header:
            return "unknown"
        version = self.header['version']
        major = (version >> 16) & 0xFF
        minor = (version >> 8) & 0xFF
        patch = version & 0xFF
        return f"v{major}.{minor}.{patch}"

    def print_info(self):
        """Print SFU file information"""
        if not self.header:
            print("  No header loaded")
            return

        print(f"  Firmware version: {self.get_version_string()}")
        print(f"  Size: {self.header['original_size']} bytes")


class EncryptedFirmwareUpdater:
    def __init__(self, port, baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.is_windows = platform.system() == 'Windows'
        self.bootloader_version = None
        self.is_v3 = False

    def find_device_port(self):
        ports = list(serial.tools.list_ports.comports())
        for p in ports:
            if p.device == self.port:
                return p.device
        if os.path.exists(self.port):
            return self.port
        if 'usbmodem' in self.port:
            # Prefer cu.* over tty.* on Mac (cu.* doesn't wait for carrier detect)
            found = glob.glob('/dev/cu.usbmodem*')
            if not found:
                found = glob.glob('/dev/tty.usbmodem*')
            if found:
                return sorted(found)[0]
        elif 'ttyACM' in self.port:
            found = glob.glob('/dev/ttyACM*')
            if found:
                return sorted(found)[0]
        return None

    def connect(self, verbose=True):
        try:
            actual_port = self.find_device_port()
            if not actual_port:
                if verbose:
                    print(f"  Device not found")
                return False

            if verbose:
                print(f"  Opening {actual_port}...", end='', flush=True)

            if self.ser:
                try:
                    self.ser.close()
                except:
                    pass

            self.ser = serial.Serial()
            self.ser.port = actual_port
            self.ser.baudrate = self.baudrate
            self.ser.bytesize = serial.EIGHTBITS
            self.ser.parity = serial.PARITY_NONE
            self.ser.stopbits = serial.STOPBITS_ONE
            self.ser.timeout = 3.0
            self.ser.write_timeout = 3.0
            self.ser.rts = False
            self.ser.dtr = False
            self.ser.xonxoff = False
            self.ser.rtscts = False
            self.ser.dsrdtr = False

            self.ser.open()

            if self.is_windows:
                try:
                    self.ser.setRTS(False)
                    self.ser.setDTR(False)
                except:
                    pass

            self.port = actual_port
            if verbose:
                print(f" Connected!")

            time.sleep(0.2)
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            return True

        except Exception as e:
            if self.ser:
                try:
                    self.ser.close()
                except:
                    pass
                self.ser = None
            if verbose:
                print(f" Failed: {e}")
            return False

    def detect_bootloader_version(self):
        """Send STATUS packet to detect bootloader version"""
        print("  Detecting bootloader version...", end='', flush=True)

        packet = struct.pack('<III', PACKET_TYPE_STATUS, 0, 0)
        packet += struct.pack('<I', 0)
        packet += b'\xFF' * PACKET_DATA_SIZE

        try:
            self.ser.reset_input_buffer()
            self.ser.write(packet)
            self.ser.flush()

            old_timeout = self.ser.timeout
            self.ser.timeout = 2.0
            response = self.ser.read(STATUS_RESPONSE_SIZE)
            self.ser.timeout = old_timeout

            if len(response) >= STATUS_RESPONSE_SIZE:
                if response[0] == 0xAA and response[1] == PACKET_TYPE_STATUS:
                    version_major = response[2]
                    version_minor = response[3]
                    self.bootloader_version = f"{version_major}.{version_minor}"
                    self.is_v3 = version_major >= 3
                    protocol = "encrypted" if self.is_v3 else "legacy"
                    print(f" v{self.bootloader_version} ({protocol})")
                    return True

            # Check for older bootloader
            if len(response) >= 8 and response[0] == 0xAA and response[7] == 0x55:
                version_major = response[2] if len(response) > 2 else 0
                version_minor = response[3] if len(response) > 3 else 0
                if version_major > 0:
                    self.bootloader_version = f"{version_major}.{version_minor}"
                else:
                    self.bootloader_version = "1.x"
                self.is_v3 = False
                print(f" v{self.bootloader_version} (legacy - no encryption support)")
                return True

            print(f" no valid response")
            return False

        except Exception as e:
            print(f" detection failed: {e}")
            return False

    def disconnect(self):
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except:
                pass
            print("  Disconnected")

    def is_connected(self):
        if not self.ser:
            return False
        try:
            return self.ser.is_open
        except:
            return False

    def wait_for_response(self, timeout=5.0):
        try:
            old_timeout = self.ser.timeout
            self.ser.timeout = timeout
            response = self.ser.read(8)
            self.ser.timeout = old_timeout

            if len(response) == 8 and response[0] == 0xAA and response[7] == 0x55:
                return {
                    'success': response[2] == 0x01,
                    'progress': response[3],
                    'state': response[4],
                    'packet_type': response[1],
                    'error_code': response[5] if len(response) > 5 else 0
                }
            return None
        except:
            return None

    def wait_for_reconnect(self):
        print(f"\n  Waiting for device to reconnect...")

        if self.ser:
            try:
                self.ser.close()
            except:
                pass
            self.ser = None

        for i in range(MAX_RECONNECT_WAIT):
            time.sleep(1.0)
            print(f"\r  Waiting... {MAX_RECONNECT_WAIT - i - 1}s  ", end='', flush=True)
            if self.connect(verbose=False):
                print(f"\r  Device reconnected!                    ")
                return True

        print(f"\r  Device did not reconnect")
        return False

    def send_enc_start_packet(self, sfu_header):
        """Send ENC_START packet with SFU header"""
        print(f"  Sending ENC_START packet (header + erase flash)...")

        # Build packet: type + address + length + crc + data (header in data field)
        packet = struct.pack('<III', PACKET_TYPE_ENC_START, APPLICATION_ADDRESS, SFU_HEADER_SIZE)
        packet += struct.pack('<I', 0)  # CRC not used for header
        # Pad header to PACKET_DATA_SIZE
        header_padded = sfu_header + b'\xFF' * (PACKET_DATA_SIZE - len(sfu_header))
        packet += header_padded

        try:
            self.ser.write(packet)
            self.ser.flush()
        except Exception as e:
            print(f"  Failed to send ENC_START: {e}")
            return False

        # Wait longer for flash erase + header validation
        response = self.wait_for_response(timeout=15.0)
        if response and response['success']:
            print(f"  Header validated, flash erased, ready for encrypted data")
            return True
        else:
            error_msg = "Header validation failed"
            if response:
                error_code = response.get('error_code', 0)
                if error_code == 1:
                    error_msg = "Invalid SFU magic"
                elif error_code == 2:
                    error_msg = "Invalid firmware size"
                elif error_code == 3:
                    error_msg = "Crypto initialization failed"
            print(f"  ENC_START failed: {error_msg}")
            return False

    def send_enc_data_packet(self, offset, data):
        """Send ENC_DATA packet with encrypted chunk"""
        import binascii
        address = APPLICATION_ADDRESS + offset

        # Build packet
        length = len(data)
        crc = binascii.crc32(data) & 0xFFFFFFFF  # CRC of encrypted data
        packet = struct.pack('<III', PACKET_TYPE_ENC_DATA, address, length)
        packet += struct.pack('<I', crc)
        data_padded = data + b'\xFF' * (PACKET_DATA_SIZE - length)
        packet += data_padded

        try:
            if not self.is_connected():
                return (False, True)  # Disconnected

            self.ser.write(packet)
            self.ser.flush()

            response = self.wait_for_response(timeout=10.0)
            if response and response['success']:
                return (True, False)
            elif response is None:
                return (False, True)  # Disconnect
            else:
                return (False, True)  # Treat as disconnect for retry

        except (serial.SerialException, OSError):
            return (False, True)
        except Exception:
            return (False, True)

    def send_enc_end_packet(self, expected_hash=None):
        """Send ENC_END packet to trigger signature verification
        Returns: 'success', 'usb_error', 'signature_failed', or 'hash_failed'
        """
        print("\n  Verifying signature...", end='', flush=True)

        packet = struct.pack('<III', PACKET_TYPE_ENC_END, 0, 0)
        packet += struct.pack('<I', 0)
        packet += b'\xFF' * PACKET_DATA_SIZE

        try:
            self.ser.write(packet)
            self.ser.flush()

            # First, check for debug hash response (0xDB marker + 32 bytes hash)
            # ECDSA verification takes ~1-2 seconds, so use longer timeout
            old_timeout = self.ser.timeout
            self.ser.timeout = 10.0
            debug_data = self.ser.read(64)  # Read more to find marker
            self.ser.timeout = old_timeout

            # DEBUG: Show what we received
            print(f"\n  [DEBUG] Received {len(debug_data)} bytes: {debug_data.hex() if debug_data else 'nothing'}")

            # Check for 0xEE error marker FIRST
            if debug_data and len(debug_data) >= 2 and debug_data[0] == 0xEE:
                print("\n  " + "=" * 60)
                print("  DEBUG: ERROR RESPONSE FROM BOOTLOADER")
                print("  " + "=" * 60)
                error_type = debug_data[1] if len(debug_data) > 1 else 0
                if error_type == 0x01:
                    print("  Error: g_debug_hash_ready flag not set")
                elif error_type == 0x02:
                    print("  Error: Hash finalize (SHA256_Finish) failed")
                    if len(debug_data) >= 10:
                        error_code = debug_data[2]
                        hash_started_flag = debug_data[3]
                        hal_state = debug_data[4]
                        fw_state = debug_data[5]
                        enc_bytes = debug_data[6] | (debug_data[7] << 8) | (debug_data[8] << 16) | (debug_data[9] << 24)

                        # Decode error code (from crypto.c)
                        error_str = "Unknown"
                        if error_code == 10:
                            error_str = "hash_started flag was 0 (hash not started)"
                        elif error_code == 101:
                            error_str = "HAL_ERROR - peripheral error"
                        elif error_code == 102:
                            error_str = "HAL_BUSY - peripheral still busy"
                        elif error_code == 103:
                            error_str = "HAL_TIMEOUT - operation timed out"
                        elif error_code == 11:
                            error_str = "OLD CODE: HAL fail (rebuild bootloader!)"

                        # Decode HAL state
                        hal_states = {0: "RESET", 1: "READY", 2: "BUSY", 3: "TIMEOUT", 4: "ERROR", 5: "SUSPENDED"}
                        hal_state_str = hal_states.get(hal_state, f"Unknown({hal_state})")

                        # Decode fw_state
                        fw_states = {0: "IDLE", 1: "RECEIVING", 2: "COMPLETE", 3: "ERROR"}
                        fw_state_str = fw_states.get(fw_state, f"Unknown({fw_state})")

                        print(f"  Error code:       {error_code} = {error_str}")
                        print(f"  hash_started:     {hash_started_flag} ({'YES' if hash_started_flag else 'NO'})")
                        print(f"  HAL state:        {hal_state} = {hal_state_str}")
                        print(f"  fw_update_state:  {fw_state} = {fw_state_str}")
                        print(f"  enc_received:     {enc_bytes} bytes (0x{enc_bytes:08X})")
                else:
                    print(f"  Unknown error type: 0x{error_type:02X}")
                print("  " + "=" * 60)
                return 'hash_failed'

            # Look for 0xDB marker and extract hash
            computed_hash = None
            if debug_data:
                # Find 0xDB marker
                try:
                    marker_idx = debug_data.index(0xDB)
                    if marker_idx + 33 <= len(debug_data):
                        computed_hash = debug_data[marker_idx+1:marker_idx+33].hex()
                        print(f"  [DEBUG] Found 0xDB marker at byte {marker_idx}")
                except ValueError:
                    # No 0xDB marker found, try raw 32 bytes
                    if len(debug_data) >= 32:
                        computed_hash = debug_data[0:32].hex()
                        print(f"  [DEBUG] No marker, using first 32 bytes")

            if computed_hash:
                print("\n  " + "=" * 50)
                print("  DEBUG: Hash from bootloader")
                print("  " + "=" * 50)
                print(f"  Computed: {computed_hash}")
                if expected_hash:
                    print(f"  Expected: {expected_hash}")
                    if computed_hash == expected_hash:
                        print("  MATCH!")
                    else:
                        print("  MISMATCH!")
                        # Show first differing byte
                        comp_bytes = bytes.fromhex(computed_hash)
                        exp_bytes = bytes.fromhex(expected_hash)
                        for i in range(32):
                            if comp_bytes[i] != exp_bytes[i]:
                                print(f"  First diff at byte {i}: 0x{comp_bytes[i]:02x} vs 0x{exp_bytes[i]:02x}")
                                break
                print("  " + "=" * 50)

            # Now wait for the actual response
            response = self.wait_for_response(timeout=10.0)
            if response and response['success']:
                print(" signature valid")
                return 'success'
            else:
                error_msg = "Signature verification failed"
                if response:
                    error_code = response.get('error_code', 0)
                    if error_code == 1:
                        error_msg = "Hash computation failed"
                    elif error_code == 2:
                        error_msg = "Invalid ECDSA signature"
                print(f"  ENC_END failed: {error_msg}")
                return 'signature_failed'

        except Exception as e:
            # USB disconnect after sending ENC_END - device is rebooting
            # Verification will confirm if update succeeded
            return 'usb_disconnect_after_end'

    def do_encrypted_transfer(self, sfu_file):
        """Perform encrypted firmware transfer"""
        encrypted_data = sfu_file.encrypted_data
        total_size = len(encrypted_data)
        total_packets = (total_size + PACKET_DATA_SIZE - 1) // PACKET_DATA_SIZE

        # Compute expected hash for debug comparison
        import hashlib
        expected_hash = hashlib.sha256(encrypted_data).hexdigest()

        # Send header
        if not self.send_enc_start_packet(sfu_file.header['raw']):
            return 'error'

        print(f"  Sending {total_packets} encrypted packets...")

        for packet_num in range(total_packets):
            offset = packet_num * PACKET_DATA_SIZE
            chunk_size = min(PACKET_DATA_SIZE, total_size - offset)
            chunk = encrypted_data[offset:offset + chunk_size]

            success, need_restart = self.send_enc_data_packet(offset, chunk)

            if need_restart:
                return 'disconnect'

            if not success:
                print(f"\n  Failed at packet {packet_num}")
                return 'error'

            # Progress display
            percent = ((packet_num + 1) * 100) // total_packets
            bar_length = 40
            filled = (percent * bar_length) // 100
            bar = '=' * filled + '-' * (bar_length - filled)
            bytes_sent = min((packet_num + 1) * PACKET_DATA_SIZE, total_size)
            print(f"\r  [{bar}] {percent}% ({bytes_sent}/{total_size})", end='', flush=True)

            time.sleep(PACKET_DELAY_MS / 1000.0)

        print()

        # Finalize with signature verification
        end_result = self.send_enc_end_packet(expected_hash)
        if end_result == 'success':
            return 'success'
        elif end_result == 'usb_disconnect_after_end':
            # USB disconnect after sending ENC_END is likely success
            # The device resets after successful verification
            return 'likely_success'
        elif end_result == 'usb_error':
            return 'usb_error'
        elif end_result == 'hash_failed':
            return 'hash_failed'
        else:
            return 'signature_failed'

    def update_firmware(self, firmware_path):
        """Main update function for encrypted .sfu files (requires v3.0+ bootloader)"""
        try:
            # Detect bootloader version FIRST
            if not self.detect_bootloader_version():
                print("\n" + "=" * 60)
                print("  CANNOT START UPDATE")
                print("=" * 60)
                print("  Device is not in bootloader mode.")
                print("")
                print("  To enter bootloader mode:")
                print("    1. Go to Settings > Maintenance > Firmware Update")
                print("    2. Confirm to enter update mode")
                print("    3. Wait for 3 short beeps + 1 long beep")
                print("    4. Run this updater again")
                print("=" * 60)
                return 'not_bootloader'

            # Require v3.0+ bootloader for encrypted updates
            if not self.is_v3:
                print("\n" + "=" * 60)
                print("  CANNOT START UPDATE")
                print("=" * 60)
                print(f"  Bootloader v{self.bootloader_version} does not support encryption.")
                print("  This updater requires bootloader v3.0 or newer.")
                print("")
                print("  For legacy bootloaders (v1.x/v2.x), use the")
                print("  LeShuffler_Legacy_Updater instead.")
                print("=" * 60)
                return 'unsupported_bootloader'

            # Load .sfu file
            print(f"\n  Firmware: {os.path.basename(firmware_path)}")
            sfu = SFUFile(firmware_path)
            if not sfu.load():
                print("\n" + "=" * 60)
                print("  CANNOT START UPDATE")
                print("=" * 60)
                print(f"  {sfu.error}")
                print("=" * 60)
                return False
            sfu.print_info()
            firmware_size = sfu.header['original_size']

            if firmware_size > MAX_FIRMWARE_SIZE:
                print("\n" + "=" * 60)
                print("  CANNOT START UPDATE")
                print("=" * 60)
                print("  Firmware file is too large.")
                print("=" * 60)
                return False

            # Confirm before proceeding
            print()
            confirm = input("  Proceed with update? [Y/n]: ").strip().lower()
            if confirm == 'n':
                print("\n  Update cancelled.")
                return False

            return self._do_encrypted_update(sfu)

        except FileNotFoundError:
            print("\n" + "=" * 60)
            print("  CANNOT START UPDATE")
            print("=" * 60)
            print("  Firmware file not found.")
            print("=" * 60)
            return False
        except Exception as e:
            print("\n" + "=" * 60)
            print("  FIRMWARE UPDATE FAILED")
            print("=" * 60)
            print(f"  Unexpected error: {e}")
            print("=" * 60)
            return False

    def _do_encrypted_update(self, sfu):
        """Handle encrypted firmware update (v3.0+ bootloaders)"""
        restart_count = 0

        while restart_count <= MAX_RESTART_ATTEMPTS:
            if restart_count > 0:
                print(f"\n  === RESTARTING (attempt {restart_count + 1}/{MAX_RESTART_ATTEMPTS + 1}) ===")

            result = self.do_encrypted_transfer(sfu)

            if result == 'success':
                print("\n" + "=" * 60)
                print("  FIRMWARE UPDATE SUCCESSFUL")
                print("=" * 60)
                print("  Signature verified. Device is rebooting.")
                print("=" * 60)
                return True

            elif result == 'likely_success':
                # USB disconnected after ENC_END - verify device is not in bootloader mode
                print(" signature valid")
                print("  Verifying update...", end='', flush=True)

                # Close current connection
                if self.ser:
                    try:
                        self.ser.close()
                    except:
                        pass
                    self.ser = None

                # Wait for device to fully reboot
                time.sleep(3.0)

                # Check if bootloader is still running (would mean failure)
                bootloader_found = False
                for attempt in range(3):
                    if self.connect(verbose=False):
                        try:
                            packet = struct.pack('<III', PACKET_TYPE_STATUS, 0, 0)
                            packet += struct.pack('<I', 0)
                            packet += b'\xFF' * PACKET_DATA_SIZE
                            self.ser.reset_input_buffer()
                            self.ser.write(packet)
                            self.ser.flush()
                            self.ser.timeout = 1.0
                            response = self.ser.read(STATUS_RESPONSE_SIZE)
                            if len(response) >= 8 and response[0] == 0xAA:
                                bootloader_found = True
                                break
                        except:
                            pass
                        finally:
                            if self.ser:
                                try:
                                    self.ser.close()
                                except:
                                    pass
                                self.ser = None
                    time.sleep(1.0)

                if bootloader_found:
                    print(" failed")
                    print("\n" + "=" * 60)
                    print("  FIRMWARE UPDATE FAILED")
                    print("=" * 60)
                    print("  Device is still in bootloader mode.")
                    print("  Signature verification may have failed.")
                    print("=" * 60)
                    return False
                else:
                    print(" verified")
                    print("\n" + "=" * 60)
                    print("  FIRMWARE UPDATE SUCCESSFUL")
                    print("=" * 60)
                    print("  Device is running new firmware.")
                    print("=" * 60)
                    return True

            elif result == 'disconnect':
                print(f"\n  USB disconnected during transfer")
                if not self.wait_for_reconnect():
                    return False
                restart_count += 1

            elif result == 'signature_failed':
                print("\n" + "=" * 60)
                print("  FIRMWARE UPDATE FAILED")
                print("=" * 60)
                print("  Signature verification failed.")
                print("  Possible causes:")
                print("    - Corrupted .sfu file")
                print("    - Wrong encryption keys")
                print("=" * 60)
                return 'signature_failed'

            elif result == 'usb_error':
                print("\n" + "=" * 60)
                print("  FIRMWARE UPDATE FAILED")
                print("=" * 60)
                print("  USB disconnected during signature verification.")
                print("  Please power cycle the device and try again.")
                print("=" * 60)
                return 'usb_error'

            elif result == 'hash_failed':
                print("\n" + "=" * 60)
                print("  FIRMWARE UPDATE FAILED")
                print("=" * 60)
                print("  Hash computation failed on device.")
                print("  Please report this issue.")
                print("=" * 60)
                return 'hash_failed'

            else:
                print("\n" + "=" * 60)
                print("  FIRMWARE UPDATE FAILED")
                print("=" * 60)
                print(f"  Transfer error: {result}")
                print("=" * 60)
                return False

        print("\n" + "=" * 60)
        print("  FIRMWARE UPDATE FAILED")
        print("=" * 60)
        print(f"  Too many restart attempts ({restart_count}).")
        print("=" * 60)
        return False


def print_header():
    print()
    print("=" * 60)
    print("    LESHUFFLER FIRMWARE UPDATER v3.0")
    print("=" * 60)


def print_usb_tips():
    print("\n" + "-" * 60)
    print("  USB CONNECTION TIPS:")
    print("-" * 60)
    print("  - Use a USB hub between computer and device")
    print("  - Use a USB 2.0 port if available")
    print("  - Keep the cable still during transfer")
    print("-" * 60)


def main():
    print_header()

    firmware_file = None
    selected_port = None

    # Parse arguments
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        arg = args[i]

        if arg in ['--help', '-h']:
            print("\nUsage:")
            print("  LeShuffler_Updater              Interactive mode")
            print("  LeShuffler_Updater <port>       Direct mode")
            print("  LeShuffler_Updater --list       List ports")
            print("  LeShuffler_Updater --file X.sfu Use specific file")
            print("\nNote: Requires bootloader v3.0+ (encrypted updates only)")
            print("      For legacy bootloaders, use LeShuffler_Legacy_Updater")
            print("\nExamples:")
            print("  LeShuffler_Updater COM5")
            print("  LeShuffler_Updater --file firmware.sfu COM5")
            input("\n  Press Enter to exit...")
            sys.exit(0)

        elif arg == '--list':
            ports = list_serial_ports()
            print("\nAvailable serial ports:")
            if ports:
                for port in ports:
                    marker = " [LeShuffler]" if port['is_stm32'] else ""
                    print(f"  {port['device']}: {port['description']}{marker}")
            else:
                print("  No serial ports found")
            input("\n  Press Enter to exit...")
            sys.exit(0)

        elif arg == '--file':
            if i + 1 < len(args):
                firmware_file = args[i + 1]
                i += 1
            else:
                print("\n  Usage: --file <filename>")
                sys.exit(1)

        elif not arg.startswith('-'):
            selected_port = arg

        i += 1

    # Interactive port selection if not specified
    if not selected_port:
        selected_port = select_port_interactive()

    if not selected_port:
        print("\n  No port selected. Exiting.")
        input("\n  Press Enter to exit...")
        sys.exit(0)

    # Get firmware path
    firmware_path = get_firmware_path(firmware_file)

    if not os.path.exists(firmware_path):
        print("\n" + "=" * 60)
        print("  CANNOT START UPDATE")
        print("=" * 60)
        print(f"  Firmware file not found: {firmware_path}")
        print("")
        print("  Place LeShuffler.sfu in the same folder as this updater.")
        print("=" * 60)
        input("\n  Press Enter to exit...")
        sys.exit(1)

    updater = EncryptedFirmwareUpdater(selected_port)

    print(f"\n  Connecting to {selected_port}...")
    connected = False

    for attempt in range(15):
        if updater.connect(verbose=(attempt == 0)):
            connected = True
            break

        if attempt == 0:
            print("\n  Device not ready. Waiting...")

        remaining = 14 - attempt
        print(f"\r  Waiting... {remaining}s remaining  ", end='', flush=True)
        time.sleep(1.0)

    print()

    if not connected:
        print("\n" + "=" * 60)
        print("  CANNOT START UPDATE")
        print("=" * 60)
        print("  Could not connect to device.")
        print("=" * 60)
        print_usb_tips()
        input("\n  Press Enter to exit...")
        sys.exit(1)

    try:
        result = updater.update_firmware(firmware_path)

        if result == True:
            input("\n  Press Enter to exit...")
            sys.exit(0)
        else:
            if result not in ['not_bootloader', 'unsupported_bootloader', 'signature_failed', 'hash_failed']:
                print("\n" + "=" * 60)
                print("  FIRMWARE UPDATE FAILED")
                print("=" * 60)
                print_usb_tips()
            input("\n  Press Enter to exit...")
            sys.exit(1)

    finally:
        updater.disconnect()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n  Cancelled by user.")
        input("\n  Press Enter to exit...")
        sys.exit(0)
    except Exception as e:
        print("\n" + "=" * 60)
        print("  UNEXPECTED ERROR")
        print("=" * 60)
        print(f"  {type(e).__name__}: {e}")
        print("=" * 60)
        input("\n  Press Enter to exit...")
        sys.exit(1)
