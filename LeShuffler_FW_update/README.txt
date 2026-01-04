LeShuffler Firmware Update
==========================

REQUIREMENTS:
- Windows PC (macOS not supported due to USB stability issues)
- LeShuffler device connected via USB

INSTRUCTIONS:

1. Connect the USB-C port of your LeShuffler to a USB port of your Windows PC.
   - Your LeShuffler should power up. If not, use a powered hub.
   - Make sure your USB connection is stable and uninterrupted - your device is 1.0, failure would require a more complex recovery.

2. On your LeShuffler device:
   - Go to the Settings menu and select "Firmware Update"
   - Wait for the 3 short beeps + 1 long beep (USB ready)

3. Run LeShuffler_Updater.exe
   - Select the COM port  flagged 'LeShuffler' when prompted
   - Wait for the update to complete (~30 seconds)

4. When complete, the device will automatically restart

TROUBLESHOOTING:

- "Access denied" error: Close any other programs using the COM port
- Device not detected: Unplug and reconnect the USB cable
- Update fails: Ensure LeShuffler.bin is in the same folder as the .exe

DO NOT:
- Disconnect the USB cable during update
- Close the updater window during update
- Turn off the device during update

If the update is interrupted:
- New devices (v2.1): Will automatically resume - just retry
- Older devices (v1.0): May require ST-LINK recovery

For support, contact: info@LeShuffler.com
