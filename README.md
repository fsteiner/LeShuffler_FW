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
│   └── LeShuffler_ST-Link_Flasher.py    # ST-LINK factory flasher
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

### Bootloader v1.0 Limitation (devices in field)

v1.0 erases flash immediately when user enters update mode (before USB connects).
This is **inherent to v1.0 bootloader** - cannot be fixed by firmware update alone.

| Scenario | What Happens | Bricked? |
|----------|--------------|----------|
| User enters update mode, then cancels | Flash erased but empty (0xFF) → bootloader detects invalid app, stays in bootloader mode | **No** - can retry |
| USB cable issue, no connection | Same - empty flash detected as invalid | **No** - can retry |
| USB disconnect mid-transfer | Partial firmware written with valid-looking header → bootloader jumps to corrupt code | **Yes** - ST-LINK required |

**v3.0 bootloader fix:** Flash not erased until START packet received (connection confirmed). User can power cycle and old firmware still works.

### USB Update Process

1. On device: Settings → Maintenance → Firmware Update
2. Wait for 3 short beeps + 1 long beep (bootloader ready)
3. On PC: Run `python Tools/firmware_updater.py`
4. Select COM port and wait for transfer to complete

**Updaters by bootloader version:**
- v3.0+ bootloader: Use `firmware_updater.py` with `.sfu` file
- v1.x/v2.x bootloader: Use `legacy_usb_updater.py` (self-erasing exe)

### Service Mode Shortcut (v1.0.2+)

For field updates when USB power is insufficient for motor initialization:

1. Hold **both ENTER and ESC buttons** while powering on the device
2. Keep holding for **2 seconds**
3. Screen shows "SERVICE MODE" and 2 short beeps confirm
4. Device enters bootloader mode directly (bypasses motor init)
5. Run firmware updater as normal

This allows firmware updates on low-power USB ports (laptops) that can't power the stepper motor driver.

## Tools

### firmware_updater.py

USB firmware updater for encrypted .sfu files (requires v3.0+ bootloader).

```bash
python firmware_updater.py              # Interactive
python firmware_updater.py COM5         # Direct
python firmware_updater.py --list       # List ports
```

**Required files** (in Tools folder):
- `LeShuffler.sfu` - Encrypted firmware

For legacy bootloaders (v1.x/v2.x), use `legacy_usb_updater.py` instead.

### encrypt_firmware.py

Creates encrypted `.sfu` files from plain `.bin` files.

```bash
# With test keys
python encrypt_firmware.py LeShuffler.bin LeShuffler.sfu --keys test_keys.json

# Generate new production keys
python encrypt_firmware.py --generate-keys /secure/path/production_keys.json
```

### LeShuffler_ST-Link_Flasher.py

Factory programming via ST-LINK. Flashes both bootloader and firmware.

```bash
python LeShuffler_ST-Link_Flasher.py                # Flash both
python LeShuffler_ST-Link_Flasher.py --rdp 1        # Flash + enable read protection
python LeShuffler_ST-Link_Flasher.py --firmware-only  # Firmware only
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

#### Auto-RDP1 Protection

The application firmware now auto-sets RDP Level 1 on first boot after update:
- Checks current RDP level at startup
- If RDP0 → sets RDP1 and triggers reset
- If already RDP1 → continues normal boot
- Runs once, idempotent

This means legacy devices get full RDP1 protection after their first firmware update with the new firmware.

#### Protection level (after update)

| Attack Vector | Protected? |
|--------------|-----------|
| Copy .bin file from distribution | ✅ Yes - embedded & deleted |
| Extract from exe memory | ⚠️ Harder but possible |
| Read flash via ST-LINK | ✅ Yes - RDP1 auto-set after update |

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

### 3. Build with Production Keys (rare - only when rebuilding bootloader)

1. Open 1Password → find `production_keys.json`
2. Copy `aes_key` array → paste into `AES_KEY[32]` in `crypto_keys.h`
3. Copy `ecdsa_public_key` array → paste into `ECDSA_PUBLIC_KEY[64]`
4. Rebuild Bootloader_E project
5. Copy binary to secure location: `cp Bootloader_E/Debug/LeShuffler_Bootloader_E.bin ~/.leshuffler_keys/`
6. **Revert crypto_keys.h to placeholders after build**

### 4. Create Encrypted Firmware (.sfu)

```bash
# Export production_keys.json from 1Password to /tmp/production_keys.json
python Tools/encrypt_firmware.py Tools/LeShuffler.bin Tools/LeShuffler.sfu \
    --keys /tmp/production_keys.json
rm /tmp/production_keys.json  # Delete immediately after use
```

### 5. Factory Flash (RDP Level 1)

LeShuffler_ST-Link_Flasher.py looks for bootloader at `~/.leshuffler_keys/LeShuffler_Bootloader_E.bin` first.

```bash
python Tools/LeShuffler_ST-Link_Flasher.py --rdp 1
```

This:
- Flashes bootloader from `~/.leshuffler_keys/` (contains embedded AES key)
- Flashes application firmware from `Tools/LeShuffler.bin`
- Enables read protection (flash cannot be read, but can be erased and reflashed)

## Building Windows Executables

For distributing update tools to end users without Python.

**Prerequisites:**
```powershell
pip install pyinstaller pyserial
```

### Encrypted Updater (for v3.0 bootloader devices)

```powershell
cd Tools
python -m PyInstaller --onefile --name "LeShuffler_Updater" --collect-all serial --clean firmware_updater.py
```

**Output:** `dist/LeShuffler_Updater.exe`

**Distribution package:**
```
LeShuffler_Update/
├── LeShuffler_Updater.exe
└── LeShuffler.sfu
```

### Legacy Updater (for v1.x/v2.x bootloader devices)

```powershell
cd Tools
python build_legacy_updater.py
```

**Output:** `dist/LeShuffler_Legacy_Updater.exe`

**Distribution:** Single exe only (firmware embedded, self-deleting, auto-sets RDP1)

### Remote Recovery Flasher (for ST-LINK recovery)

```powershell
cd Tools
python build_remote_flasher.py
```

**Output:** `dist/LeShuffler_Remote_Recovery.exe`

**Do not distribute** - for remote support sessions only.

### ST-LINK Factory Flasher

For factory programming with RDP1 protection:

```powershell
cd Tools
python -m PyInstaller --onefile --name "LeShuffler_ST-Link_Flasher" --clean LeShuffler_ST-Link_Flasher.py
```

**Output:** `dist/LeShuffler_ST-Link_Flasher.exe`

**Usage:**
```
LeShuffler_ST-Link_Flasher.exe --rdp 1 -y    # Factory flash with RDP1, no prompts
LeShuffler_ST-Link_Flasher.exe --firmware-only  # Update firmware only
LeShuffler_ST-Link_Flasher.exe --help        # Show all options
```

### Image Loader (Manufacturing)

For uploading images to device external flash during manufacturing.

**Source:** `Tools/LeShuffler_Image_Loader.py`
**Executable:** `Manufacturing/APIC/Test_and_production_firmware/LeShuffler_Image_Loader.exe`

```powershell
cd Tools
python -m PyInstaller --onefile --name "LeShuffler_Image_Loader" --collect-all serial --clean LeShuffler_Image_Loader.py
# Copy to manufacturing folder:
cp dist/LeShuffler_Image_Loader.exe ../../Manufacturing/APIC/Test_and_production_firmware/
```

**Manufacturing folder contents:**
```
Test_and_production_firmware/
├── LeShuffler_Image_Loader.exe      # Image uploader
├── LeShuffler_ST-Link_Flasher.exe   # Factory flasher
├── LeShuffler.bin
├── LeShuffler_Bootloader_E.bin
└── C_headers/                        # Image header files
```

## Versioning

### Firmware Version

Defined in `Core/Inc/version.h`:
```c
#define FW_VERSION_MAJOR  1
#define FW_VERSION_MINOR  0
#define FW_VERSION_PATCH  2
```

Displayed as "v1.0.2" in Settings → About.

### Bootloader Version

Stored at fixed address `0x0800BFF0`. Application can read via `GetBootloaderVersion()`.

## Security

### Key Storage Locations

| Location | Contents | Notes |
|----------|----------|-------|
| **1Password** | `production_keys.json` | **ONLY permanent location** for all keys |
| `~/.leshuffler_keys/` | `LeShuffler_Bootloader_E.bin` | Bootloader binary with embedded AES + ECDSA public |
| `crypto_keys.h` | Placeholders (zeros) | Gitignored - real keys never on filesystem |
| `Tools/test_keys.json` | Test keys only | Don't match production bootloader |

### Key Types

| Key | Purpose | Risk if Leaked |
|-----|---------|----------------|
| AES-256 | Decrypt .sfu firmware | Firmware can be decrypted |
| ECDSA Private | Sign firmware | **CRITICAL** - attacker can sign malicious FW |
| ECDSA Public | Verify signature | Safe (embedded in bootloader) |

**Never commit:**
- `crypto_keys.h` (with real keys)
- `*_keys.json` (key files)
- `*.pem` (exported keys)
- `LeShuffler_Bootloader_E.bin` (contains embedded AES key)

### Read Protection (RDP)

| Level | Read Flash | Reflash | Revert |
|-------|------------|---------|--------|
| 0 | Yes | Yes | N/A |
| 1 | No | Yes (mass erase) | Yes |
| 2 | No | No | No (permanent) |

Production devices use **RDP Level 1**.

### Firmware Distribution Summary

| Device Type | Updater | What to Distribute | Protection |
|-------------|---------|-------------------|------------|
| v3.0+ (encrypted) | `LeShuffler_Updater.exe` | exe + `.sfu` file | AES-256 encryption + RDP1 |
| v1.x/v2.x (legacy) | `LeShuffler_Legacy_Updater.exe` | exe only | Auto-RDP1 after update |

### Reverse Engineering Risk Assessment

| Attack Vector | Encrypted (v3.0) | Legacy (after v1.0.2 update) | Legacy (before update) |
|---------------|------------------|------------------------------|------------------------|
| Intercept update file | ❌ AES-256 encrypted | N/A | N/A |
| Copy .bin file | N/A | ❌ Embedded + deleted | ⚠️ Vulnerable if distributed |
| Read flash via ST-LINK | ❌ RDP1 blocks | ❌ RDP1 auto-set | ⚠️ RDP0 allows read |
| Extract from exe memory | N/A | ⚠️ Difficult but possible | N/A |
| Hardware attack (chip decap) | ⚠️ Very expensive | ⚠️ Very expensive | ⚠️ Very expensive |

**Summary:**
- **Encrypted devices (v3.0+)**: Fully protected by encryption and RDP1
- **Legacy devices after update**: Protected by RDP1 (auto-set by firmware v1.0.2+)
- **Legacy devices before update**: Vulnerable until updated to v1.0.2+

## Development History

See `SESSION_LOG.md` for the full development history of the encrypted bootloader system (24 sessions covering bootloader versions, crypto implementation, debugging, and production setup).

## License

Proprietary - All rights reserved.
