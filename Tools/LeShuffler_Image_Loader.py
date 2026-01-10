#!/usr/bin/env python3
"""
LeShuffler Image Loader (CLI)
Uploads C header image files to device external flash.

Usage:
    python image_loader.py              # Upload all .h files from C_headers/
    python image_loader.py --erase      # Erase external flash only
    python image_loader.py --list       # List available ports
    python image_loader.py COM5         # Use specific port

The script looks for a "C_headers" folder in the same directory.
"""

import os
import sys
import re
import glob
import time
import platform

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("\nERROR: pyserial not installed")
    print("Run: pip install pyserial")
    input("\nPress Enter to exit...")
    sys.exit(1)

# Protocol constants
CHUNK_SIZE = 2043
BAUD_RATE = 115200
TIMEOUT = 10

# Packet types
PACKET_DATA = 0x01
PACKET_END_FILE = 0x04
PACKET_ERASE = 0x43


def get_script_dir():
    """Get directory containing this script (works for exe too)"""
    if getattr(sys, 'frozen', False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))


def list_serial_ports():
    """List available serial ports, mark STM32 devices"""
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

    # Mac: prefer cu.* over tty.* and add usbmodem devices
    if platform.system() == 'Darwin':
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
        # Filter out tty.* duplicates
        cu_ports = set(p['device'] for p in ports if '/dev/cu.' in p['device'])
        ports = [p for p in ports if not (
            '/dev/tty.' in p['device'] and
            p['device'].replace('/dev/tty.', '/dev/cu.') in cu_ports
        )]

    ports.sort(key=lambda p: (not p['is_stm32'], p['device']))
    return ports


def select_port():
    """Auto-select port if one LeShuffler device, otherwise prompt"""
    ports = list_serial_ports()
    leshuffler_ports = [p for p in ports if p['is_stm32']]

    if len(leshuffler_ports) == 1:
        port = leshuffler_ports[0]
        print(f"\n  Auto-selected: {port['device']}")
        print(f"                 {port['description']} [LeShuffler]")
        return port['device']

    if not ports:
        print("\n  No serial ports found!")
        return None

    print("\n  Available ports:")
    for i, port in enumerate(ports, 1):
        marker = " [LeShuffler]" if port['is_stm32'] else ""
        print(f"    [{i}] {port['device']} - {port['description']}{marker}")

    print(f"\n    [R] Refresh")
    print(f"    [Q] Quit")

    while True:
        choice = input("\n  Select port: ").strip().upper()
        if choice == 'Q':
            return None
        elif choice == 'R':
            return select_port()  # Recurse to refresh
        elif choice.isdigit():
            num = int(choice)
            if 1 <= num <= len(ports):
                return ports[num - 1]['device']
        print(f"  Invalid. Enter 1-{len(ports)}, R, or Q")


def natural_sort_key(s):
    """Sort strings with embedded numbers naturally"""
    return [int(text) if text.isdigit() else text.lower()
            for text in re.split(r'(\d+)', s)]


def parse_h_file(filepath):
    """Parse a C header file and extract hex array data"""
    try:
        with open(filepath, 'r') as f:
            content = f.read()
        start = content.index("0x")
        end = content.rindex("0x")
        data_str = content[start:end + 4]  # Include last 0xNN
        data_str = data_str.replace(" ", "").replace("\n", "")
        hex_values = data_str.split(",")
        return [int(b, 16) for b in hex_values if b.startswith("0x")]
    except Exception as e:
        print(f"  ERROR parsing {os.path.basename(filepath)}: {e}")
        return None


def calc_crc16(data):
    """Calculate CRC-16 CCITT"""
    crc = 0xFFFF
    poly = 0x1021
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ poly
            else:
                crc = crc << 1
            crc &= 0xFFFF
    return crc


def send_chunk(ser, data):
    """Send a data chunk with header and CRC, wait for ACK"""
    size = len(data)
    size_bytes = [(size >> 8) & 0xFF, size & 0xFF]
    crc = calc_crc16(data)
    crc_bytes = [(crc >> 8) & 0xFF, crc & 0xFF]

    # Pad to chunk size if needed
    padded_data = list(data) + [0x00] * (CHUNK_SIZE - size) if size < CHUNK_SIZE else list(data)

    packet = [PACKET_DATA] + size_bytes + padded_data + crc_bytes
    ser.write(bytes(packet))

    # Wait for ACK
    response = ser.read(1)
    if response == b'A':
        return True
    elif response == b'N':
        return False  # NAK - retry
    else:
        return False  # Timeout or unknown


def send_file_data(ser, data, filename, progress_callback=None):
    """Send file data in chunks"""
    total = len(data)
    sent = 0
    retries = 0
    max_retries = 5

    while sent < total:
        end = min(sent + CHUNK_SIZE, total)
        chunk = data[sent:end]

        if send_chunk(ser, chunk):
            sent = end
            retries = 0
            if progress_callback:
                progress_callback(sent, total)
        else:
            retries += 1
            if retries >= max_retries:
                print(f"\n  ERROR: Max retries reached for {filename}")
                return False

    return True


def send_end_file(ser):
    """Send end-of-file signal and get address response"""
    packet = [PACKET_END_FILE] * 2048
    ser.write(bytes(packet))
    response = ser.read(8)
    if len(response) == 8:
        start_addr = (response[0] << 24) | (response[1] << 16) | (response[2] << 8) | response[3]
        end_addr = ((response[4] << 24) | (response[5] << 16) | (response[6] << 8) | response[7]) - 1
        return start_addr, end_addr
    return None, None


def erase_flash(ser):
    """Erase external flash memory"""
    packet = [PACKET_ERASE] * 2048
    ser.write(bytes(packet))
    print("  Erasing... wait for buzzer on device")


def print_progress(current, total, width=40):
    """Print progress bar"""
    percent = current / total
    filled = int(width * percent)
    bar = '=' * filled + '-' * (width - filled)
    sys.stdout.write(f'\r  [{bar}] {percent*100:.1f}%')
    sys.stdout.flush()


def upload_folder(port, folder_path):
    """Upload all .h files from folder"""
    # Get list of .h files
    h_files = [f for f in os.listdir(folder_path) if f.endswith('.h')]
    h_files.sort(key=natural_sort_key)

    if not h_files:
        print(f"\n  No .h files found in {folder_path}")
        return False

    print(f"\n  Found {len(h_files)} files to upload")

    # Calculate total size
    print("  Calculating total size...")
    all_data = []
    for filename in h_files:
        filepath = os.path.join(folder_path, filename)
        data = parse_h_file(filepath)
        if data is None:
            return False
        all_data.append((filename, data))

    total_bytes = sum(len(d) for _, d in all_data)
    print(f"  Total size: {total_bytes:,} bytes")

    # Connect to device
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=TIMEOUT)
    except Exception as e:
        print(f"\n  ERROR: Could not open {port}: {e}")
        return False

    print(f"\n  Uploading to {port}...")
    uploaded_bytes = 0

    try:
        for filename, data in all_data:
            file_size = len(data)

            def progress(sent, total):
                nonlocal uploaded_bytes
                print_progress(uploaded_bytes + sent, total_bytes)

            if not send_file_data(ser, data, filename, progress):
                ser.close()
                return False

            uploaded_bytes += file_size

            # Send end-of-file
            start_addr, end_addr = send_end_file(ser)
            print(f"\r  {filename}: OK" + " " * 30)

    except Exception as e:
        print(f"\n  ERROR: {e}")
        ser.close()
        return False

    ser.close()
    print(f"\n  Upload complete! ({total_bytes:,} bytes)")
    return True


def main():
    print("\n" + "=" * 50)
    print("  LeShuffler Image Loader")
    print("=" * 50)

    # Parse arguments
    args = sys.argv[1:]
    specified_port = None
    erase_only = False
    list_only = False

    for arg in args:
        if arg == '--erase':
            erase_only = True
        elif arg == '--list':
            list_only = True
        elif arg in ['--help', '-h']:
            print(__doc__)
            input("\nPress Enter to exit...")
            return 0
        elif not arg.startswith('-'):
            specified_port = arg

    # List ports only
    if list_only:
        ports = list_serial_ports()
        print("\n  Available ports:")
        for port in ports:
            marker = " [LeShuffler]" if port['is_stm32'] else ""
            print(f"    {port['device']}: {port['description']}{marker}")
        input("\nPress Enter to exit...")
        return 0

    # Select port
    if specified_port:
        port = specified_port
        print(f"\n  Using port: {port}")
    else:
        port = select_port()
        if not port:
            input("\nPress Enter to exit...")
            return 1

    # Erase only mode
    if erase_only:
        print("\n  WARNING: This will erase ALL images from external flash!")
        confirm = input("  Type 'yes' to confirm: ").strip().lower()
        if confirm != 'yes':
            print("  Aborted.")
            input("\nPress Enter to exit...")
            return 0

        try:
            ser = serial.Serial(port, BAUD_RATE, timeout=TIMEOUT)
            erase_flash(ser)
            ser.close()
        except Exception as e:
            print(f"\n  ERROR: {e}")
        input("\nPress Enter to exit...")
        return 0

    # Find C_headers folder
    script_dir = get_script_dir()
    headers_folder = os.path.join(script_dir, "C_headers")

    if not os.path.isdir(headers_folder):
        print(f"\n  ERROR: C_headers folder not found!")
        print(f"  Expected: {headers_folder}")
        input("\nPress Enter to exit...")
        return 1

    print(f"\n  Source folder: {headers_folder}")

    # Upload
    success = upload_folder(port, headers_folder)

    if success:
        print("\n" + "=" * 50)
        print("  SUCCESS!")
        print("=" * 50)
    else:
        print("\n" + "=" * 50)
        print("  UPLOAD FAILED")
        print("=" * 50)

    input("\nPress Enter to exit...")
    return 0 if success else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n\n  Aborted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n  ERROR: {e}")
        input("\nPress Enter to exit...")
        sys.exit(1)
