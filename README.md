# OpenBadge

ESP32-S3 Bluetooth Hands-Free headset firmware for the GlassBridge AI Assistant.

## Overview

OpenBadge emulates a standard Bluetooth headset using HFP (Hands-Free Profile) and AVRCP (Audio/Video Remote Control Profile). It serves as the hardware interface for the GlassBridge Android app, enabling voice-based AI interactions.

**Target Hardware:** M5Stack CoreS3

## Features

- Bluetooth Classic HFP 1.7 Client
- Wideband Speech (mSBC) support for 16kHz audio
- AVRCP Controller for media button triggers
- Split-screen display with status and scrolling log
- Touch-to-trigger interface

## Screen Layout

The CoreS3 display is divided into two sections:

```
┌─────────────────────────────────┐
│      STATUS DISPLAY             │  Top 100px
│   [Blue] "Tap to Speak"         │  Color-coded status
│          OpenBadge              │
├─────────────────────────────────┤
│ OpenBadge v1.0                  │  Bottom 140px
│ Initializing...                 │  Scrolling log text
│ Speaker: 16000 Hz mono          │  (cyan on black)
│ BT controller OK                │
│ HFP Client OK                   │
│ Ready to pair!                  │
└─────────────────────────────────┘
```

**Touch Zones:**
- **STATUS section (top)** - Tap here to trigger AI
- **LOG section (bottom)** - Display only, shows debug output

## Quick Start

### 1. Pair the Device

1. Power on the M5Stack CoreS3
2. Screen shows gray "Not Connected" with initialization logs below
3. On Android: **Settings → Bluetooth → Pair new device**
4. Select **"OpenBadge"** from the list
5. Accept pairing on both devices
6. Screen turns blue "Tap to Speak"

### 2. Use with GlassBridge

1. Open the GlassBridge app on Android
2. Ensure OpenBadge is shown as connected headset
3. **Tap the top STATUS section** to activate the AI
4. Screen turns red "Listening..." - SCO audio session active
5. Speak your query, then hear the AI response through the speaker
6. When session ends, screen returns to blue "Tap to Speak"

---

## Development Setup

### Prerequisites

1. **PlatformIO** - Install via VS Code extension or CLI
2. **USB-C Cable** - For programming the CoreS3
3. **CP2104 Driver** - May be needed on Windows ([Download](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers))

### Install PlatformIO CLI (if not using VS Code)

```bash
# Using pip
pip install platformio

# Or using Homebrew (macOS)
brew install platformio
```

---

## Building and Flashing

### Option 1: VS Code with PlatformIO Extension (Recommended)

1. Install [VS Code](https://code.visualstudio.com/)
2. Install **PlatformIO IDE** extension from marketplace
3. Open this folder in VS Code
4. Click **PlatformIO icon** in sidebar (alien head)
5. Under **PROJECT TASKS → m5stack-cores3**:
   - Click **Build** to compile
   - Click **Upload** to flash
   - Click **Monitor** to view serial output

### Option 2: Command Line

```bash
# Navigate to project directory
cd /path/to/openbadge

# Build firmware
pio run

# Upload to device (connect CoreS3 via USB-C first)
pio run -t upload

# Monitor serial output (115200 baud)
pio device monitor
```

### Option 3: Build + Upload + Monitor in One Command

```bash
pio run -t upload && pio device monitor
```

---

## Connecting M5Stack CoreS3 for Flashing

### Step 1: Connect USB-C Cable

1. Use the **USB-C port on the bottom** of the CoreS3 (not the side port)
2. Connect to your computer
3. The device may power on automatically

### Step 2: Enter Download Mode (if upload fails)

If automatic upload doesn't work:

1. **Hold the reset button** (small button on the side)
2. **Press and hold the power button** for 2 seconds
3. **Release reset button first**, then power button
4. Device is now in download mode
5. Run `pio run -t upload` again

### Step 3: Verify Connection

```bash
# List connected serial ports
pio device list

# You should see something like:
# /dev/ttyUSB0 or /dev/ttyACM0 (Linux)
# /dev/cu.usbserial-* (macOS)
# COM3 or COM4 (Windows)
```

### Troubleshooting Upload Issues

| Problem | Solution |
|---------|----------|
| "No serial port found" | Install CP2104 driver, try different USB cable |
| "Permission denied" (Linux) | `sudo usermod -a -G dialout $USER` then logout/login |
| Upload timeout | Enter download mode manually (see above) |
| Wrong port selected | Set `upload_port = /dev/ttyXXX` in platformio.ini |

---

## On-Screen Log Output

When running, the bottom section shows scrolling log messages:

```
OpenBadge v1.0
Initializing...
Speaker: 16000 Hz mono
Mic: 16000 Hz mono
Hardware ready
==== Bluetooth Init ====
NVS init...
NVS OK
BT controller init...
BT controller OK
Bluedroid init...
Bluedroid OK
Device: OpenBadge
HFP Client init...
HFP Client OK
AVRCP init...
AVRCP OK
Setting discoverable...
Discoverable!
==== BT Ready ====
Ready to pair!
Scan for 'OpenBadge'
```

When connecting:
```
[HFP] Connecting...
[HFP] Connected XX:YY
[HFP] SLC Ready
Status: Tap to Speak
```

When triggering (tap STATUS section):
```
>>> Touch triggered!
Status: Listening...
Sending AVRCP Play...
AVRCP sent
```

When SCO audio connects:
```
[SCO] Connecting...
[SCO] mSBC 16kHz
```

If sample rate changes (e.g., 8kHz → 16kHz):
```
Sample rate: 8000 -> 16000 Hz
```

---

## Verification Tests

### Test 1: Split Screen Test
**Expected:** Top shows colored status, bottom shows cyan log text
**Verifies:** Display layout, M5Unified, power management

### Test 2: Discovery Test
**Expected:** "OpenBadge" appears in Android Bluetooth scan
**Verifies:** Bluetooth stack initialization

### Test 3: Touch Zone Test
**Expected:** Tap top section → triggers action, tap bottom → no trigger
**Verifies:** Touch zone detection

### Test 4: Trigger Test
**Expected:** Tap status section → GlassBridge app activates
**Verifies:** AVRCP passthrough commands

### Test 5: Voice Test
**Expected:** Tap → Red screen → Speak → Hear response → Blue screen when done
**Verifies:** SCO audio, sample rate negotiation, bidirectional audio

---

## Project Structure

```
openbadge/
├── platformio.ini          # Build configuration
├── sdkconfig.defaults      # ESP-IDF Bluetooth settings
├── README.md               # This file
├── docs/
│   └── ARCHITECTURE.md     # Detailed design documentation
└── src/
    ├── main.cpp            # Entry point
    ├── Core/
    │   ├── BluetoothManager.h
    │   └── BluetoothManager.cpp
    └── HAL/
        ├── IBoard.h        # Hardware interface (includes log())
        ├── BoardManager.h  # Board factory
        ├── Board_M5CoreS3.h
        └── Board_M5CoreS3.cpp
```

---

## Status Colors (Top Section)

| Color | State | Meaning |
|-------|-------|---------|
| Gray | Disconnected | Not paired/connected to phone |
| Blue | Idle | Connected, ready - tap to speak |
| Red | Listening | SCO audio active (mic/speaker streaming) |
| Green | Speaking | Reserved for future use (TTS playback indicator) |

**Note:** The current implementation uses Red (Listening) for the entire SCO session since the badge cannot distinguish between recording and TTS playback. The phone handles that logic internally.

---

## Troubleshooting

### Log section not updating
- Serial output still works - connect via `pio device monitor`
- Check if display initialization succeeded

### Device not discoverable
- Check log for "Discoverable!" message
- Power cycle the CoreS3

### Audio sounds distorted (chipmunk/slow)
- Check log for "[SCO] CVSD 8kHz" vs "[SCO] mSBC 16kHz"
- Ensure `sdkconfig.defaults` has WBS enabled

### AVRCP commands not received by GlassBridge
- Check log for "AVRCP sent" message
- Try alternatives in main.cpp:
  - `sendHfpButton()` - Sends KEYCODE_HEADSETHOOK
  - `sendBvra()` - Sends AT+BVRA=1 (voice recognition activation)

### No speaker output
- Check log for "Speaker: XXXXX Hz mono" during init
- Verify `M5.Power.begin()` is called

---

## License

MIT License - See LICENSE file

## Related Projects

- [GlassBridge](../glassbridge) - Android AI Assistant middleware
