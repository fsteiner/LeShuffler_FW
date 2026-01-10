# LeShuffler Standalone ST-LINK Flasher

A lightweight tool for reflashing LeShuffler devices via ST-LINK without installing STM32CubeProgrammer.

## For End Users

### Requirements

1. **ST-LINK adapter** (V2 or V3)
2. **Windows PC**

### Download Package Contents

```
LeShuffler_Recovery/
├── LeShuffler_STLink_Flasher.exe    # The flasher tool
├── st-flash.exe                      # ST-LINK driver (included)
├── LeShuffler_Bootloader_E.bin       # Bootloader
├── LeShuffler.bin                    # Firmware
└── README.txt                        # Instructions
```

### Usage

1. Connect ST-LINK to your PC via USB
2. Connect ST-LINK to the device's SWD port (4 wires: VCC, GND, SWDIO, SWCLK)
3. Power on the device
4. Double-click `LeShuffler_STLink_Flasher.exe`
5. Follow the prompts

---

## For Developers (Building the Distribution)

### Step 1: Download stlink tools

Download from: https://github.com/stlink-org/stlink/releases

Get the Windows release (e.g., `stlink-x.x.x-win32.zip`) and extract `st-flash.exe`.

### Step 2: Build the executable

```powershell
cd Tools
pip install pyinstaller
python -m PyInstaller --onefile --name "LeShuffler_STLink_Flasher" stlink_standalone_flasher.py
```

Output: `dist/LeShuffler_STLink_Flasher.exe`

### Step 3: Create distribution package

```
LeShuffler_Recovery/
├── LeShuffler_STLink_Flasher.exe    # From dist/
├── st-flash.exe                      # From stlink tools download
├── LeShuffler_Bootloader_E.bin       # From Tools/
├── LeShuffler.bin                    # From Tools/
└── README.txt                        # User instructions (see below)
```

### Step 4: README.txt for end users

```
LeShuffler Recovery Tool
========================

This tool reflashes your LeShuffler device using an ST-LINK adapter.

REQUIREMENTS:
- ST-LINK V2 or V3 adapter
- 4 jumper wires (or SWD cable)

CONNECTIONS:
Connect ST-LINK to the device's programming header:
  ST-LINK    ->    Device
  --------         ------
  VCC        ->    3.3V
  GND        ->    GND
  SWDIO      ->    SWDIO
  SWCLK      ->    SWCLK

INSTRUCTIONS:
1. Connect ST-LINK to PC via USB
2. Connect ST-LINK to device (see above)
3. Power on the device
4. Run LeShuffler_STLink_Flasher.exe
5. Press 'y' to confirm when prompted
6. Wait for "COMPLETE" message
7. Disconnect ST-LINK and restart device

TROUBLESHOOTING:
- "No device detected": Check all 4 wire connections
- "st-flash not found": Make sure st-flash.exe is in the same folder
- Still not working: Contact support
```

---

## Alternative: Using stlink tools directly

If you prefer command-line:

```cmd
# Probe device
st-flash --probe

# Mass erase
st-flash erase

# Flash bootloader
st-flash write LeShuffler_Bootloader_E.bin 0x08000000

# Flash firmware
st-flash write LeShuffler.bin 0x08020000

# Reset device
st-flash reset
```

---

## Comparison with STM32CubeProgrammer

| Feature | This Tool | STM32CubeProgrammer |
|---------|-----------|---------------------|
| Download size | ~10 MB | ~500 MB |
| Installation | None (portable) | Required |
| GUI | Simple prompts | Full GUI |
| RDP support | No | Yes |
| End-user friendly | Yes | No |

This tool is designed for **recovery scenarios** where the user just needs to reflash. For development and RDP configuration, use STM32CubeProgrammer.
