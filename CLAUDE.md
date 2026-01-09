# CLAUDE.md - Quick Reference for Claude

## Project Overview

LeShuffler: Automatic card shuffler firmware for STM32H733VGT6.
- **Current version**: v1.0.1 (tagged)
- **Bootloader**: v3.0 (encrypted, AES-256-CBC + ECDSA-P256)
- **Status**: Production ready, RDP Level 1 verified

## Project Structure

```
LeShuffler/
├── Core/                  # Application firmware
├── Bootloader_E/          # Encrypted bootloader (v3.0) - ACTIVE
├── Tools/
│   ├── firmware_updater.py           # USB updater (encrypted + legacy)
│   ├── encrypt_firmware.py           # Creates .sfu files
│   ├── stlink_flasher.py             # Factory ST-LINK flasher (requires STM32CubeProg)
│   ├── stlink_standalone_flasher.py  # Standalone flasher (Windows, uses st-flash)
│   ├── remote_recovery_flasher.py    # Remote support flasher (self-deleting, ST-LINK)
│   ├── build_remote_flasher.py       # Build script for remote flasher
│   ├── legacy_usb_updater.py         # Self-erasing USB updater for legacy devices
│   ├── build_legacy_updater.py       # Build script for legacy updater
│   ├── LeShuffler.bin                # Plain firmware
│   ├── LeShuffler.sfu                # Encrypted firmware
│   └── LeShuffler_Bootloader_E.bin
├── Legacy/
│   ├── Bootloader/              # Non-encrypted bootloader (old devices)
│   └── Tools/
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

### crypto_keys.h Security
- **Location**: `Bootloader_E/Core/Inc/crypto_keys.h`
- **Contains**: Placeholders (zeros) - NOT production keys
- **Gitignored**: Yes
- **Dropbox-ignored**: Yes (`xattr -w com.dropbox.ignored 1`)
- **Production keys**: **1Password only** (not on filesystem)

**To rebuild Bootloader_E**:
1. Open 1Password → find `production_keys.json`
2. Copy `aes_key` → paste into `AES_KEY[32]` in `crypto_keys.h`
3. Copy `ecdsa_public_key` → paste into `ECDSA_PUBLIC_KEY[64]`
4. Build in STM32CubeIDE
5. Copy binary: `cp Bootloader_E/Debug/*.bin Tools/`
6. **Revert crypto_keys.h to placeholders**

**To create .sfu files**:
1. Export `production_keys.json` from 1Password to `/tmp/`
2. Run: `python3 Tools/encrypt_firmware.py Tools/LeShuffler.bin Tools/LeShuffler.sfu --keys /tmp/production_keys.json`
3. Delete: `rm /tmp/production_keys.json`

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
- **Factory command**: `python stlink_flasher.py --rdp 1 -y` then power cycle

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

### Encrypted Devices (v3.0 bootloader)
`.sfu` files are encrypted+signed - **safe to distribute openly**.

**Distribution package:**
```
LeShuffler_Update/
├── LeShuffler_Updater.exe     # Built from firmware_updater.py
└── LeShuffler.sfu             # Encrypted firmware
```

### Legacy Devices (v1.x/v2.x bootloader)
`.bin` files are unprotected - use self-erasing updater.

**Distribution:** Single `LeShuffler_Legacy_Updater.exe` (embeds .bin, self-deletes)

**Auto-RDP1:** Application firmware now auto-sets RDP Level 1 on first boot:
- `Core/Src/rdp_protection.c` - checks RDP level, sets RDP1 if needed
- Runs once, idempotent (if RDP1 already set, does nothing)
- After update, device is protected from debugger flash read

## Building Windows Executables

**Prerequisites:**
```powershell
pip install pyinstaller pyserial
```

### Encrypted Updater (for v3.0 bootloader)
```powershell
cd Tools
python -m PyInstaller --onefile --name "LeShuffler_Updater" --collect-all serial --clean firmware_updater.py
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

## Reference Files

When updating documentation, update all three "reference files":
- `CLAUDE.md` - Quick reference for Claude
- `README.md` - Project documentation
- `SESSION_LOG.md` - Development history

## Session History

See `SESSION_LOG.md` for full development history (22 sessions).
