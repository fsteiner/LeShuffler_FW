LeShuffler Firmware Updater for macOS
=====================================

REQUIREMENTS:
- macOS 10.13 or later
- LeShuffler device in bootloader mode

IMPORTANT - macOS USB LIMITATIONS:
- macOS has known USB stability issues with some devices
- v1.0 bootloader: NOT SUPPORTED on macOS (blocked by updater)
- v2.1 bootloader: Fully supported with auto-resume on disconnect

FILES IN THIS FOLDER:
- LeShuffler_Updater     : The updater application
- LeShuffler.bin         : Firmware file (you must add this)
- README.txt             : This file

INSTRUCTIONS:

1. Put your LeShuffler in bootloader mode:
   - On device: Settings > Maintenance > Firmware Update
   - Confirm to enter update mode
   - Wait for 3 short beeps + 1 long beep (bootloader ready)

2. Place "LeShuffler.bin" firmware file in this folder
   (same folder as LeShuffler_Updater)

3. Open Terminal and navigate to this folder:
   cd /path/to/LeShuffler_FW_update_Mac

4. Make the updater executable (first time only):
   chmod +x LeShuffler_Updater

5. Run the updater:
   ./LeShuffler_Updater

6. Select the correct serial port (usually /dev/tty.usbmodem...)

7. Wait for the update to complete

TROUBLESHOOTING:

"Permission denied" error:
  Run: chmod +x LeShuffler_Updater

"Cannot be opened because the developer cannot be verified":
  Right-click > Open, then click "Open" in the dialog
  OR: System Preferences > Security & Privacy > Allow

Device not found:
  - Check USB cable connection
  - Try a different USB port
  - Make sure device is in bootloader mode (beeps heard)
  - Try using a USB hub

If update disconnects mid-transfer:
  - v2.1 bootloader: Will automatically resume - just wait
  - v1.0 bootloader: Not supported on macOS

SUPPORT:
For assistance, contact LeShuffler support.
