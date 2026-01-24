# M5StickC Plus2 Implementation Guide

This guide provides step-by-step instructions for implementing M5StickC Plus2 support in the OpenBadge firmware. All code templates are complete and ready to copy-paste.

## Overview

The M5StickC Plus2 migration adds a second board target while keeping the existing M5Stack Core S3 support intact. The implementation leverages the existing HAL (Hardware Abstraction Layer) architecture, requiring only:

1. New board implementation files (Board_M5StickCPlus2.h/cpp)
2. Factory update (BoardManager.h)
3. Build configuration (platformio.ini, partition table)
4. Documentation

**Estimated Time**: All code files have been created. Build and testing remain.

## Prerequisites

- PlatformIO CLI installed
- M5StickC Plus2 hardware
- USB-C cable
- GlassBridge Android app (for end-to-end testing)

## Implementation Steps

### Step 1: Board Implementation Files

The following files have already been created:

**Created Files:**
- `src/HAL/Board_M5StickCPlus2.h` - Header with class definition, constants, and private members
- `src/HAL/Board_M5StickCPlus2.cpp` - Full implementation of IBoard interface

**Key Implementation Details:**

**Portrait Layout (135x240)**:
- Status section: Top 80px (vs 100px on Core S3)
- Log section: Bottom 160px (vs 140px on Core S3)
- Font: FreeSansBold12pt7b for status (vs 18pt on Core S3)
- Text truncation: 22 chars per line (vs 38 chars)

**Button Detection** (replaces touch):
```cpp
// In update(): Must call M5.update() every loop
M5.update();

// In isActionTriggered(): Use built-in edge detection
bool triggered = M5.BtnA.wasPressed();
```
This uses M5Unified's built-in debouncing and edge detection (cleaner than manual state tracking).

**Power Management**:
```cpp
auto cfg = M5.config();
cfg.output_power = true;  // CRITICAL: Prevents 60-second auto-shutdown on battery
M5.begin(cfg);
```
Unlike Core S3, M5StickC Plus2 doesn't need explicit `M5.Power.begin()` call.

**Audio Configuration**:
- Speaker: PAM8303 via I2S on GPIO0
- Microphone: SPM1423 PDM via GPIO34/GPIO0
- M5Unified handles GPIO0 multiplexing internally
- Volume may need tuning (currently set to 200)

### Step 2: HAL Factory Update

**Updated File:** `src/HAL/BoardManager.h`

The factory has been updated to support both boards:

```cpp
#if defined(BOARD_M5_CORES3)
    #include "Board_M5CoreS3.h"
#elif defined(BOARD_M5_STICKC_PLUS2)
    #include "Board_M5StickCPlus2.h"
#else
    #error "No board defined!"
#endif

class BoardManager {
public:
    static IBoard* createBoard() {
#if defined(BOARD_M5_CORES3)
        static Board_M5CoreS3 board;
        return &board;
#elif defined(BOARD_M5_STICKC_PLUS2)
        static Board_M5StickCPlus2 board;
        return &board;
#endif
    }
};
```

### Step 3: Build Configuration

**Updated File:** `platformio.ini`

Added new environment:

```ini
[env:m5stickc-plus2]
platform = espressif32@6.9.0
board = esp32dev                     # Universal ESP32 board (M5Unified handles pin config)
framework = espidf, arduino
lib_deps = m5stack/M5Unified@^0.1.13
build_flags =
    -DBOARD_M5_STICKC_PLUS2          # Board selection flag
    # Bluetooth Classic flags (identical to Core S3)
    -DCONFIG_BT_ENABLED=1
    -DCONFIG_BT_CLASSIC_ENABLED=1
    -DCONFIG_BT_HFP_ENABLE=1
    # ... (all BT flags same as Core S3)
board_build.partitions = default_8MB.csv
board_build.esp-idf.sdkconfig_path = sdkconfig.defaults
```

**Created File:** `default_8MB.csv`

```csv
# Partition table for 8MB flash
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x300000,   # 3MB
app1,     app,  ota_1,   0x310000,0x300000,   # 3MB
spiffs,   data, spiffs,  0x610000,0x1E0000,   # 1.9MB
coredump, data, coredump,0x7F0000,0x10000,    # 64KB
```

### Step 4: Build and Upload

**Build for M5StickC Plus2:**
```bash
cd /mnt/c/dev4/openbadge
pio run -e m5stickc-plus2
```

**Expected Output:**
```
Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
RAM:   [=         ]  XX.X% (used XXXXX bytes from XXXXXX bytes)
Flash: [====      ]  XX.X% (used XXXXXXX bytes from 3145728 bytes)
```

**Upload to Device:**
```bash
pio run -e m5stickc-plus2 --target upload
```

**Monitor Serial Output:**
```bash
pio device monitor -e m5stickc-plus2
```

**Expected Boot Messages:**
```
OpenBadge v1.0
M5StickC Plus2
Speaker: 16000 Hz
Mic: 16000 Hz
Hardware ready
Press Button A
Bluetooth initialized
Advertising as: OpenBadge
```

### Step 5: Verification Tests

**Hardware Tests:**

1. **Display Test**
   - Device boots and shows portrait UI
   - Status section shows "Not Connected" (gray background)
   - Log section shows initialization messages
   - Text is readable (not upside down or cut off)

2. **Button Test**
   - Press Button A (top button on front)
   - Serial monitor shows: `>>> Button A pressed!`
   - Log section updates immediately

3. **Audio Output Test** (requires Bluetooth connection)
   - Pair with phone
   - Connect via GlassBridge
   - Speaker plays audio clearly

4. **Audio Input Test** (requires Bluetooth connection)
   - Microphone captures voice
   - Whisper transcription works

**Integration Tests:**

5. **Bluetooth Pairing**
   - Open phone Bluetooth settings
   - See "OpenBadge" in available devices
   - Pair successfully

6. **GlassBridge Connection**
   - Open GlassBridge app
   - Select OpenBadge as audio device
   - HFP profile connects
   - Status changes to "Tap to Speak" (blue)

7. **Voice Session**
   - Press Button A
   - Status changes to "Listening..." (red)
   - Speak into microphone
   - Status changes to "Speaking..." (green)
   - TTS audio plays through speaker
   - Status returns to "Tap to Speak" (blue)

**Regression Tests:**

8. **Core S3 Still Works**
   ```bash
   pio run -e m5stack-cores3
   ```
   Should build without errors.

### Step 6: Troubleshooting

**Build Errors:**

**Error: `Board m5stick-c not found`**
- **Fix**: The platformio.ini already uses `board = esp32dev` which is the universal fallback
- **Reason**: M5Unified handles all pin definitions, so we don't rely on PlatformIO's board-specific configs
- **Note**: `esp32dev` works for any ESP32-PICO chip including the Plus2's ESP32-PICO-V3-02

**Error: `Partition table does not fit`**
- **Fix**: Build size exceeds 3MB. Enable size optimization:
  ```ini
  build_flags = ... -Os
  ```
- **Or**: Reduce partition sizes to 2MB each in default_8MB.csv

**Error: `M5.Speaker not found`**
- **Fix**: Update M5Unified library to 0.1.13 or newer:
  ```bash
  pio lib update
  ```

**Runtime Issues:**

**Display is upside down**
- **Fix**: Change `M5.Display.setRotation(0)` to `M5.Display.setRotation(2)` in Board_M5StickCPlus2.cpp:45

**Text is cut off**
- **Fix**: Reduce font size further or decrease text length in drawStatusSection()
- **Current**: FreeSansBold12pt7b (can try Font2 or Font0)

**Button doesn't respond**
- **Check**: `M5.update()` is called in `update()` method (Board_M5StickCPlus2.cpp:66)
- **Check**: GPIO37 button A is physically working (test with M5StickC examples)

**Speaker silent**
- **Check**: Volume level (currently 200, try range 0-255)
- **Check**: `cfg.output_power = true` is set (Board_M5StickCPlus2.cpp:10)
- **Try**: `M5.Power.setExtOutput(true)` if needed (add after M5.begin())

**Device shuts down after 60 seconds on battery**
- **Critical**: Ensure `cfg.output_power = true` in M5.begin()
- **Verify**: Check serial output for unexpected resets
- **Fix**: May need `M5.Power.setExtOutput(true)` for AXP192 PMIC

**Bluetooth doesn't connect**
- **Check**: sdkconfig.defaults has all BT flags (should be identical to Core S3)
- **Check**: Build flags include all `-DCONFIG_BT_*` defines
- **Verify**: Serial output shows "Bluetooth initialized"

**Audio quality poor**
- **Try**: Adjust `magnification` parameter in speaker/mic config (currently 16)
- **Try**: Different volume levels (currently 200)
- **Try**: Different sample rates (8000 Hz vs 16000 Hz)

### Step 7: Testing Checklist

Print and check off during testing:

- [ ] Build completes without errors
- [ ] Upload succeeds
- [ ] Device boots (serial output shows initialization)
- [ ] Display shows portrait UI (correct orientation)
- [ ] Status text readable and centered
- [ ] Log messages appear in bottom section
- [ ] Button A press detected (serial: `>>> Button A pressed!`)
- [ ] Bluetooth advertises as "OpenBadge"
- [ ] Phone can pair with device
- [ ] GlassBridge app connects via HFP
- [ ] Status changes to "Tap to Speak" when connected
- [ ] Button press triggers session (status → "Listening...")
- [ ] Microphone captures audio
- [ ] Speaker plays TTS audio
- [ ] Session completes successfully
- [ ] Can repeat voice sessions multiple times
- [ ] No memory leaks (heap free remains stable)
- [ ] No unexpected shutdowns on battery power
- [ ] Core S3 environment still builds (`pio run -e m5stack-cores3`)

### Step 8: Performance Validation

**Expected Metrics:**

- **Button to SCO latency**: <2 seconds
- **Audio quality**: Clear voice, no distortion
- **Memory usage**: >50KB heap free during operation
- **Battery life**: >4 hours continuous use (estimated)

**Monitoring Commands:**

```bash
# Watch serial output for heap stats
pio device monitor -e m5stickc-plus2 --filter esp32_exception_decoder

# Check build size
pio run -e m5stickc-plus2 -v | grep "Flash:"
```

## Success Criteria

Implementation is complete when:

✅ Both environments build successfully (cores3 and m5stickc-plus2)
✅ M5StickC Plus2 boots and displays portrait UI correctly
✅ Button A triggers actions reliably
✅ Audio I/O works bidirectionally
✅ Bluetooth pairs and connects via HFP
✅ GlassBridge end-to-end voice session works
✅ No regressions in M5Stack Core S3 build
✅ All documentation complete

## Next Steps

After successful implementation:

1. **Optimize UI** - Tune font sizes, colors, layout for readability
2. **Optimize Audio** - Tune volume, gain, and sample rate for best quality
3. **Test Battery Life** - Measure actual runtime on 200mAh battery
4. **Add Configuration** - Make volume/gain user-configurable via SPIFFS
5. **Test Edge Cases** - Multiple sessions, reconnections, error recovery

## File Reference

**Created Files:**
- `src/HAL/Board_M5StickCPlus2.h` - 89 lines
- `src/HAL/Board_M5StickCPlus2.cpp` - 282 lines
- `default_8MB.csv` - 7 lines

**Modified Files:**
- `src/HAL/BoardManager.h` - Added Plus2 factory case
- `platformio.ini` - Added m5stickc-plus2 environment

**Documentation:**
- `docs/IMPLEMENTATION_GUIDE.md` (this file)
- `docs/MIGRATION_M5STICKC_PLUS2.md` - Migration rationale
- `docs/HARDWARE_COMPARISON.md` - Detailed hardware specs
- `docs/ARCHITECTURE.md` - Updated with multi-board support

## Support

For issues or questions:
- Review troubleshooting section above
- Check serial output for error messages
- Compare with Board_M5CoreS3.cpp reference implementation
- Verify M5Unified library version (0.1.13+)
- Test with M5StickC Plus2 example sketches to isolate hardware issues
