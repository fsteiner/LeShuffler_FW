# LeShuffler Firmware

Firmware for the LeShuffler automatic card shuffling device, built on the STM32H733VGT6 (ARM Cortex-M7).

## Project Structure

```
LeShuffler/
├── Core/                    # Application firmware source
│   ├── Inc/                 # Headers (interface.h, definitions.h, version.h, etc.)
│   └── Src/                 # Source files
├── Bootloader/              # Legacy bootloader (v2.1, unencrypted)
├── Bootloader_E/            # Encrypted bootloader (v3.0+)
│   ├── Core/Inc/crypto_keys.h.template  # Key template (copy to crypto_keys.h)
│   └── ...
├── Tools/                   # Python utilities
│   ├── firmware_updater_encrypted.py    # USB firmware updater
│   ├── encrypt_firmware.py              # Create encrypted .sfu files
│   └── stlink_flasher.py                # ST-LINK factory flasher
├── Drivers/                 # STM32 HAL drivers
├── LCD/                     # Display drivers (ILI9488)
├── Motors/                  # Motor control (TMC2209, DC, Servo)
└── USB_DEVICE/              # USB CDC for firmware updates
```

## Memory Layout

| Region | Address | Size | Contents |
|--------|---------|------|----------|
| Bootloader | 0x08000000 | 48 KB | Bootloader_E (encrypted support) |
| Reserved | 0x0800C000 | 80 KB | Future use |
| Application | 0x08020000 | ~900 KB | Main firmware |

## Building

**Requirements:** STM32CubeIDE 1.13+

1. Import project: File → Import → Existing Projects into Workspace
2. Select the `LeShuffler` folder
3. Build configurations:
   - **LeShuffler** (application) → produces `LeShuffler.bin`
   - **Bootloader_E** (encrypted bootloader) → produces `Bootloader_E.bin`

## Firmware Update System

### Overview

The device supports two update methods:
- **USB Update** - End users can update via USB without opening the device
- **ST-LINK** - Factory programming and recovery

### Bootloader Versions

| Version | Encryption | File Types | Notes |
|---------|------------|------------|-------|
| v1.x | No | .bin only | Original shipped version |
| v2.x | No | .bin only | Added resume on disconnect |
| v3.0+ | Yes | .sfu or .bin | AES-256-CBC + ECDSA-P256 |

### USB Update Process

1. On device: Settings → Maintenance → Firmware Update
2. Wait for 3 short beeps + 1 long beep (bootloader ready)
3. On PC: Run `python Tools/firmware_updater_encrypted.py`
4. Select COM port and wait for transfer to complete

The updater auto-detects bootloader version and selects the appropriate file:
- v3.0+ bootloader: Uses `.sfu` (encrypted)
- v1.x/v2.x bootloader: Falls back to `.bin` (legacy)

## Tools

### firmware_updater_encrypted.py

USB firmware updater supporting both encrypted and legacy transfers.

```bash
python firmware_updater_encrypted.py              # Interactive
python firmware_updater_encrypted.py COM5         # Direct
python firmware_updater_encrypted.py --list       # List ports
```

**Required files** (in Tools folder):
- `LeShuffler.sfu` - Encrypted firmware (for v3.0+ bootloaders)
- `LeShuffler.bin` - Plain firmware (fallback for legacy bootloaders)

### encrypt_firmware.py

Creates encrypted `.sfu` files from plain `.bin` files.

```bash
# With test keys
python encrypt_firmware.py LeShuffler.bin LeShuffler.sfu --keys test_keys.json

# Generate new production keys
python encrypt_firmware.py --generate-keys /secure/path/production_keys.json
```

### stlink_flasher.py

Factory programming via ST-LINK. Flashes both bootloader and firmware.

```bash
python stlink_flasher.py                # Flash both
python stlink_flasher.py --rdp 1        # Flash + enable read protection
python stlink_flasher.py --firmware-only  # Firmware only
```

**Required files** (in Tools folder):
- `Bootloader_E.bin` → 0x08000000
- `LeShuffler.bin` → 0x08020000

**Requires:** [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html)

### stlink_standalone_flasher.py (Windows)

Lightweight ST-LINK flasher for end-user recovery. No STM32CubeProgrammer installation required.

Uses [stlink tools](https://github.com/stlink-org/stlink/releases) (`st-flash.exe`) instead.

```bash
python stlink_standalone_flasher.py              # Flash both
python stlink_standalone_flasher.py --firmware   # Firmware only
python stlink_standalone_flasher.py --erase      # Mass erase only
```

**Distribution package for end users** (~10 MB):
```
LeShuffler_Recovery/
├── LeShuffler_STLink_Flasher.exe    # Built with PyInstaller
├── st-flash.exe                      # From stlink-org/stlink
├── LeShuffler_Bootloader_E.bin
├── LeShuffler.bin
└── README.txt
```

See `Tools/STANDALONE_FLASHER_README.md` for build instructions.

### remote_recovery_flasher.py (Remote Support)

**Why not distribute the bootloader binary?** The bootloader contains the AES-256 key. Anyone with the bootloader could decrypt .sfu files and extract the firmware.

**Solution:** Remote support session (TeamViewer/AnyDesk) where you control the flashing.

#### Build the remote flasher (one-time)

```powershell
cd Tools
# Ensure LeShuffler_Bootloader_E.bin is present
python build_remote_flasher.py
# Output: dist/LeShuffler_Remote_Recovery.exe (~8 MB)
```

#### Remote recovery workflow

1. Client downloads [TeamViewer QuickSupport](https://www.teamviewer.com/en/download/windows/) (portable, no install)
2. Client connects ST-LINK to device's SWD port
3. Client shares TeamViewer session ID with you
4. You connect remotely and transfer `LeShuffler_Remote_Recovery.exe`
5. You run the exe and type `FLASH` to confirm
6. Tool flashes bootloader + sets RDP Level 1
7. **Tool securely deletes itself** (3-pass overwrite)
8. Client disconnects ST-LINK and does normal USB firmware update with .sfu file

**Requirements on client machine:**
- OpenOCD installed OR `openocd.exe` in same folder
- ST-LINK V2 or V3 adapter

**Security features:**
- Bootloader binary embedded in exe (base64), extracted to temp folder
- 3-pass random data overwrite before deletion (unrecoverable)
- Exe deletes itself after completion

Download OpenOCD for Windows: https://github.com/openocd-org/openocd/releases

### legacy_usb_updater.py (Self-Erasing USB Update)

For legacy devices (v1.x/v2.x bootloader) without RDP protection. Prevents casual copying of firmware .bin files.

#### Build

```powershell
cd Tools
python build_legacy_updater.py
# Output: dist/LeShuffler_Legacy_Updater.exe
```

#### Usage

1. User puts device in bootloader mode (Settings > Maintenance > Firmware Update)
2. Wait for 3 beeps + 1 long beep
3. Run `LeShuffler_Legacy_Updater.exe`
4. Type `yes` to confirm
5. After successful update, exe securely deletes itself

#### Protection level

| Attack Vector | Protected? |
|--------------|-----------|
| Copy .bin file from distribution | ✅ Yes - embedded & deleted |
| Extract from exe memory | ⚠️ Harder but possible |
| Read flash via ST-LINK | ❌ No - requires RDP Level 1 |

**Note:** This is "soft protection" - prevents casual copying but not determined attackers with physical access + debugger.

## Production Workflow

### 1. Development (RDP Level 0)

- Full debug access via ST-LINK
- Use test keys for encryption testing
- Iterate on firmware development

### 2. Generate Production Keys

```bash
# Generate keys to a temporary location
python Tools/encrypt_firmware.py --generate-keys /tmp/production_keys.json

# IMMEDIATELY save to 1Password, then delete local copy
# Keys should ONLY exist in 1Password - never on filesystem
rm /tmp/production_keys.json
```

### 3. Build with Production Keys

1. Open 1Password → find `production_keys.json`
2. Copy `aes_key` array → paste into `AES_KEY[32]` in `crypto_keys.h`
3. Copy `ecdsa_public_key` array → paste into `ECDSA_PUBLIC_KEY[64]`
4. Rebuild Bootloader_E project
5. **Revert crypto_keys.h to placeholders after build**

### 4. Factory Flash (RDP Level 1)

```bash
python Tools/stlink_flasher.py --rdp 1
```

This:
- Flashes bootloader with production keys
- Flashes application firmware
- Enables read protection (flash cannot be read, but can be erased and reflashed)

## Versioning

### Firmware Version

Defined in `Core/Inc/version.h`:
```c
#define FW_VERSION_MAJOR  1
#define FW_VERSION_MINOR  0
#define FW_VERSION_PATCH  1
```

Displayed as "v1.0.1" in Settings → About.

### Bootloader Version

Stored at fixed address `0x0800BFF0`. Application can read via `GetBootloaderVersion()`.

## Security

### Key Management

| Key | Location | Purpose |
|-----|----------|---------|
| AES-256 | `crypto_keys.h` (bootloader flash) | Decrypt firmware |
| ECDSA Private | **1Password only** | Sign firmware |
| ECDSA Public | `crypto_keys.h` (bootloader flash) | Verify signature |

**Production keys** (`production_keys.json`) are stored **only in 1Password** - never on filesystem.

**Never commit:**
- `crypto_keys.h` (with real keys)
- `*_keys.json` (key files)
- `*.pem` (exported keys)

### Read Protection (RDP)

| Level | Read Flash | Reflash | Revert |
|-------|------------|---------|--------|
| 0 | Yes | Yes | N/A |
| 1 | No | Yes (mass erase) | Yes |
| 2 | No | No | No (permanent) |

Production devices use **RDP Level 1**.

## Development History

See `SESSION_LOG.md` for the full development history of the encrypted bootloader system (21 sessions covering bootloader versions, crypto implementation, debugging, and production setup).

## License

Proprietary - All rights reserved.
