#!/usr/bin/env python3
"""
LeShuffler Firmware Updater v5
Auto-detects bootloader version and uses appropriate strategy:
- v2 bootloader: Robust protocol with state verification, safe restart
- v1 bootloader: Conservative approach with warnings

Changes from v4:
- Auto-detects bootloader version via STATUS packet
- v2: Can safely restart after disconnect (bootloader re-erases)
- v2: Verifies bootloader state after reconnect
- v1: Warns user about USB stability requirements

Usage:
    python3 firmware_updater_v5.py              # Interactive mode
    python3 firmware_updater_v5.py COM5         # Direct mode
    python3 firmware_updater_v5.py --list       # List ports
"""

import sys
import struct
import time
import binascii
import glob
import os
import platform

# Try to import serial with helpful error message
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

FIRMWARE_FILENAME = "LeShuffler.bin"
APPLICATION_ADDRESS = 0x08020000
MAX_FIRMWARE_SIZE = 896 * 1024

# Safety limits
# v2.1 with resume: unlimited retries (always makes progress)
# v2.0 restart from 0%: limited retries (could loop forever on unstable connection)
MAX_RESTART_ATTEMPTS = 10  # For v2.0 (restart from beginning)
# No limit for v2.1 resume - we always make progress

# ============================================================================
# Protocol constants
# ============================================================================

PACKET_TYPE_START = 0x01
PACKET_TYPE_DATA = 0x02
PACKET_TYPE_END = 0x03
PACKET_TYPE_STATUS = 0x04  # v2 only
PACKET_TYPE_RESUME = 0x05  # v2.1: Resume without erase
PACKET_DATA_SIZE = 256

# Timing
PACKET_DELAY_MS = 50  # Reduced for v2 (no HAL_Delay on device)
PACKET_DELAY_MS_V1 = 150  # Higher for v1 (has HAL_Delay)
MAX_RECONNECT_WAIT = 15

# Status response size (v2)
STATUS_RESPONSE_SIZE = 24


def get_executable_dir():
    if getattr(sys, 'frozen', False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))


def get_firmware_path():
    return os.path.join(get_executable_dir(), FIRMWARE_FILENAME)


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
        usbmodem_ports = glob.glob('/dev/tty.usbmodem*')
        existing_devices = [p['device'] for p in ports]
        for device in usbmodem_ports:
            if device not in existing_devices:
                ports.append({
                    'device': device,
                    'description': 'USB Modem',
                    'hwid': '',
                    'is_stm32': True
                })

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
        result = display_port_menu(ports)
        if result == 'REFRESH':
            print("\n  Scanning for ports...")
            time.sleep(0.5)
            continue
        return result


class FirmwareUpdater:
    def __init__(self, port, baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.is_windows = platform.system() == 'Windows'
        self.bootloader_version = None  # Will be set after detection
        self.is_v2 = False

    def find_device_port(self):
        ports = list(serial.tools.list_ports.comports())
        for p in ports:
            if p.device == self.port:
                return p.device
        if os.path.exists(self.port):
            return self.port
        if 'usbmodem' in self.port:
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

    def verify_bootloader_connection(self):
        """Verify we're actually talking to a bootloader, not the application"""
        print("  Verifying bootloader connection...", end='', flush=True)

        # Send a minimal STATUS packet and check for ANY valid bootloader response
        packet = struct.pack('<III', PACKET_TYPE_STATUS, 0, 0)
        packet += struct.pack('<I', 0)
        packet += b'\xFF' * PACKET_DATA_SIZE

        try:
            self.ser.reset_input_buffer()
            self.ser.write(packet)
            self.ser.flush()

            old_timeout = self.ser.timeout
            self.ser.timeout = 2.0
            response = self.ser.read(32)  # Read up to 32 bytes
            self.ser.timeout = old_timeout

            # Check for valid bootloader response (starts with 0xAA)
            if len(response) >= 8 and response[0] == 0xAA:
                # Valid bootloader response
                print(" OK")
                return True

            # No valid response - probably not in bootloader mode
            print(" NO RESPONSE")
            return False

        except Exception as e:
            print(f" Error: {e}")
            return False

    def detect_bootloader_version(self):
        """Send STATUS packet to detect bootloader version"""
        print("  Detecting bootloader version...", end='', flush=True)

        # Build STATUS packet
        packet = struct.pack('<III', PACKET_TYPE_STATUS, 0, 0)
        packet += struct.pack('<I', 0)  # CRC
        packet += b'\xFF' * PACKET_DATA_SIZE

        try:
            self.ser.reset_input_buffer()
            self.ser.write(packet)
            self.ser.flush()

            # Wait for response
            old_timeout = self.ser.timeout
            self.ser.timeout = 2.0
            response = self.ser.read(STATUS_RESPONSE_SIZE)
            self.ser.timeout = old_timeout

            # Check for v2 response (24 bytes with proper header/footer)
            if len(response) >= STATUS_RESPONSE_SIZE:
                if response[0] == 0xAA and response[1] == PACKET_TYPE_STATUS:
                    version_major = response[2]
                    version_minor = response[3]
                    self.bootloader_version = f"{version_major}.{version_minor}"
                    self.is_v2 = True
                    print(f" v{self.bootloader_version} (robust protocol)")
                    return True

            # Check for v1 response (8 bytes, might respond with error/unknown)
            if len(response) >= 8 and response[0] == 0xAA and response[7] == 0x55:
                self.bootloader_version = "1.x"
                self.is_v2 = False
                print(f" v{self.bootloader_version} (legacy protocol)")
                return True

            # No valid response - not a bootloader
            print(f" no valid response")
            return False

        except Exception as e:
            print(f" detection failed: {e}")
            return False

    def query_status(self):
        """Query bootloader status (v2 only). Returns dict or None."""
        if not self.is_v2:
            return None

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

            if len(response) >= STATUS_RESPONSE_SIZE and response[0] == 0xAA:
                return {
                    'version_major': response[2],
                    'version_minor': response[3],
                    'state': response[4],
                    'progress': response[5],
                    'bytes_received': struct.unpack('<I', response[8:12])[0],
                    'next_address': struct.unpack('<I', response[12:16])[0],
                    'flags': struct.unpack('<I', response[16:20])[0]
                }
            return None
        except:
            return None

    def usb_stability_check(self, num_tests=5, delay=1.0):
        """Test USB stability before starting transfer (for v1.0 safety).

        Sends multiple test packets to verify USB connection is stable
        BEFORE we erase flash. If any fail, abort early.

        Returns: True if stable, False if unstable
        """
        print(f"\n  Verifying USB stability ({num_tests} tests)...")

        # Build a simple test packet (use STATUS-like packet)
        packet = struct.pack('<III', PACKET_TYPE_STATUS, 0, 0)
        packet += struct.pack('<I', 0)
        packet += b'\xFF' * PACKET_DATA_SIZE

        for i in range(num_tests):
            try:
                print(f"    Test {i+1}/{num_tests}...", end='', flush=True)

                self.ser.reset_input_buffer()
                self.ser.write(packet)
                self.ser.flush()

                old_timeout = self.ser.timeout
                self.ser.timeout = 2.0
                response = self.ser.read(8)  # Read at least 8 bytes
                self.ser.timeout = old_timeout

                if len(response) >= 8 and response[0] == 0xAA:
                    print(" OK")
                else:
                    print(" FAILED (bad response)")
                    return False

                if i < num_tests - 1:
                    time.sleep(delay)

            except Exception as e:
                print(f" FAILED ({e})")
                return False

        print("  USB connection appears stable.")
        return True

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

    def is_connected(self):
        if not self.ser:
            return False
        try:
            return self.ser.is_open
        except:
            return False

    def disconnect(self):
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except:
                pass
            print("  Disconnected")

    def create_packet(self, packet_type, address, data):
        length = len(data)
        crc32 = binascii.crc32(data) & 0xFFFFFFFF
        data_padded = data + b'\xFF' * (PACKET_DATA_SIZE - length)
        packet = struct.pack('<III', packet_type, address, length)
        packet += struct.pack('<I', crc32)
        packet += data_padded
        return packet

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
                    'packet_type': response[1]
                }
            return None
        except:
            return None

    def send_start_packet(self, total_size):
        print(f"  Sending START packet (erasing flash)...")

        packet = struct.pack('<III', PACKET_TYPE_START, APPLICATION_ADDRESS, total_size)
        packet += struct.pack('<I', 0)
        packet += b'\xFF' * PACKET_DATA_SIZE

        try:
            self.ser.write(packet)
            self.ser.flush()
        except Exception as e:
            print(f"  Failed to send START: {e}")
            return False

        # Wait longer - flash erase takes time
        response = self.wait_for_response(timeout=15.0)
        if response and response['success']:
            print(f"  Flash erased, ready to receive firmware")
            return True
        else:
            print("  START packet failed")
            return False

    def send_resume_packet(self):
        """Send RESUME packet (v2.1) - continues transfer without erasing"""
        print(f"  Sending RESUME packet (no erase)...")

        packet = struct.pack('<III', PACKET_TYPE_RESUME, 0, 0)
        packet += struct.pack('<I', 0)
        packet += b'\xFF' * PACKET_DATA_SIZE

        try:
            self.ser.write(packet)
            self.ser.flush()
        except Exception as e:
            print(f"  Failed to send RESUME: {e}")
            return False

        response = self.wait_for_response(timeout=5.0)
        if response and response['success']:
            print(f"  Resume accepted, continuing transfer")
            return True
        else:
            print("  RESUME failed - will restart from beginning")
            return False

    def send_data_packet(self, address, data):
        packet = self.create_packet(PACKET_TYPE_DATA, address, data)

        try:
            if not self.is_connected():
                return (False, True)

            self.ser.write(packet)
            self.ser.flush()

            response = self.wait_for_response(timeout=10.0)
            if response and response['success']:
                return (True, False)
            elif response is None:
                return (False, True)  # Disconnect
            else:
                return (False, True)  # Retry as disconnect

        except (serial.SerialException, OSError):
            return (False, True)
        except Exception:
            return (False, True)

    def send_end_packet(self):
        print("\n  Finalizing update...")
        packet = self.create_packet(PACKET_TYPE_END, APPLICATION_ADDRESS, b'')
        try:
            self.ser.write(packet)
            self.ser.flush()
            self.wait_for_response(timeout=2.0)
        except:
            pass
        return True

    def do_transfer(self, firmware_data, start_offset=0, is_resume=False):
        """Perform transfer. Returns: 'success', 'disconnect', 'error'

        Args:
            firmware_data: Complete firmware bytes
            start_offset: Byte offset to start from (for resume)
            is_resume: If True, send RESUME instead of START
        """
        total_size = len(firmware_data)
        total_packets = (total_size + PACKET_DATA_SIZE - 1) // PACKET_DATA_SIZE

        # Select packet delay based on bootloader version
        packet_delay = PACKET_DELAY_MS if self.is_v2 else PACKET_DELAY_MS_V1

        if is_resume:
            # v2.1: Resume without erasing
            if not self.send_resume_packet():
                # Resume failed - fall back to full restart
                return 'restart_needed'
        else:
            # Fresh start with erase
            if not self.send_start_packet(total_size):
                return 'error'

        # Calculate starting packet number from offset
        start_packet = start_offset // PACKET_DATA_SIZE

        if start_offset > 0:
            print(f"  Resuming from packet {start_packet + 1}/{total_packets} ({start_offset} bytes already sent)")
        else:
            print(f"  Sending {total_packets} packets...")

        for packet_num in range(start_packet, total_packets):
            offset = packet_num * PACKET_DATA_SIZE
            address = APPLICATION_ADDRESS + offset
            chunk_size = min(PACKET_DATA_SIZE, total_size - offset)
            chunk = firmware_data[offset:offset + chunk_size]

            success, need_restart = self.send_data_packet(address, chunk)

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

            time.sleep(packet_delay / 1000.0)

        print()

        if not self.send_end_packet():
            return 'error'

        return 'success'

    def update_firmware(self, firmware_path):
        try:
            print(f"\n  Reading firmware: {os.path.basename(firmware_path)}")
            with open(firmware_path, 'rb') as f:
                firmware_data = f.read()

            total_size = len(firmware_data)
            print(f"  Firmware size: {total_size} bytes ({total_size/1024:.1f} KB)")

            if total_size > MAX_FIRMWARE_SIZE:
                print(f"  Error: Firmware too large")
                return False

            # Verify we're talking to a bootloader
            if not self.detect_bootloader_version():
                print("\n" + "=" * 60)
                print("  ERROR: Device is not in bootloader mode!")
                print("=" * 60)
                print("  The device appears to be running the application,")
                print("  not the bootloader.")
                print("")
                print("  To enter bootloader mode:")
                print("    1. On the device, go to Settings > Maintenance > Firmware Update")
                print("    2. Confirm to enter update mode")
                print("    3. Wait for 3 short beeps + 1 long beep (bootloader ready)")
                print("    4. Run this updater again")
                print("=" * 60)
                return 'not_bootloader'

            # v1 safety checks
            if not self.is_v2:
                # Option 1: Block macOS entirely for v1.0
                if platform.system() == 'Darwin':
                    print("\n" + "=" * 60)
                    print("  ERROR: Cannot update v1.0 bootloader on macOS")
                    print("=" * 60)
                    print("  macOS has known USB stability issues that can cause")
                    print("  disconnects during transfer. With v1.0 bootloader,")
                    print("  a disconnect mid-transfer requires ST-LINK to recover.")
                    print("")
                    print("  Please use a Windows PC for this update.")
                    print("=" * 60)
                    return 'v1_macos_blocked'

                # Option 3: Strong warning + confirmation on Windows
                print("\n" + "=" * 60)
                print("  WARNING: Legacy bootloader (v1.0) detected")
                print("=" * 60)
                print("  With v1.0 bootloader, if USB disconnects during transfer:")
                print("    - The device will need ST-LINK to recover")
                print("    - This cannot be fixed via USB alone")
                print("")
                print("  Make sure:")
                print("    - USB cable is secure and not moving")
                print("    - Using a USB hub (recommended)")
                print("    - No other USB activity during transfer")
                print("=" * 60)

                confirm = input("\n  Type 'yes' to proceed, anything else to abort: ").strip().lower()
                if confirm != 'yes':
                    print("\n  Update cancelled by user.")
                    return 'cancelled'

                # Option 2: USB stability pre-check
                if not self.usb_stability_check():
                    print("\n" + "=" * 60)
                    print("  ERROR: USB connection is unstable")
                    print("=" * 60)
                    print("  Aborting BEFORE flash erase to protect your device.")
                    print("  Try a different USB cable, port, or hub.")
                    print("=" * 60)
                    return 'unstable_usb'

                print("\n  Proceeding with v1.0 update...")

            # Transfer loop
            restart_count = 0  # Full restarts (v2.0 behavior)
            resume_count = 0   # Resumes (v2.1 behavior) - no limit since we always make progress
            resume_offset = 0  # For v2.1 resume capability
            use_resume = False
            # v2 can safely restart (re-erases flash), v1 cannot
            max_restarts = MAX_RESTART_ATTEMPTS if self.is_v2 else 0

            while True:
                if resume_count > 0 or restart_count > 0:
                    # Only v2 reaches here (v1 has max_restarts=0)
                    if use_resume and resume_offset > 0:
                        print(f"\n  === RESUMING (resume #{resume_count}) ===")
                        print(f"  v2.1: Resuming from {resume_offset} bytes")
                    else:
                        print(f"\n  === RESTARTING (attempt {restart_count + 1}/{max_restarts + 1}) ===")
                        print(f"  v2 bootloader: Safe restart with flash re-erase")

                result = self.do_transfer(firmware_data, start_offset=resume_offset, is_resume=use_resume)

                if result == 'success':
                    print("\n  Device is restarting with new firmware...")
                    return True

                elif result == 'restart_needed':
                    # Resume was rejected - do full restart
                    print("  Falling back to full restart...")
                    resume_offset = 0
                    use_resume = False
                    restart_count += 1
                    if restart_count > max_restarts:
                        print(f"\n  ERROR: Too many full restarts ({restart_count})")
                        return False
                    continue

                elif result == 'disconnect':
                    # v1: Fail immediately to prevent corruption
                    if not self.is_v2:
                        print(f"\n  " + "=" * 56)
                        print(f"  USB DISCONNECTED - UPDATE ABORTED")
                        print(f"  " + "=" * 56)
                        print(f"  To prevent flash corruption, no retry will be attempted.")
                        print(f"  ")
                        print(f"  REQUIRED: Power cycle the device (unplug and replug)")
                        print(f"  Then run this updater again.")
                        print(f"  " + "=" * 56)
                        return False

                    print(f"\n  USB disconnected during transfer")

                    if not self.wait_for_reconnect():
                        return False

                    # v2: Query state and try to resume
                    status = self.query_status()
                    if status:
                        bytes_received = status['bytes_received']
                        transfer_in_progress = (status['flags'] & 0x02) != 0

                        if transfer_in_progress and bytes_received > 0:
                            # v2.1: Can resume from where we left off - NO LIMIT (always makes progress)
                            resume_count += 1
                            print(f"  Bootloader has {bytes_received} bytes - attempting resume")
                            resume_offset = bytes_received
                            use_resume = True
                        else:
                            # No valid state to resume - restart from beginning
                            restart_count += 1
                            if restart_count > max_restarts:
                                print(f"\n  ERROR: Too many full restarts ({restart_count})")
                                return False
                            print(f"  No resumable state - restarting from beginning")
                            resume_offset = 0
                            use_resume = False
                    else:
                        # Couldn't query status - restart from beginning
                        restart_count += 1
                        if restart_count > max_restarts:
                            print(f"\n  ERROR: Too many full restarts ({restart_count})")
                            return False
                        resume_offset = 0
                        use_resume = False

                else:
                    print(f"\n  Transfer error")
                    return False

        except FileNotFoundError:
            print(f"  Error: Firmware file not found")
            return False
        except Exception as e:
            print(f"  Error: {e}")
            return False


def print_header():
    print()
    print("=" * 60)
    print("       LESHUFFLER FIRMWARE UPDATER v5.1")
    print("     (Auto-detect, resume on disconnect)")
    print("=" * 60)
    print(f"  Platform: {platform.system()} {platform.release()}")
    print(f"  Firmware: {FIRMWARE_FILENAME}")
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

    if len(sys.argv) > 1:
        arg = sys.argv[1]

        if arg in ['--help', '-h']:
            print("\nUsage:")
            print("  LeShuffler_Updater              Interactive mode")
            print("  LeShuffler_Updater <port>       Direct mode")
            print("  LeShuffler_Updater --list       List ports")
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

        else:
            selected_port = arg
            print(f"\n  Using port: {selected_port}")
    else:
        selected_port = select_port_interactive()

    if not selected_port:
        print("\n  No port selected. Exiting.")
        input("\n  Press Enter to exit...")
        sys.exit(0)

    firmware_path = get_firmware_path()
    print(f"\n  Firmware file: {firmware_path}")

    if not os.path.exists(firmware_path):
        print(f"\n  ERROR: Firmware file not found!")
        input("\n  Press Enter to exit...")
        sys.exit(1)

    updater = FirmwareUpdater(selected_port)

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
        print("\n  ERROR: Could not connect to device!")
        print_usb_tips()
        input("\n  Press Enter to exit...")
        sys.exit(1)

    try:
        result = updater.update_firmware(firmware_path)

        if result == True:
            print("\n" + "=" * 60)
            print("  SUCCESS! Firmware update complete.")
            print("=" * 60)
            input("\n  Press Enter to exit...")
            sys.exit(0)
        else:
            # Only show generic failure message for actual transfer failures
            if result not in ['not_bootloader', 'v1_macos_blocked', 'cancelled', 'unstable_usb']:
                print("\n  FAILED: Firmware update did not complete.")
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
