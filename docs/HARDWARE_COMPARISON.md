# Hardware Comparison: M5Stack Core S3 vs M5StickC Plus2

## Quick Reference Table

| Specification | M5Stack Core S3 | M5StickC Plus2 | Impact on OpenBadge |
|--------------|-----------------|----------------|---------------------|
| **MCU** | ESP32-S3 (dual-core 240MHz) | ESP32-PICO-V3-02 (dual-core 240MHz) | None - same performance |
| **Bluetooth** | Classic + BLE | Classic + BLE | None - identical stack |
| **Flash** | 16MB | 8MB | Partition table change (3MB app OK) |
| **PSRAM** | 8MB | 2MB | None - app uses <10KB PSRAM |
| **Display** | ILI9342C 320x240 | ST7789v2 135x240 | UI redesign (portrait, smaller fonts) |
| **Orientation** | Landscape | Portrait | Layout change required |
| **Input** | Capacitive touch | 3 physical buttons | Trigger detection change |
| **Speaker** | AW88298 I2S amp | PAM8303 I2S amp | Volume tuning may differ |
| **Microphone** | ES7210 dual PDM | SPM1423 single PDM | Gain tuning may differ |
| **Audio GPIO** | Dedicated pins | Shared GPIO0 | M5Unified handles multiplexing |
| **PMIC** | AXP2101 | AXP192 | Power init differs |
| **Battery** | Larger capacity | 200mAh | Shorter runtime (4-6h estimated) |
| **Dimensions** | 54×30×16mm | 48.2×25.5×13.7mm | Plus2 40% smaller (wearable) |
| **Weight** | Heavier | Lighter | Plus2 better for wearable |
| **Cost** | Higher | Lower | Plus2 more affordable |

## Detailed Specifications

### Processing

**M5Stack Core S3**
- **MCU**: ESP32-S3-FN8 (Xtensa LX7 dual-core)
- **Frequency**: 240MHz
- **ROM**: 384KB
- **SRAM**: 512KB
- **Flash**: 16MB QSPI
- **PSRAM**: 8MB QSPI (OPI mode)

**M5StickC Plus2**
- **MCU**: ESP32-PICO-V3-02 (Xtensa LX6 dual-core)
- **Frequency**: 240MHz
- **ROM**: 448KB
- **SRAM**: 520KB
- **Flash**: 8MB QSPI (integrated in SiP)
- **PSRAM**: 2MB QSPI (integrated in SiP)

**OpenBadge Impact**: None. Both MCUs provide sufficient performance for Bluetooth Classic + HFP + audio processing.

### Bluetooth

**M5Stack Core S3**
- **Bluetooth Classic**: ✅ (via ESP32-S3)
- **BLE**: ✅
- **Profiles**: HFP, A2DP, AVRCP, SPP
- **Range**: ~10m (typical)

**M5StickC Plus2**
- **Bluetooth Classic**: ✅ (via ESP32-PICO-V3-02)
- **BLE**: ✅
- **Profiles**: HFP, A2DP, AVRCP, SPP
- **Range**: ~10m (typical)

**OpenBadge Impact**: None. Identical Bluetooth stack and capabilities. Zero code changes required.

### Display

**M5Stack Core S3**
- **Panel**: ILI9342C TFT LCD
- **Size**: 2.0 inch
- **Resolution**: 320×240 pixels (QVGA)
- **Orientation**: Landscape (wide)
- **Touch**: Capacitive (single-point)
- **Colors**: 65K (16-bit RGB565)
- **Interface**: SPI

**M5StickC Plus2**
- **Panel**: ST7789v2 TFT LCD
- **Size**: 1.14 inch
- **Resolution**: 135×240 pixels
- **Orientation**: Portrait (tall)
- **Touch**: None (physical buttons instead)
- **Colors**: 65K (16-bit RGB565)
- **Interface**: SPI

**OpenBadge Impact**: Major UI redesign required.

**Layout Changes**:
```
Core S3 (320x240 landscape)       StickC Plus2 (135x240 portrait)
┌──────────────────────┐          ┌─────────┐
│   STATUS (100px)     │          │ STATUS  │  80px
│                      │          │ (80px)  │
├──────────────────────┤          ├─────────┤
│   LOG SECTION        │          │   LOG   │ 160px
│   (140px)            │          │ SECTION │
│                      │          │ (160px) │
│   38 chars/line      │          │         │
│   ~8 lines           │          │ 22 chr  │
│                      │          │ ~10 ln  │
└──────────────────────┘          └─────────┘
```

**Font Changes**:
- Status text: FreeSansBold18pt7b → FreeSansBold12pt7b
- Log text: Font2 → Font0
- Text truncation: 38 chars → 22 chars per line

### Input

**M5Stack Core S3**
- **Touch Screen**: FT6336U capacitive controller
- **Points**: Single-touch
- **Detection**: `M5.Touch.getCount() > 0`
- **Position**: X/Y coordinates available
- **Area**: Entire screen (can detect touch region)

**M5StickC Plus2**
- **Button A**: GPIO37 (main trigger)
- **Button B**: GPIO39 (power button)
- **Button C**: GPIO35 (side button)
- **Detection**: `M5.BtnA.wasPressed()` (built-in debounce)
- **No position data** - discrete on/off only

**OpenBadge Impact**: Trigger detection logic change.

**Code Comparison**:
```cpp
// Core S3 (touch)
bool isActionTriggered() {
    auto touch = M5.Touch.getDetail();
    bool inStatusArea = (touch.y < STATUS_HEIGHT);
    bool currentTouch = (M5.Touch.getCount() > 0) && inStatusArea;
    bool triggered = currentTouch && !m_lastTouchState;
    m_lastTouchState = currentTouch;
    return triggered;
}

// StickC Plus2 (button)
bool isActionTriggered() {
    // M5.update() must be called in update() first!
    return M5.BtnA.wasPressed();  // Built-in edge detection
}
```

### Audio Output (Speaker)

**M5Stack Core S3**
- **Amplifier**: AW88298 I2S (mono)
- **Power**: 1W max
- **Interface**: I2S (dedicated GPIO)
- **Control**: M5.Speaker API
- **Volume**: 0-255 (software control)
- **Sample Rates**: 8000-48000 Hz

**M5StickC Plus2**
- **Amplifier**: PAM8303 I2S (mono)
- **Power**: 0.4W max (smaller)
- **Interface**: I2S (GPIO0 shared)
- **Control**: M5.Speaker API
- **Volume**: 0-255 (software control)
- **Sample Rates**: 8000-48000 Hz

**OpenBadge Impact**: API identical, but volume/gain may need tuning. GPIO0 sharing unverified for simultaneous operation.

**Code** (identical API):
```cpp
// Both boards use same API
auto spk_cfg = M5.Speaker.config();
spk_cfg.sample_rate = 16000;
spk_cfg.stereo = false;
spk_cfg.magnification = 16;  // May need different values per board
M5.Speaker.config(spk_cfg);
M5.Speaker.begin();
M5.Speaker.setVolume(200);  // May need different values per board

// Playback
M5.Speaker.playRaw((int16_t*)data, samples, rate, stereo, repeat, channel);
```

### Audio Input (Microphone)

**M5Stack Core S3**
- **Mic**: ES7210 (dual PDM mics)
- **Channels**: 2 (can use mono or stereo)
- **Interface**: PDM (dedicated GPIO)
- **Control**: M5.Mic API
- **Gain**: Configurable via magnification
- **Sample Rates**: 8000-48000 Hz

**M5StickC Plus2**
- **Mic**: SPM1423 (single PDM mic)
- **Channels**: 1 (mono only)
- **Interface**: PDM (GPIO34/GPIO0 shared)
- **Control**: M5.Mic API
- **Gain**: Configurable via magnification
- **Sample Rates**: 8000-48000 Hz

**OpenBadge Impact**: API identical, but gain may need tuning. GPIO0 sharing unverified for simultaneous operation.

**Code** (identical API):
```cpp
// Both boards use same API
auto mic_cfg = M5.Mic.config();
mic_cfg.sample_rate = 16000;
mic_cfg.stereo = false;  // Always mono for OpenBadge
mic_cfg.magnification = 16;  // May need different values per board
M5.Mic.config(mic_cfg);
M5.Mic.begin();

// Recording
M5.Mic.record(buffer, samples, rate);
```

### GPIO Pin Mapping

**M5Stack Core S3**
```
Display:
  MOSI:  GPIO37
  MISO:  GPIO35
  SCK:   GPIO36
  DC:    GPIO35
  CS:    GPIO3
  RST:   GPIO34

Speaker (AW88298):
  BCLK:  GPIO34
  WS:    GPIO33
  DOUT:  GPIO13 (unused for playback)
  DIN:   GPIO14

Microphone (ES7210):
  BCLK:  GPIO34 (shared with speaker)
  WS:    GPIO33 (shared with speaker)
  DIN:   GPIO14 (shared with speaker)

Touch (FT6336U):
  SDA:   GPIO11
  SCL:   GPIO12
  INT:   GPIO1

I2C (internal):
  SDA:   GPIO11
  SCL:   GPIO12
```

**M5StickC Plus2**
```
Display (ST7789v2):
  MOSI:  GPIO15
  SCK:   GPIO13
  DC:    GPIO14
  CS:    GPIO5
  RST:   GPIO12
  BL:    GPIO27

Speaker (PAM8303):
  BCLK:  GPIO0 (shared!)
  WS:    GPIO0 (shared!)
  DIN:   GPIO0 (shared!)

Microphone (SPM1423):
  CLK:   GPIO0 (shared!)
  DATA:  GPIO34

Buttons:
  A:     GPIO37 (main trigger)
  B:     GPIO39 (power)
  C:     GPIO35 (side)

I2C (internal):
  SDA:   GPIO21
  SCL:   GPIO22

IR LED:
  GPIO19
```

**Critical Difference**: GPIO0 shared between speaker and mic on Plus2. M5Unified library handles multiplexing internally.

### Power Management

**M5Stack Core S3**
- **PMIC**: AXP2101
- **Battery**: Larger capacity (not specified in datasheet)
- **Charging**: USB-C (5V)
- **Voltage**: 3.7V Li-Po
- **Management**: Requires explicit `M5.Power.begin()` call
- **Features**: Voltage monitoring, current monitoring, power path management

**M5StickC Plus2**
- **PMIC**: AXP192
- **Battery**: 200mAh Li-Po
- **Charging**: USB-C (5V)
- **Voltage**: 3.7V Li-Po
- **Management**: M5Unified handles automatically
- **Features**: Voltage monitoring, auto-shutdown protection, power path management

**OpenBadge Impact**: Initialization differs.

**Code Comparison**:
```cpp
// Core S3
auto cfg = M5.config();
cfg.internal_spk = true;
cfg.internal_mic = true;
cfg.output_power = true;
M5.begin(cfg);
M5.Power.begin();  // CRITICAL for Core S3!

// StickC Plus2
auto cfg = M5.config();
cfg.internal_spk = true;
cfg.internal_mic = true;
cfg.output_power = true;  // CRITICAL for Plus2 - prevents auto-shutdown!
M5.begin(cfg);
// No M5.Power.begin() needed - M5Unified handles it
```

**Plus2 Auto-Shutdown Risk**: If `cfg.output_power = false` or not set, AXP192 may auto-shutdown after 60 seconds on battery power.

### Physical Dimensions

**M5Stack Core S3**
- **Dimensions**: 54×30×16mm
- **Weight**: ~50g (with battery)
- **Form Factor**: Rectangular "brick"
- **Mounting**: M3 mounting holes, groove connector
- **Wearability**: Not ideal (bulky)

**M5StickC Plus2**
- **Dimensions**: 48.2×25.5×13.7mm
- **Weight**: ~15g (with battery)
- **Form Factor**: Compact "stick"
- **Mounting**: Watch band groove, clip mount
- **Wearability**: Excellent (wrist-mountable)

**Size Comparison**:
```
Core S3: █████████████████  (volume: ~26 cm³)
Plus2:   ████               (volume: ~17 cm³)

Plus2 is ~35% smaller by volume, ~40% lighter
```

### Connectivity

**Both Boards**
- **WiFi**: 802.11 b/g/n (2.4GHz)
- **Bluetooth**: Classic + BLE
- **USB**: USB-C (data + charging)
- **GPIO**: Header pins available

**M5Stack Core S3 Additional**
- **Port A**: I2C connector (Grove compatible)
- **Port B**: UART connector
- **Port C**: I2C connector
- **TF Card**: MicroSD card slot

**M5StickC Plus2 Additional**
- **HAT Connector**: Grove-compatible I2C/GPIO
- **IR LED**: GPIO19 (infrared transmitter)

### Memory Partitions

**M5Stack Core S3** (16MB Flash)
```
nvs:      20KB   (0x9000-0xe000)
otadata:   8KB   (0xe000-0x10000)
app0:     6.4MB  (0x10000-0x650000)
app1:     6.4MB  (0x650000-0xc90000)
spiffs:   3.5MB  (0xc90000-0xff0000)
coredump: 64KB   (0xff0000-0x1000000)
```

**M5StickC Plus2** (8MB Flash)
```
nvs:      20KB   (0x9000-0xe000)
otadata:   8KB   (0xe000-0x10000)
app0:     3MB    (0x10000-0x310000)
app1:     3MB    (0x310000-0x610000)
spiffs:   1.9MB  (0x610000-0x7f0000)
coredump: 64KB   (0x7f0000-0x800000)
```

**OpenBadge Impact**: Current build ~2.3MB, fits comfortably in 3MB partitions. SPIFFS reduced but still ample for config files.

## Software Abstraction

Despite hardware differences, M5Unified library provides identical API for both boards:

```cpp
// Initialization (mostly identical)
M5.begin(cfg);

// Display (identical API)
M5.Display.setRotation(rotation);
M5.Display.fillScreen(color);
M5.Display.drawString(text, x, y);

// Audio output (identical API)
M5.Speaker.playRaw(data, samples, rate, stereo, repeat, channel);

// Audio input (identical API)
M5.Mic.record(buffer, samples, rate);

// Input (different methods)
// Core S3:
bool touched = M5.Touch.getCount() > 0;

// Plus2:
bool pressed = M5.BtnA.wasPressed();

// Power (different initialization)
// Core S3:
M5.Power.begin();

// Plus2:
// (handled automatically by M5Unified)
```

This abstraction is why the migration required only ~300 lines of board-specific code.

## Decision Matrix

**Choose M5Stack Core S3 if you need:**
- ✅ Large display (2.0" vs 1.14")
- ✅ Touch screen interaction
- ✅ Longer battery life
- ✅ More flash storage (16MB vs 8MB)
- ✅ Desktop/stationary use
- ✅ Rich visual feedback

**Choose M5StickC Plus2 if you need:**
- ✅ Compact wearable form factor
- ✅ Lightweight (<15g)
- ✅ Physical button control
- ✅ Lower cost
- ✅ Discrete/low-profile
- ✅ Mobile/on-the-go use

**Both boards provide:**
- ✅ Bluetooth Classic (HFP, A2DP, AVRCP)
- ✅ Full-duplex audio (speaker + mic)
- ✅ 240MHz dual-core ESP32
- ✅ M5Unified library support
- ✅ Sufficient performance for OpenBadge

## GPIO0 Sharing Analysis

**M5StickC Plus2 Critical Detail**: GPIO0 is shared between speaker (PAM8303) and microphone (SPM1423).

**Theory**: M5Unified library handles I2S multiplexing internally, allowing:
1. Speaker playback (I2S TX on GPIO0)
2. Microphone recording (PDM RX on GPIO0)
3. **Simultaneous operation** (required for HFP SCO bidirectional audio)

**Testing Required**: Verify simultaneous speaker output + mic input works during active phone call (SCO connection).

**Risk**: If M5Unified doesn't handle simultaneous operation, may need application-layer time-division multiplexing (TDM):
- Buffer outgoing audio
- Alternate: record 10ms → playback 10ms → record 10ms → ...
- Will increase latency and complexity

**Mitigation**: Test early in integration phase. If simultaneous operation fails, this is a project blocker for Plus2.

## Summary

The M5StickC Plus2 is a viable alternative to the M5Stack Core S3 for OpenBadge, providing:
- **40% smaller size** and **70% lighter weight** for wearability
- **Identical Bluetooth and audio capabilities** (same software stack)
- **Sufficient memory** (3MB app partitions vs current 2.3MB build)
- **Clean HAL abstraction** (only ~300 lines of board-specific code)

Primary trade-offs:
- **Smaller display** (requires UI redesign)
- **Shorter battery life** (200mAh vs larger Core S3 battery)
- **GPIO0 sharing** (unverified simultaneous speaker+mic operation)

**Recommendation**: Proceed with implementation and testing. The hardware is sufficient, but real-world validation of audio quality, battery life, and GPIO0 multiplexing is required before production use.
