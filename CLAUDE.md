# CLAUDE.md - Quick Reference for Claude

## Project Overview

LeShuffler: Automatic card shuffler firmware for STM32H733VGT6.
- **Current version**: v1.0.2
- **Bootloader**: v3.0 (encrypted, AES-256-CBC + ECDSA-P256)
- **Status**: Production ready, RDP Level 1 verified

## Project Structure

```
LeShuffler/
├── Core/                  # Application firmware
├── Bootloader_E/          # Encrypted bootloader (v3.0) - ACTIVE
├── Tools/
│   ├── LeShuffler_Updater.py         # USB updater (encrypted .sfu)
│   ├── LeShuffler_ST-Link_Flasher.py # ST-LINK factory flasher
│   ├── LeShuffler_Image_Loader.py    # Image uploader
│   ├── encrypt_firmware.py           # Creates .sfu files
│   ├── LeShuffler.bin                # Plain firmware
│   └── LeShuffler.sfu                # Encrypted firmware
├── Legacy/
│   ├── Bootloader/                   # Non-encrypted bootloader (v1.x/v2.x)
│   └── Tools/                        # Legacy/rare tools (remote recovery, standalone flasher, etc.)
└── README.md              # Full project documentation
```

## Working Practices

### File/Folder Operations
- **ALWAYS verify** operations with `ls`, `dir`, etc. before confirming to user
- Windows commands can fail silently - never assume success

### Tools Binaries - Check Before Use
```bash
# Compare timestamps
ls -la Tools/*.bin Debug/*.bin Bootloader_E/Debug/*.bin

# If Debug is newer, update Tools:
cp Debug/LeShuffler.bin Tools/
# Export production_keys.json from 1Password to /tmp/, then:
python3 Tools/encrypt_firmware.py Tools/LeShuffler.bin Tools/LeShuffler.sfu --keys /tmp/production_keys.json
rm /tmp/production_keys.json  # Delete after use
```
**CRITICAL**: `.sfu` must be regenerated using **production keys** from 1Password

### Crypto Keys Locations

| Location | Contents | Notes |
|----------|----------|-------|
| **1Password** | `production_keys.json` | **ONLY permanent location** for keys |
| `~/.leshuffler_keys/` | `LeShuffler_Bootloader_E.bin` | Bootloader with embedded AES + ECDSA public |
| `Bootloader_E/Core/Inc/crypto_keys.h` | Placeholders (zeros) | Gitignored, safe |
| `Tools/test_keys.json` | Test keys | Don't match production bootloader |

**Key types in production_keys.json:**
- `aes_key` - AES-256 symmetric key (decrypt firmware)
- `ecdsa_private_key` - Signs firmware (MOST SENSITIVE - allows signing malicious FW)
- `ecdsa_public_key` - Verifies signature (embedded in bootloader, safe to expose)

### Workflow: Create Encrypted Firmware (.sfu)

```bash
# 1. Export production_keys.json from 1Password to temp location
#    (Copy JSON content to /tmp/production_keys.json)

# 2. Encrypt firmware
python3 Tools/encrypt_firmware.py Tools/LeShuffler.bin Tools/LeShuffler.sfu \
    --keys /tmp/production_keys.json

# 3. DELETE keys immediately
rm /tmp/production_keys.json
```

### Workflow: Rebuild Bootloader_E (rare)

1. Open 1Password → find `production_keys.json`
2. Copy `aes_key` array → paste into `AES_KEY[32]` in `crypto_keys.h`
3. Copy `ecdsa_public_key` array → paste into `ECDSA_PUBLIC_KEY[64]`
4. Build in STM32CubeIDE
5. Copy binary: `cp Bootloader_E/Debug/LeShuffler_Bootloader_E.bin ~/.leshuffler_keys/`
6. **Revert crypto_keys.h to placeholders (all zeros)**

### Factory Programming (LeShuffler_ST-Link_Flasher.py)

LeShuffler_ST-Link_Flasher.py looks for bootloader in this order:
1. `~/.leshuffler_keys/LeShuffler_Bootloader_E.bin` (secure, outside Dropbox)
2. Same folder as script (fallback)

```bash
python LeShuffler_ST-Link_Flasher.py --rdp 1 -y  # Flash + set RDP Level 1
# Then power cycle the device
```

### Factory Image Loader

**Source:** `Tools/LeShuffler_Image_Loader.py`
**Executable:** `Manufacturing/APIC/Test_and_production_firmware/LeShuffler_Image_Loader.exe`

Uploads C header image files to device external flash. CLI version (no GUI).

```bash
LeShuffler_Image_Loader.exe              # Upload all .h files from C_headers/
LeShuffler_Image_Loader.exe --erase      # Erase external flash only
LeShuffler_Image_Loader.exe --list       # List available ports
LeShuffler_Image_Loader.exe COM5         # Use specific port
```

- Auto-selects port when one LeShuffler device found
- Looks for `C_headers/` folder in same directory as exe

### STM32_Programmer_CLI

**Mac:**
```bash
alias stm32prog='"/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI"'
```

**Windows (PowerShell):**
```powershell
& "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" -c port=SWD -ob displ
```

### RDP Level 1 Notes
- **Mac ST-LINK drivers don't work with RDP Level 1** - use Windows
- **CLI works for**: Flashing, setting RDP 0→1, mass erase at RDP 0
- **GUI required for**: Mass erase at RDP 1, RDP 1→0 recovery
- **Recovery**: GUI → OB tab → Set RDP to 0xAA → Apply (triggers mass erase)
- **Factory command**: `python LeShuffler_ST-Link_Flasher.py --rdp 1 -y` then power cycle

### Git Safety
- **NEVER** `git checkout <file>` to undo formatting - reverts ALL changes
- **Use** `git stash` first if needed: `git stash` → fix → `git stash pop`
- **Verify** after amending: `git show --stat` or `git diff HEAD~1`
- **Check** before revert/checkout: `git diff <file>`

## STM32H7 HAL Notes

### HASH Peripheral
- **NO `Instance` member** in `HASH_HandleTypeDef`
- Just set `hhash.Init.DataType = HASH_DATATYPE_8B;` and call `HAL_HASH_Init(&hhash)`

### CRYP Peripheral
- **HAS `Instance` member**: `hcryp.Instance = CRYP;`
- **Key/IV format**: Must be big-endian 32-bit words (use `bytes_to_be32()`)

```c
// CRYP - needs Instance + big-endian key/IV
hcryp.Instance = CRYP;
hcryp.Init.Algorithm = CRYP_AES_CBC;
hcryp.Init.pKey = key_words;      // Big-endian formatted
hcryp.Init.pInitVect = iv_words;  // Big-endian formatted
HAL_CRYP_Init(&hcryp);

// HASH - NO Instance member
hhash.Init.DataType = HASH_DATATYPE_8B;
HAL_HASH_Init(&hhash);
```

## STM32CubeMX Warning

**DO NOT regenerate code in `Bootloader_E/`** - manual modifications will be lost:
- `stm32h7xx_hal_conf.h` - HAL module enables
- `cryp.c/h`, `hash.c/h`, `rng.c/h` - manually created

## Remote Recovery (for bricked devices)

**Why not distribute bootloader .bin?** It contains the AES key - anyone could decrypt .sfu files.

**Solution:** Remote support session where you control the flashing.

**Build the remote flasher (one-time, on your Windows PC):**
```powershell
cd Tools
python build_remote_flasher.py
# Output: dist/LeShuffler_Remote_Recovery.exe
```

**Remote recovery flow:**
1. Client downloads TeamViewer QuickSupport (portable, no install)
2. Client connects ST-LINK to device, shares session ID with you
3. You connect, transfer exe, run it (type "FLASH" to confirm)
4. Bootloader flashed + RDP1 set + **exe securely self-deletes**
5. Client runs USB updater with .sfu file

**Security:** Bootloader embedded in exe, extracted to temp, 3-pass overwrite before deletion.

## Firmware Distribution

| Device Type | Updater | Distribute | Protection |
|-------------|---------|------------|------------|
| v3.0+ (encrypted) | `LeShuffler_Updater.exe` | exe + `.sfu` | AES-256 + RDP1 |
| v1.x/v2.x (legacy) | `LeShuffler_Legacy_Updater.exe` | exe only | Auto-RDP1 after update |

### Encrypted Devices (v3.0 bootloader)
`.sfu` files are encrypted+signed - **safe to distribute openly**.

### Legacy Devices (v1.x/v2.x bootloader)
`.bin` embedded in exe, self-deletes after use. Firmware auto-sets RDP1 on first boot.

### Bootloader v1.0 Limitation (devices in field)

v1.0 erases flash immediately when user enters update mode (before USB connects).
This is **inherent to v1.0 bootloader** - cannot be fixed by firmware update.

| Scenario | What Happens | Bricked? |
|----------|--------------|----------|
| User enters update mode, then cancels | Flash erased but empty (0xFF) → bootloader detects invalid app, stays in bootloader mode | **No** - can retry |
| USB cable issue, no connection | Same - empty flash detected as invalid | **No** - can retry |
| USB disconnect mid-transfer | Partial firmware written with valid-looking header → bootloader jumps to corrupt code | **Yes** - ST-LINK required |

**v3.0 bootloader fix:** Flash not erased until START packet received (connection confirmed). User can power cycle and old firmware still works.

## Reverse Engineering Risk Summary

| Attack Vector | Encrypted (v3.0) | Legacy (after update) | Legacy (before update) |
|---------------|------------------|----------------------|------------------------|
| Intercept .sfu file | ❌ AES-256 encrypted | N/A | N/A |
| Copy .bin file | N/A | ❌ Embedded + deleted | ⚠️ If distributed |
| Read flash via ST-LINK | ❌ RDP1 protected | ❌ RDP1 auto-set | ⚠️ RDP0 vulnerable |
| Extract from exe memory | N/A | ⚠️ Possible but difficult | N/A |
| Hardware attack (decap) | ⚠️ Expensive/difficult | ⚠️ Expensive/difficult | ⚠️ Expensive/difficult |

**Bottom line:**
- **Encrypted devices**: Fully protected (encryption + RDP1)
- **Legacy devices after update**: Protected by RDP1 (firmware sets it automatically)
- **Legacy devices before update**: Vulnerable until they receive the v1.0.2+ update

## Building Windows Executables

**Prerequisites:**
```powershell
pip install pyinstaller pyserial
```

### Encrypted Updater (for v3.0 bootloader)
```powershell
cd Tools
python -m PyInstaller --onefile --name "LeShuffler_Updater" --collect-all serial --clean LeShuffler_Updater.py
# Output: dist/LeShuffler_Updater.exe
```
Distribute with `LeShuffler.sfu` file.

### Legacy Updater (for v1.x/v2.x bootloader, self-erasing)
```powershell
cd Tools
python build_legacy_updater.py
# Output: dist/LeShuffler_Legacy_Updater.exe
```
Distribute exe only (firmware embedded, auto-sets RDP1 after update).

### Remote Recovery Flasher (ST-LINK, self-erasing)
```powershell
cd Tools
python build_remote_flasher.py
# Output: dist/LeShuffler_Remote_Recovery.exe
```
**Do not distribute** - for remote support sessions only.

### ST-LINK Factory Flasher
```powershell
cd Tools
python -m PyInstaller --onefile --name "LeShuffler_ST-Link_Flasher" --clean LeShuffler_ST-Link_Flasher.py
# Output: dist/LeShuffler_ST-Link_Flasher.exe
```
Usage: `LeShuffler_ST-Link_Flasher.exe --rdp 1 -y` (factory flash with RDP1, no prompts)

### Image Loader (Manufacturing)
Source in `Tools/`, exe goes to `Manufacturing/APIC/Test_and_production_firmware/`:
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
├── LeShuffler_ST-Link_Flasher.exe   # ST-LINK factory flasher
├── LeShuffler_Updater.exe           # USB firmware updater
├── LeShuffler.bin                   # Plain firmware (for ST-LINK)
├── LeShuffler.sfu                   # Encrypted firmware (for USB update)
├── LeShuffler_Bootloader_E.bin      # Bootloader with embedded keys
└── C_headers/                       # Image header files
```

## Reference Files

When updating documentation, update all three "reference files":
- `CLAUDE.md` - Quick reference for Claude
- `README.md` - Project documentation
- `SESSION_LOG.md` - Development history

## Session History

See `SESSION_LOG.md` for full development history (24 sessions).
