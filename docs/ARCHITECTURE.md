# OpenBadge Firmware Architecture

## Executive Summary

OpenBadge is an ESP32 based wearable firmware that emulates a Bluetooth Smart Headset. It serves as the reference hardware client for the GlassBridge Android application, enabling AI voice interactions through standard Bluetooth HFP (Hands-Free Profile) and AVRCP (Audio/Video Remote Control Profile).

**Supported Hardware:**
- M5Stack Core S3 (ESP32-S3, 320x240 touch screen)
- M5StickC Plus2 (ESP32-PICO-V3-02, 135x240 buttons)

**Framework:** Arduino with ESP-IDF components
**Primary Library:** M5Unified (for hardware abstraction)

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Critical Technical Discoveries](#critical-technical-discoveries)
3. [Hardware Specification](#hardware-specification)
4. [Supported Hardware](#supported-hardware)
5. [Directory Structure](#directory-structure)
6. [Component Architecture](#component-architecture)
7. [Bluetooth Stack Implementation](#bluetooth-stack-implementation)
8. [Audio Pipeline](#audio-pipeline)
9. [HAL Interface Specification](#hal-interface-specification)
10. [GlassBridge Integration Contract](#glassbridge-integration-contract)
11. [PlatformIO Configuration](#platformio-configuration)
12. [Implementation Checklist](#implementation-checklist)
13. [Verification Plan](#verification-plan)
14. [Audio Buffer Architecture](#audio-buffer-architecture)
15. [Reconnection Strategy](#reconnection-strategy)
16. [Memory Architecture](#memory-architecture)
17. [Error Handling](#error-handling)
18. [Testing Strategy](#testing-strategy)
19. [Performance Targets](#performance-targets)

---

## System Overview

### The Big Picture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        GLASSBRIDGE ANDROID APP                       │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────┐  ┌───────────┐ │
│  │MediaButton  │  │BluetoothSco  │  │AudioInput   │  │OpenAI API │ │
│  │Interceptor  │  │Manager       │  │Manager      │  │(Whisper/  │ │
│  │             │  │              │  │             │  │GPT/TTS)   │ │
│  └──────┬──────┘  └──────┬───────┘  └──────┬──────┘  └─────┬─────┘ │
│         │                │                 │                │       │
│         │    ┌───────────┴─────────────────┴────────────────┘       │
│         │    │                                                       │
│         ▼    ▼                                                       │
│  ┌─────────────────┐                                                 │
│  │GlassSession     │ State Machine: Idle→Connecting→Listening→      │
│  │Manager          │ Thinking→Speaking→Cooldown                      │
│  └────────┬────────┘                                                 │
└───────────┼─────────────────────────────────────────────────────────┘
            │ Bluetooth HFP/AVRCP
            │ SCO Audio Channel
            ▼
┌───────────────────────────────────────────────────────────────────┐
│                        OPENBADGE FIRMWARE                          │
│                        (THIS PROJECT)                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌───────────────────┐  │
│  │BluetoothManager │  │Board_M5CoreS3   │  │main.cpp           │  │
│  │- HFP Client     │  │- Display UI     │  │- Setup/Loop       │  │
│  │- AVRCP Ctrl     │  │- Touch Input    │  │- Coordination     │  │
│  │- SCO Audio      │  │- I2S Speaker    │  │                   │  │
│  │                 │  │- PDM Mic        │  │                   │  │
│  └────────┬────────┘  └────────┬────────┘  └─────────┬─────────┘  │
│           │                    │                     │             │
│           └────────────────────┴─────────────────────┘             │
│                                │                                    │
│                        ┌───────┴───────┐                           │
│                        │  M5Unified    │                           │
│                        │  Library      │                           │
│                        └───────┬───────┘                           │
│                                │                                    │
│                        ┌───────┴───────┐                           │
│                        │ M5Stack CoreS3│                           │
│                        │ Hardware      │                           │
│                        └───────────────┘                           │
└───────────────────────────────────────────────────────────────────┘
```

### System Roles

| Component | Role | Responsibilities |
|-----------|------|------------------|
| **GlassBridge (Android)** | Gateway/Processor | Intercepts button, manages SCO, captures audio, calls OpenAI APIs, plays TTS |
| **OpenBadge (ESP32)** | Dumb Terminal | Mic input, speaker output, button trigger, visual feedback |

### Key Principle

**The Phone initiates everything.** OpenBadge is passive:
- Phone initiates SCO connection (via `startBluetoothSco()`)
- Phone captures audio from SCO channel
- Phone plays TTS back through SCO channel
- OpenBadge just sends the trigger signal and routes audio

---

## Critical Technical Discoveries

### ESP32-A2DP Library Limitation

**CRITICAL: The ESP32-A2DP library by Phil Schatzmann does NOT support HFP.**

The library only supports:
- A2DP Sink (receive music streaming)
- A2DP Source (send music streaming)
- AVRCP (remote control)

It does **NOT** support:
- HFP (Hands-Free Profile) - Required for voice calls
- HSP (Headset Profile)
- SCO audio (Synchronous Connection-Oriented) - Required for bidirectional voice

### Required Solution

Use **ESP-IDF's native Bluetooth Classic APIs** directly:

```cpp
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"    // HFP Client
#include "esp_avrc_api.h"          // AVRCP Controller
```

These APIs are available in Arduino framework when using ESP-IDF as the underlying platform.

### Wideband Speech (WBS/mSBC) Support

For 16kHz audio (better Whisper accuracy), WBS must be enabled:

1. **Build flag:** `-DCONFIG_BT_HFP_WBS_ENABLE=1`
2. **Data path:** Must use HCI data path (not PCM) for mSBC codec
3. **Build flag:** `-DCONFIG_BT_HFP_AUDIO_DATA_PATH_HCI=1`

If WBS negotiation fails, fallback to CVSD at 8kHz.

---

## Hardware Specification

### M5Stack CoreS3

| Component | Specification | Notes |
|-----------|--------------|-------|
| **MCU** | ESP32-S3 (Xtensa LX7 dual-core @ 240MHz) | Bluetooth Classic + BLE |
| **Flash** | 16MB | Sufficient for firmware |
| **PSRAM** | 8MB | Available for audio buffers |
| **Audio Output** | AW88298 I2S Amplifier | Built-in speaker |
| **Audio Input** | Dual ES7210 PDM Microphones | Built-in |
| **Display** | ILI9342C 2" IPS LCD (320x240) | Touch enabled |
| **Power** | AXP2101 PMIC | **CRITICAL: Must enable audio power rails** |
| **Battery** | 500mAh LiPo | Built-in |

### Critical Hardware Notes

1. **Power Rail Initialization**: The AXP2101 PMIC must explicitly enable the speaker amplifier power rail. Without this, audio output is completely silent.

```cpp
// REQUIRED in init()
M5.Power.begin();
```

2. **I2S Pin Configuration**: M5Unified handles pin mapping internally. Do NOT manually configure I2S pins - use M5.Speaker and M5.Mic APIs.

3. **Touch as Primary Input**: CoreS3 has no physical buttons on the front face. The touchscreen is the primary user input.

---

## Supported Hardware

OpenBadge firmware supports multiple ESP32-based M5Stack devices through the HAL (Hardware Abstraction Layer). Each board implementation provides the same IBoard interface while handling hardware-specific details internally.

### 4.1 Board Comparison

| Feature | M5Stack Core S3 | M5StickC Plus2 |
|---------|----------------|----------------|
| **MCU** | ESP32-S3 (240MHz) | ESP32-PICO-V3-02 (240MHz) |
| **Bluetooth** | Classic + BLE | Classic + BLE |
| **Display** | ILI9342C 320x240 landscape | ST7789v2 135x240 portrait |
| **Input** | Capacitive touch screen | 3 physical buttons (GPIO37/39/35) |
| **Speaker** | AW88298 I2S amplifier | PAM8303 I2S amplifier |
| **Microphone** | ES7210 dual PDM | SPM1423 single PDM |
| **Flash** | 16MB | 8MB |
| **PSRAM** | 8MB | 2MB |
| **Size** | 54×30×16mm (~50g) | 48.2×25.5×13.7mm (~15g) |
| **Form Factor** | Desktop/stationary | Wearable/portable |
| **Best For** | Development, larger UI | Wearable assistant, compact |

### 4.2 Switching Between Boards

The firmware uses compile-time board selection via PlatformIO environments:

**Build for M5Stack Core S3:**
```bash
pio run -e m5stack-cores3
pio run -e m5stack-cores3 --target upload
```

**Build for M5StickC Plus2:**
```bash
pio run -e m5stickc-plus2
pio run -e m5stickc-plus2 --target upload
```

### 4.3 Board-Specific Implementation Details

**M5Stack Core S3:**
- Layout: Landscape (320x240)
- Trigger: Touch screen tap on status section (top 100px)
- Power: Requires explicit `M5.Power.begin()` for AXP2101 PMIC
- Partition: default_16MB.csv (6.4MB app partitions)

**M5StickC Plus2:**
- Layout: Portrait (135x240)
- Trigger: Button A press (GPIO37) using `M5.BtnA.wasPressed()`
- Power: Auto-handled by M5Unified (AXP192 PMIC)
- Partition: default_8MB.csv (3MB app partitions)
- **Important**: Must set `cfg.output_power = true` to prevent auto-shutdown

### 4.4 Adding New Board Support

To add support for another M5Stack device:

1. **Create HAL implementation**:
   - `src/HAL/Board_NewDevice.h` - Declare class implementing IBoard
   - `src/HAL/Board_NewDevice.cpp` - Implement all IBoard methods

2. **Update BoardManager.h**:
   ```cpp
   #elif defined(BOARD_NEW_DEVICE)
       #include "Board_NewDevice.h"
   ```

   ```cpp
   #elif defined(BOARD_NEW_DEVICE)
       static Board_NewDevice board;
       return &board;
   ```

3. **Add PlatformIO environment**:
   ```ini
   [env:new-device]
   platform = espressif32@6.9.0
   board = <board_id>
   build_flags = -DBOARD_NEW_DEVICE
   ```

4. **Create partition table if needed** (for non-16MB flash sizes)

### 4.5 Hardware Abstraction Benefits

The IBoard interface ensures:
- **Main application code is portable** - Zero changes needed to support new boards
- **Bluetooth logic is hardware-agnostic** - HFP and AVRCP work identically on all boards
- **Audio API is consistent** - Same readAudio()/writeAudio() interface for all hardware
- **Testing is easier** - Can create mock IBoard implementations for unit tests

For detailed hardware specifications, see `docs/HARDWARE_COMPARISON.md`.

---

## Directory Structure

```
/mnt/c/dev4/openbadge/
├── platformio.ini              # Build configuration
├── README.md                   # User-facing documentation
├── docs/
│   └── ARCHITECTURE.md         # This file
├── sdkconfig.defaults          # ESP-IDF configuration overrides (if needed)
└── src/
    ├── main.cpp                # Entry point (minimal)
    │
    ├── Core/                   # Business Logic Layer
    │   ├── BluetoothManager.h
    │   └── BluetoothManager.cpp
    │
    └── HAL/                    # Hardware Abstraction Layer
        ├── IBoard.h            # Pure virtual interface
        ├── Board_M5CoreS3.h
        ├── Board_M5CoreS3.cpp
        └── BoardManager.h      # Factory/selector
```

### Layer Responsibilities

```
┌─────────────────────────────────────────────────────────┐
│                      main.cpp                           │
│  - Arduino setup()/loop()                               │
│  - Instantiate board via BoardManager                   │
│  - Instantiate BluetoothManager                         │
│  - Main coordination loop                               │
└─────────────────────────┬───────────────────────────────┘
                          │
┌─────────────────────────┴───────────────────────────────┐
│                   Core/BluetoothManager                 │
│  - ESP-IDF Bluetooth initialization                     │
│  - HFP Client callbacks and state                       │
│  - AVRCP Controller commands                            │
│  - SCO audio routing                                    │
│  - Sample rate negotiation                              │
│  - Does NOT know about specific hardware                │
└─────────────────────────┬───────────────────────────────┘
                          │ IBoard interface
┌─────────────────────────┴───────────────────────────────┐
│                   HAL/Board_M5CoreS3                    │
│  - M5Unified initialization                             │
│  - Touch input handling                                 │
│  - Display rendering (status colors)                    │
│  - I2S speaker output via M5.Speaker                    │
│  - PDM microphone input via M5.Mic                      │
│  - Sample rate reconfiguration                          │
└─────────────────────────────────────────────────────────┘
```

---

## Component Architecture

### 5.1 IBoard Interface (`HAL/IBoard.h`)

The hardware abstraction interface enabling portability to other boards.

```cpp
#pragma once
#include <cstdint>
#include <cstddef>

enum class StatusState {
    Disconnected,  // Gray  - "Not Connected"
    Idle,          // Blue  - "Tap to Speak"
    Listening,     // Red   - "Listening..."
    Speaking       // Green - "Speaking..."
};

class IBoard {
public:
    virtual ~IBoard() = default;

    // Lifecycle
    virtual void init() = 0;
    virtual void update() = 0;

    // User Input (tap STATUS section to trigger)
    virtual bool isActionTriggered() = 0;

    // Visual Feedback (updates STATUS section)
    virtual void setLedStatus(StatusState state) = 0;

    // Logging (outputs to LOG section + Serial)
    virtual void log(const char* message) = 0;
    virtual void logf(const char* format, ...) = 0;

    // Audio
    virtual size_t writeAudio(const uint8_t* data, size_t size) = 0;
    virtual size_t readAudio(uint8_t* data, size_t size) = 0;
    virtual void setSampleRate(int rate) = 0;
};
```

### 5.2 BoardManager (`HAL/BoardManager.h`)

Compile-time board selection factory.

```cpp
#pragma once
#include "IBoard.h"

#if defined(BOARD_M5_CORES3)
    #include "Board_M5CoreS3.h"
#elif defined(BOARD_M5_STICKC_PLUS2)
    #include "Board_M5StickCPlus2.h"
#else
    #error "No board defined! Add -DBOARD_M5_CORES3 or -DBOARD_M5_STICKC_PLUS2 to build_flags"
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

### 5.3 Board_M5CoreS3 Implementation (`HAL/Board_M5CoreS3.h/.cpp`)

**Screen Layout (320x240 landscape):**
```
┌─────────────────────────────────┐
│      STATUS DISPLAY SECTION     │  Top 100px - Color + "Tap to Speak"
├─────────────────────────────────┤
│      TEXT LOG SECTION           │  Bottom 140px - Scrolling cyan text
│   OpenBadge v1.0                │
│   BT controller OK              │
│   HFP Client OK                 │
└─────────────────────────────────┘
```

**Touch Zones:**
- STATUS section (y < 100): Triggers AI action
- LOG section (y >= 100): Display only, no touch action

**Header:**
```cpp
#pragma once
#include "IBoard.h"
#include <M5Unified.h>
#include <vector>
#include <string>

class Board_M5CoreS3 : public IBoard {
public:
    void init() override;
    void update() override;
    bool isActionTriggered() override;
    void setLedStatus(StatusState state) override;
    void log(const char* message) override;
    void logf(const char* format, ...) override;
    size_t writeAudio(const uint8_t* data, size_t size) override;
    size_t readAudio(uint8_t* data, size_t size) override;
    void setSampleRate(int rate) override;

private:
    int m_sampleRate = 16000;
    StatusState m_currentState = StatusState::Disconnected;
    bool m_lastTouchState = false;

    // Screen layout constants
    static constexpr int16_t STATUS_HEIGHT = 100;
    static constexpr int16_t LOG_HEIGHT = 140;
    static constexpr int16_t LOG_MAX_LINES = 8;

    // Circular log buffer
    std::vector<std::string> m_logLines;

    void drawStatusSection(const char* text, uint32_t bgColor);
    void drawLogSection();
    void addLogLine(const char* message);
};
```

**Implementation (key methods):**
```cpp
#include "Board_M5CoreS3.h"
#include <cstdarg>

void Board_M5CoreS3::init() {
    auto cfg = M5.config();
    cfg.internal_spk = true;
    cfg.internal_mic = true;
    cfg.output_power = true;
    M5.begin(cfg);

    // CRITICAL: Enable speaker power rail
    M5.Power.begin();

    // Configure audio
    auto spk_cfg = M5.Speaker.config();
    spk_cfg.sample_rate = m_sampleRate;
    spk_cfg.stereo = false;
    M5.Speaker.config(spk_cfg);
    M5.Speaker.begin();
    M5.Speaker.setVolume(200);

    auto mic_cfg = M5.Mic.config();
    mic_cfg.sample_rate = m_sampleRate;
    mic_cfg.stereo = false;
    M5.Mic.config(mic_cfg);
    M5.Mic.begin();

    // Initialize split-screen display
    M5.Display.setRotation(1);
    M5.Display.fillScreen(TFT_BLACK);
    setLedStatus(StatusState::Disconnected);

    log("OpenBadge v1.0");
    log("Hardware ready");
}

bool Board_M5CoreS3::isActionTriggered() {
    // Only trigger in STATUS section (top 100px)
    auto touch = M5.Touch.getDetail();
    bool inStatusArea = (touch.y < STATUS_HEIGHT);
    bool currentTouch = (M5.Touch.getCount() > 0) && inStatusArea;
    bool triggered = currentTouch && !m_lastTouchState;
    m_lastTouchState = currentTouch;
    return triggered;
}

void Board_M5CoreS3::setLedStatus(StatusState state) {
    if (state == m_currentState) return;
    m_currentState = state;

    uint32_t color;
    const char* text;
    switch (state) {
        case StatusState::Disconnected: color = TFT_DARKGREY; text = "Not Connected"; break;
        case StatusState::Idle:         color = TFT_BLUE;     text = "Tap to Speak"; break;
        case StatusState::Listening:    color = TFT_RED;      text = "Listening..."; break;
        case StatusState::Speaking:     color = TFT_GREEN;    text = "Speaking..."; break;
    }

    drawStatusSection(text, color);
    logf("Status: %s", text);
}

void Board_M5CoreS3::log(const char* message) {
    Serial.println(message);  // Always to serial
    addLogLine(message);       // Also to screen
}

void Board_M5CoreS3::logf(const char* format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    log(buffer);
}

void Board_M5CoreS3::drawStatusSection(const char* text, uint32_t bgColor) {
    // Fill top section with status color
    M5.Display.fillRect(0, 0, 320, STATUS_HEIGHT, bgColor);
    M5.Display.setTextColor(TFT_WHITE, bgColor);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString(text, 160, 50);
}

void Board_M5CoreS3::drawLogSection() {
    // Clear bottom section
    M5.Display.fillRect(0, STATUS_HEIGHT, 320, LOG_HEIGHT, TFT_BLACK);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);

    // Draw most recent LOG_MAX_LINES
    int startIdx = max(0, (int)m_logLines.size() - LOG_MAX_LINES);
    int y = STATUS_HEIGHT + 4;
    for (size_t i = startIdx; i < m_logLines.size(); i++) {
        M5.Display.drawString(m_logLines[i].c_str(), 4, y);
        y += 16;
    }
}

size_t Board_M5CoreS3::writeAudio(const uint8_t* data, size_t size) {
    // M5.Speaker.playRaw expects int16_t samples
    // data is already int16_t packed as uint8_t
    size_t samples = size / sizeof(int16_t);

    // playRaw: (data, samples, sample_rate, stereo, repeat)
    // Returns true if audio was queued
    if (M5.Speaker.playRaw((const int16_t*)data, samples, m_sampleRate, false, 1)) {
        return size;
    }
    return 0;
}

size_t Board_M5CoreS3::readAudio(uint8_t* data, size_t size) {
    size_t samples = size / sizeof(int16_t);

    // M5.Mic.record fills buffer with int16_t samples
    // Returns number of samples actually recorded
    if (M5.Mic.record((int16_t*)data, samples, m_sampleRate)) {
        // record() is async - check if data is ready
        while (M5.Mic.isRecording()) {
            vTaskDelay(1);
        }
        return size;
    }
    return 0;
}

void Board_M5CoreS3::setSampleRate(int rate) {
    if (rate == m_sampleRate) return;

    Serial.printf("[Board] Changing sample rate: %d -> %d Hz\n", m_sampleRate, rate);
    m_sampleRate = rate;

    // Stop current audio
    M5.Speaker.stop();
    M5.Mic.end();

    // Reconfigure speaker
    auto spk_cfg = M5.Speaker.config();
    spk_cfg.sample_rate = rate;
    M5.Speaker.config(spk_cfg);
    M5.Speaker.begin();

    // Reconfigure microphone
    auto mic_cfg = M5.Mic.config();
    mic_cfg.sample_rate = rate;
    M5.Mic.config(mic_cfg);
    M5.Mic.begin();
}
```

### 5.4 BluetoothManager (`Core/BluetoothManager.h/.cpp`)

**Header:**
```cpp
#pragma once

#include "HAL/IBoard.h"
#include <cstdint>

// Forward declare ESP-IDF types
extern "C" {
    #include "esp_hf_client_api.h"
}

class BluetoothManager {
public:
    // Initialize Bluetooth stack with device name
    void init(const char* deviceName, IBoard* board);

    // Called every loop - handles pending events
    void update();

    // Send media button press via AVRCP (triggers GlassBridge)
    void sendMediaButton();

    // Connection state
    bool isConnected() const { return m_slcConnected; }
    bool isScoConnected() const { return m_scoConnected; }
    bool isWidebandActive() const { return m_wideband; }

    // Audio pump - called from FreeRTOS tasks
    void pumpMicToSco();
    void pumpScoToSpeaker(const uint8_t* data, uint32_t len);

private:
    IBoard* m_board = nullptr;
    bool m_slcConnected = false;   // Service Level Connection (HFP control)
    bool m_scoConnected = false;   // SCO audio link
    bool m_wideband = false;       // mSBC (true) or CVSD (false)

    // Callbacks (static to work with C API, use singleton pattern)
    static void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
    static void hfClientCallback(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t* param);
    static void avrcCtCallback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param);
    static uint32_t hfOutgoingDataCallback(uint8_t* data, uint32_t len);
    static void hfIncomingDataCallback(const uint8_t* data, uint32_t len);

    // Internal event handlers
    void handleSlcConnected(esp_bd_addr_t& addr);
    void handleSlcDisconnected();
    void handleScoConnected(uint8_t codec);
    void handleScoDisconnected();
};

// Global instance (needed for C callbacks)
extern BluetoothManager* g_btManager;
```

**Implementation (Key Sections):**

```cpp
#include "BluetoothManager.h"
#include <Arduino.h>

extern "C" {
    #include "esp_bt.h"
    #include "esp_bt_main.h"
    #include "esp_bt_device.h"
    #include "esp_gap_bt_api.h"
    #include "esp_hf_client_api.h"
    #include "esp_avrc_api.h"
    #include "nvs_flash.h"
}

BluetoothManager* g_btManager = nullptr;

// ============================================================
// INITIALIZATION
// ============================================================

void BluetoothManager::init(const char* deviceName, IBoard* board) {
    m_board = board;
    g_btManager = this;

    Serial.println("[BT] Initializing Bluetooth stack...");

    // 1. Initialize NVS (required for Bluetooth)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 2. Release memory for BLE (we only use Classic BT)
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

    // 3. Initialize Bluetooth controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    bt_cfg.mode = ESP_BT_MODE_CLASSIC_BT;

    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        Serial.printf("[BT] Controller init failed: %s\n", esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        Serial.printf("[BT] Controller enable failed: %s\n", esp_err_to_name(ret));
        return;
    }

    // 4. Initialize Bluedroid stack
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        Serial.printf("[BT] Bluedroid init failed: %s\n", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        Serial.printf("[BT] Bluedroid enable failed: %s\n", esp_err_to_name(ret));
        return;
    }

    // 5. Set device name
    esp_bt_dev_set_device_name(deviceName);
    Serial.printf("[BT] Device name: %s\n", deviceName);

    // 6. Register GAP callback
    esp_bt_gap_register_callback(gapCallback);

    // 7. Initialize HFP Client
    esp_hf_client_register_callback(hfClientCallback);
    esp_hf_client_init();

    // Register audio data callbacks
    esp_hf_client_register_data_callback(
        hfIncomingDataCallback,   // Incoming audio (phone → badge)
        hfOutgoingDataCallback    // Outgoing audio (badge → phone)
    );

    Serial.println("[BT] HFP Client initialized");

    // 8. Initialize AVRCP Controller
    esp_avrc_ct_register_callback(avrcCtCallback);
    esp_avrc_ct_init();
    Serial.println("[BT] AVRCP Controller initialized");

    // 9. Set discoverable and connectable
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    // 10. Set Class of Device to "Audio - Hands-Free"
    esp_bt_cod_t cod = {0};
    cod.major = ESP_BT_COD_MAJOR_DEV_AV;       // Audio/Video
    cod.minor = 0x04;                           // Hands-free
    cod.service = ESP_BT_COD_SRVC_AUDIO | ESP_BT_COD_SRVC_RENDERING;
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);

    Serial.println("[BT] Stack ready - device is discoverable");
}

// ============================================================
// HFP CLIENT CALLBACKS
// ============================================================

void BluetoothManager::hfClientCallback(esp_hf_client_cb_event_t event,
                                         esp_hf_client_cb_param_t* param) {
    if (!g_btManager) return;

    switch (event) {
        case ESP_HF_CLIENT_CONNECTION_STATE_EVT: {
            // Service Level Connection state change
            auto& conn = param->conn_stat;
            Serial.printf("[HFP] Connection state: %d, peer: %02x:%02x:%02x:%02x:%02x:%02x\n",
                conn.state,
                conn.remote_bda[0], conn.remote_bda[1], conn.remote_bda[2],
                conn.remote_bda[3], conn.remote_bda[4], conn.remote_bda[5]);

            if (conn.state == ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED ||
                conn.state == ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED) {
                g_btManager->handleSlcConnected(conn.remote_bda);
            } else if (conn.state == ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED) {
                g_btManager->handleSlcDisconnected();
            }
            break;
        }

        case ESP_HF_CLIENT_AUDIO_STATE_EVT: {
            // SCO audio connection state
            auto& audio = param->audio_stat;
            Serial.printf("[HFP] Audio state: %d\n", audio.state);

            if (audio.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED ||
                audio.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC) {
                // Determine codec (CVSD=0, mSBC=1)
                uint8_t codec = (audio.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC) ? 1 : 0;
                g_btManager->handleScoConnected(codec);
            } else if (audio.state == ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED) {
                g_btManager->handleScoDisconnected();
            }
            break;
        }

        case ESP_HF_CLIENT_BVRA_EVT:
            // Voice recognition state (from AG)
            Serial.printf("[HFP] Voice recognition: %d\n", param->bvra.value);
            break;

        case ESP_HF_CLIENT_VOLUME_CONTROL_EVT:
            // Volume change request from AG
            Serial.printf("[HFP] Volume: type=%d, value=%d\n",
                param->volume_control.type, param->volume_control.volume);
            break;

        default:
            Serial.printf("[HFP] Event: %d\n", event);
            break;
    }
}

void BluetoothManager::handleScoConnected(uint8_t codec) {
    m_scoConnected = true;
    m_wideband = (codec == 1);

    int sampleRate = m_wideband ? 16000 : 8000;
    Serial.printf("[BT] SCO connected - Codec: %s, Rate: %d Hz\n",
        m_wideband ? "mSBC (Wideband)" : "CVSD (Narrowband)", sampleRate);

    // Configure audio hardware for negotiated rate
    m_board->setSampleRate(sampleRate);
    m_board->setLedStatus(StatusState::Idle);
}

void BluetoothManager::handleScoDisconnected() {
    m_scoConnected = false;
    Serial.println("[BT] SCO disconnected");
    m_board->setLedStatus(StatusState::Idle);
}

// ============================================================
// AUDIO DATA CALLBACKS
// ============================================================

// Called by BT stack when it needs outgoing audio data (mic → phone)
uint32_t BluetoothManager::hfOutgoingDataCallback(uint8_t* data, uint32_t len) {
    if (!g_btManager || !g_btManager->m_board) return 0;

    // Read from microphone
    size_t read = g_btManager->m_board->readAudio(data, len);
    return read;
}

// Called by BT stack when incoming audio arrives (phone → speaker)
void BluetoothManager::hfIncomingDataCallback(const uint8_t* data, uint32_t len) {
    if (!g_btManager || !g_btManager->m_board) return;

    // Write to speaker immediately (low latency)
    g_btManager->m_board->writeAudio(data, len);
}

// ============================================================
// AVRCP - MEDIA BUTTON TRIGGER
// ============================================================

void BluetoothManager::sendMediaButton() {
    if (!m_slcConnected) {
        Serial.println("[BT] Cannot send media button - not connected");
        return;
    }

    Serial.println("[BT] Sending AVRCP Play/Pause...");

    // Send key down
    esp_avrc_ct_send_passthrough_cmd(
        0,                              // Transaction label
        ESP_AVRC_PT_CMD_PLAY,           // Key code
        ESP_AVRC_PT_CMD_STATE_PRESSED   // Key state
    );

    vTaskDelay(pdMS_TO_TICKS(100));     // Brief delay

    // Send key up
    esp_avrc_ct_send_passthrough_cmd(
        0,
        ESP_AVRC_PT_CMD_PLAY,
        ESP_AVRC_PT_CMD_STATE_RELEASED
    );
}

// Alternative 1: HFP button press (sends KEYCODE_HEADSETHOOK)
void BluetoothManager::sendHfpButton() {
    if (!m_slcConnected) return;

    Serial.println("[BT] Sending HFP button press...");
    esp_hf_client_send_key_pressed();
}

// Alternative 2: HFP Voice Recognition Activation (sends AT+BVRA=1)
// This is the cleanest HFP-only approach - no A2DP dependency
void BluetoothManager::sendBvra() {
    if (!m_slcConnected) return;

    Serial.println("[BT] Activating voice recognition via BVRA...");
    esp_hf_client_send_bvra(true);  // true = activate, false = deactivate
}

void BluetoothManager::update() {
    // Event processing happens in callbacks
}
```

**Trigger Method Selection:**

| Method | Command | Android Event | Pros | Cons |
|--------|---------|---------------|------|------|
| **AVRCP** (Primary) | Play/Pause passthrough | `KEYCODE_MEDIA_PLAY_PAUSE` | GlassBridge expects this | Requires A2DP enabled |
| **HFP Button** | `AT+CKPD=200` | `KEYCODE_HEADSETHOOK` | No A2DP needed | May not work on all phones |
| **BVRA** | `AT+BVRA=1` | Voice recognition intent | Clean HFP-only | Not all AGs support it |

**A2DP Dependency Warning:**

AVRCP passthrough commands require A2DP to be enabled in ESP-IDF (`CONFIG_BT_A2DP_ENABLE=1`). This has side effects:
- Phone thinks device supports music streaming (shows in media output list)
- Uses ~40KB additional RAM for A2DP stack
- User might accidentally stream music to badge (poor UX)

**Recommendation:** Start with AVRCP (primary), fallback to HFP button or BVRA if issues arise.

### 5.5 Main Entry Point (`main.cpp`)

```cpp
#include <Arduino.h>
#include "HAL/BoardManager.h"
#include "Core/BluetoothManager.h"

IBoard* g_board = nullptr;
BluetoothManager g_btManager;

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== OpenBadge Firmware ===\n");

    // Initialize hardware abstraction
    g_board = BoardManager::createBoard();
    g_board->init();

    // Initialize Bluetooth
    g_btManager.init("OpenBadge", g_board);

    Serial.println("\n[Main] Ready! Pair this device in Android Bluetooth settings.");
    Serial.println("[Main] Then open GlassBridge app and tap the screen to activate.\n");
}

void loop() {
    // Update hardware (touch, display)
    g_board->update();

    // Update Bluetooth (process events)
    g_btManager.update();

    // Handle user trigger
    if (g_board->isActionTriggered()) {
        if (g_btManager.isScoConnected()) {
            // SCO already active - send trigger
            g_btManager.sendMediaButton();
            g_board->setLedStatus(StatusState::Listening);
        } else if (g_btManager.isConnected()) {
            // Connected but no SCO - this is fine
            // GlassBridge will establish SCO when it receives the button press
            g_btManager.sendMediaButton();
            g_board->setLedStatus(StatusState::Listening);
        } else {
            // Not connected at all
            Serial.println("[Main] Not connected to phone");
        }
    }

    // Small delay to prevent tight loop
    delay(10);
}
```

---

## Bluetooth Stack Implementation

### 6.1 Profile Configuration

| Profile | Role | Purpose |
|---------|------|---------|
| **HFP 1.7** | Client (HF Unit) | Voice audio, SCO link, AT commands |
| **AVRCP 1.6** | Controller | Media button passthrough |

### 6.2 Initialization Sequence

```
1. nvs_flash_init()              - Non-volatile storage for BT pairing
2. esp_bt_controller_init()      - Initialize BT radio
3. esp_bt_controller_enable()    - Enable Classic BT mode
4. esp_bluedroid_init()          - Initialize Bluedroid stack
5. esp_bluedroid_enable()        - Enable Bluedroid
6. esp_bt_dev_set_device_name()  - Set "OpenBadge" name
7. esp_bt_gap_register_callback() - Register GAP events
8. esp_hf_client_register_callback() - Register HFP events
9. esp_hf_client_init()          - Initialize HFP Client
10. esp_hf_client_register_data_callback() - Register audio callbacks
11. esp_avrc_ct_register_callback() - Register AVRCP events
12. esp_avrc_ct_init()           - Initialize AVRCP Controller
13. esp_bt_gap_set_scan_mode()   - Make discoverable
14. esp_bt_gap_set_cod()         - Set device class (Hands-Free)
```

### 6.3 Connection Flow

```
Android                          OpenBadge
   │                                 │
   │  User pairs in BT settings      │
   │────────────────────────────────>│
   │                                 │
   │  ACL Connection                 │
   │<───────────────────────────────>│
   │                                 │
   │  HFP SLC (Service Level Conn)   │
   │<───────────────────────────────>│
   │  ESP_HF_CLIENT_CONNECTION_STATE │
   │                                 │
   │  User taps screen               │
   │                                 │
   │  AVRCP Play/Pause               │
   │<────────────────────────────────│
   │                                 │
   │  GlassBridge receives event     │
   │  Calls startBluetoothSco()      │
   │                                 │
   │  SCO Connection                 │
   │────────────────────────────────>│
   │  ESP_HF_CLIENT_AUDIO_STATE      │
   │                                 │
   │  Bidirectional Audio            │
   │<───────────────────────────────>│
```

### 6.4 WBS/mSBC Codec Negotiation

When SCO connects, the codec is negotiated:

```cpp
case ESP_HF_CLIENT_AUDIO_STATE_EVT: {
    if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC) {
        // Wideband: 16kHz mSBC
        m_board->setSampleRate(16000);
    } else if (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED) {
        // Narrowband: 8kHz CVSD
        m_board->setSampleRate(8000);
    }
}
```

**Build flags required for WBS:**
```ini
-DCONFIG_BT_HFP_WBS_ENABLE=1
-DCONFIG_BT_HFP_AUDIO_DATA_PATH_HCI=1
```

---

## Audio Pipeline

### 7.1 Data Flow Diagram

```
┌──────────────────────────────────────────────────────────────────────┐
│                          MICROPHONE PATH                             │
│                        (Badge → Phone)                               │
│                                                                      │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐              │
│  │ PDM Mic     │───>│ M5.Mic      │───>│ BT Stack    │──> Phone     │
│  │ (Hardware)  │    │ readAudio() │    │ HCI/SCO     │              │
│  └─────────────┘    └─────────────┘    └─────────────┘              │
│                                                                      │
│  Sample Rate: 8kHz (CVSD) or 16kHz (mSBC)                           │
│  Format: PCM 16-bit mono                                             │
│  Packet Size: ~60-120 bytes per callback                             │
└──────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────┐
│                          SPEAKER PATH                                │
│                        (Phone → Badge)                               │
│                                                                      │
│          ┌─────────────┐    ┌─────────────┐    ┌─────────────┐      │
│  Phone ─>│ BT Stack    │───>│ M5.Speaker  │───>│ I2S Amp     │      │
│          │ HCI/SCO     │    │ writeAudio()│    │ (AW88298)   │      │
│          └─────────────┘    └─────────────┘    └─────────────┘      │
│                                                                      │
│  Sample Rate: 8kHz (CVSD) or 16kHz (mSBC)                           │
│  Format: PCM 16-bit mono                                             │
│  Latency Target: <50ms end-to-end                                    │
└──────────────────────────────────────────────────────────────────────┘
```

### 7.2 Audio Timing

| Codec | Sample Rate | Frame Size | Frame Duration |
|-------|-------------|------------|----------------|
| CVSD | 8000 Hz | 60 bytes | 7.5ms |
| mSBC | 16000 Hz | 120 bytes | 7.5ms |

**Critical:** Do NOT buffer excessively. The BT stack calls audio callbacks at ~7.5ms intervals. Process immediately.

### 7.3 Ring Buffer (Optional)

If I2S write blocks, use a small ring buffer:

```cpp
// Only if needed for latency smoothing
#include <freertos/ringbuf.h>

RingbufHandle_t speakerBuffer;

void initAudioBuffers() {
    // 1920 bytes = 60ms at 16kHz mono 16-bit
    speakerBuffer = xRingbufferCreate(1920, RINGBUF_TYPE_BYTEBUF);
}

void hfIncomingDataCallback(const uint8_t* data, uint32_t len) {
    xRingbufferSend(speakerBuffer, data, len, 0);
}

// In a separate FreeRTOS task
void speakerTask(void* param) {
    while (true) {
        size_t len;
        uint8_t* data = (uint8_t*)xRingbufferReceive(speakerBuffer, &len, portMAX_DELAY);
        if (data) {
            g_board->writeAudio(data, len);
            vRingbufferReturnItem(speakerBuffer, data);
        }
    }
}
```

---

## HAL Interface Specification

### 8.1 Method Contract Details

#### `void init()`
- **Preconditions:** None
- **Postconditions:** All hardware ready, display shows initial state
- **Side Effects:** Configures GPIO, I2S, display, power rails
- **Blocking:** Yes, may take 100-500ms

#### `void update()`
- **Call Frequency:** Every loop iteration (~10ms)
- **Purpose:** Poll inputs, refresh display if needed
- **Blocking:** No, should complete in <5ms

#### `bool isActionTriggered()`
- **Returns:** `true` on rising edge of user action
- **Debouncing:** Implementation must debounce internally
- **Repeated Calls:** Returns `false` until next trigger event

#### `void setLedStatus(StatusState state)`
- **Thread Safety:** May be called from any context
- **Idempotent:** Calling with same state should be no-op

#### `size_t writeAudio(const uint8_t* data, size_t size)`
- **Format:** PCM 16-bit signed little-endian mono
- **Blocking:** May block briefly if buffer full
- **Returns:** Bytes actually written (may be less than requested)

#### `size_t readAudio(uint8_t* data, size_t size)`
- **Format:** PCM 16-bit signed little-endian mono
- **Blocking:** Should not block; return 0 if no data
- **Returns:** Bytes actually read

#### `void setSampleRate(int rate)`
- **Valid Values:** 8000 or 16000
- **Effect:** Reconfigures both input and output paths
- **Latency:** May cause brief audio glitch during reconfiguration

---

## GlassBridge Integration Contract

### 9.1 What GlassBridge Expects

| Expectation | OpenBadge Implementation |
|-------------|-------------------------|
| Device appears as Bluetooth headset | Set CoD to Audio/Hands-Free |
| Device name is identifiable | Name = "OpenBadge" |
| Supports HFP for SCO audio | Use esp_hf_client API |
| Sends media button on trigger | AVRCP Play/Pause or HFP button |
| Accepts incoming SCO connection | Passive - stack handles it |
| Supports WBS for 16kHz audio | Enable CONFIG_BT_HFP_WBS_ENABLE |

### 9.2 GlassBridge State Machine Alignment

| GlassBridge State | OpenBadge StatusState | Notes |
|-------------------|----------------------|-------|
| Idle | Idle (Blue) | Waiting for trigger |
| Connecting | Idle (Blue) | Phone establishing SCO |
| Listening | Listening (Red) | Mic active, sending audio |
| Thinking | Listening (Red) | Still recording or processing |
| Speaking | Speaking (Green) | TTS playing through speaker |
| Cooldown | Idle (Blue) | Brief pause before next |

### 9.3 Media Button Event Details

GlassBridge `MediaButtonInterceptor` listens for:
- `KEYCODE_MEDIA_PLAY_PAUSE` (79) - Preferred
- `KEYCODE_HEADSETHOOK` (79) - Alternative

The AVRCP passthrough command maps to these keycodes on Android.

---

## PlatformIO Configuration

### 10.1 Complete `platformio.ini`

```ini
[env:m5stack-cores3]
platform = espressif32@6.4.0
board = m5stack-cores3
framework = arduino
monitor_speed = 115200

; Library dependencies
lib_deps =
    m5stack/M5Unified@^0.1.13

; Build flags
build_flags =
    ; Debug level (0=None, 3=Info, 5=Verbose)
    -DCORE_DEBUG_LEVEL=3

    ; Board selection for HAL
    -DBOARD_M5_CORES3

    ; ===== Bluetooth Classic Configuration =====
    ; Enable Bluetooth
    -DCONFIG_BT_ENABLED=1

    ; Classic BT only (no BLE to save memory)
    -DCONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY=1
    -DCONFIG_BT_CLASSIC_ENABLED=1

    ; ===== HFP Configuration =====
    -DCONFIG_BT_HFP_ENABLE=1
    -DCONFIG_BT_HFP_CLIENT_ENABLE=1

    ; Wideband Speech (mSBC codec for 16kHz audio)
    -DCONFIG_BT_HFP_WBS_ENABLE=1

    ; Use HCI data path for mSBC support
    ; (PCM path only supports CVSD)
    -DCONFIG_BT_HFP_AUDIO_DATA_PATH_HCI=1

    ; ===== AVRCP Configuration =====
    ; Note: AVRCP requires A2DP to be enabled in ESP-IDF
    -DCONFIG_BT_A2DP_ENABLE=1

; Partition table for 16MB flash
board_build.partitions = default_16MB.csv

; Upload settings
upload_speed = 921600

; Monitor filters
monitor_filters = esp32_exception_decoder
```

### 10.2 Optional `sdkconfig.defaults`

If build flags don't work, create `sdkconfig.defaults`:

```
CONFIG_BT_ENABLED=y
CONFIG_BT_CLASSIC_ENABLED=y
CONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY=y
CONFIG_BT_HFP_ENABLE=y
CONFIG_BT_HFP_CLIENT_ENABLE=y
CONFIG_BT_HFP_WBS_ENABLE=y
CONFIG_BT_HFP_AUDIO_DATA_PATH_HCI=y
CONFIG_BT_A2DP_ENABLE=y
```

---

## Implementation Checklist

### Phase 1: Project Setup
- [ ] Create directory structure
- [ ] Create `platformio.ini`
- [ ] Verify M5Unified library installs
- [ ] Create empty source files

### Phase 2: HAL Implementation
- [ ] Implement `IBoard.h` interface
- [ ] Implement `Board_M5CoreS3.cpp`
- [ ] Implement `BoardManager.h`
- [ ] Test hardware: display, touch, speaker, mic
- [ ] Verify `M5.Power.begin()` enables speaker

### Phase 3: Bluetooth Stack
- [ ] Implement BT initialization sequence
- [ ] Register HFP Client callbacks
- [ ] Register AVRCP Controller callbacks
- [ ] Test device discovery (shows as "OpenBadge")
- [ ] Test pairing with Android phone

### Phase 4: HFP Integration
- [ ] Handle SLC connection events
- [ ] Handle SCO connection events
- [ ] Detect WBS vs CVSD codec
- [ ] Call `setSampleRate()` on codec negotiation

### Phase 5: Audio Pipeline
- [ ] Implement audio data callbacks
- [ ] Route mic to HFP outgoing
- [ ] Route HFP incoming to speaker
- [ ] Test voice quality at 8kHz
- [ ] Test voice quality at 16kHz (WBS)

### Phase 6: Trigger Mechanism
- [ ] Implement AVRCP Play/Pause command
- [ ] Test with GlassBridge MediaButtonInterceptor
- [ ] Fallback: Implement HFP button press
- [ ] Verify GlassBridge receives trigger

### Phase 7: Integration Testing
- [ ] Full end-to-end test with GlassBridge
- [ ] Test state transitions (UI colors)
- [ ] Test audio latency
- [ ] Test reconnection after disconnect

---

## Verification Plan

### Build Verification
```bash
cd /mnt/c/dev4/openbadge
pio run
```
Expected: Compiles with 0 errors

### Flash and Boot
```bash
pio run -t upload
pio device monitor
```
Expected: Serial output shows "OpenBadge ready"

### Bluetooth Discovery
1. Open Android Settings → Bluetooth
2. Scan for devices
3. Expected: "OpenBadge" appears in list
4. Pair device

### GlassBridge Integration
1. Open GlassBridge app on Android
2. Verify headset detected in app
3. Tap CoreS3 touchscreen
4. Expected: GlassBridge activates, screen turns red
5. Speak into device
6. Expected: Response plays through speaker, screen turns green

### Audio Quality Check
1. Monitor serial output for codec negotiation
2. Expected: "mSBC (Wideband)" if phone supports WBS
3. Verify speech is clear, not chipmunk/slow-motion

---

## Audio Buffer Architecture

### 13.1 Buffer Configuration

```cpp
// Audio configuration constants
struct AudioConfig {
    static constexpr uint32_t SAMPLE_RATE_NARROWBAND = 8000;   // CVSD codec
    static constexpr uint32_t SAMPLE_RATE_WIDEBAND = 16000;    // mSBC codec
    static constexpr uint8_t CHANNELS = 1;                      // Mono
    static constexpr uint8_t BYTES_PER_SAMPLE = 2;              // 16-bit PCM

    // Frame sizes based on HFP timing (~7.5ms per frame)
    static constexpr uint16_t FRAME_SAMPLES_8K = 60;    // 60 samples @ 8kHz = 7.5ms
    static constexpr uint16_t FRAME_SAMPLES_16K = 120;  // 120 samples @ 16kHz = 7.5ms

    // Buffer sizes (in bytes)
    static constexpr size_t FRAME_SIZE_8K = FRAME_SAMPLES_8K * BYTES_PER_SAMPLE;    // 120 bytes
    static constexpr size_t FRAME_SIZE_16K = FRAME_SAMPLES_16K * BYTES_PER_SAMPLE;  // 240 bytes

    // Ring buffer: 4 frames for jitter absorption (30ms)
    static constexpr size_t RING_BUFFER_FRAMES = 4;
    static constexpr size_t RING_BUFFER_SIZE_16K = FRAME_SIZE_16K * RING_BUFFER_FRAMES;  // 960 bytes
};
```

### 13.2 Buffer Sizing Rationale

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Frame Duration | 7.5ms | HFP SCO packet interval |
| Ring Buffer Depth | 4 frames (30ms) | Absorbs jitter without excessive latency |
| Max Latency Budget | 50ms | Perceptible delay threshold for voice |
| Internal RAM Buffer | 960 bytes | Small enough for SRAM, avoids PSRAM latency |

### 13.3 SCO Audio Callback Implementation

The ESP-IDF HFP Client uses callback-based audio routing:

```cpp
// Incoming audio: Phone → Speaker (TTS response)
static void hf_client_incoming_data_cb(const uint8_t* data, uint32_t len) {
    // Called by BT stack at ~7.5ms intervals
    // len is typically 120 bytes (8kHz) or 240 bytes (16kHz)

    // Direct path (lowest latency):
    g_board->writeAudio(data, len);

    // OR buffered path (if I2S blocks):
    // xRingbufferSend(speakerBuffer, data, len, 0);
}

// Outgoing audio: Microphone → Phone (user speech)
static uint32_t hf_client_outgoing_data_cb(uint8_t* data, uint32_t len) {
    // Called by BT stack requesting audio data
    // Must fill 'data' buffer and return bytes written

    size_t bytesRead = g_board->readAudio(data, len);
    return bytesRead;
}
```

---

## Reconnection Strategy

### 14.1 Connection State Machine

```
                    ┌─────────────┐
                    │   IDLE      │ (Boot state)
                    └──────┬──────┘
                           │ Phone initiates connection
                           ▼
                    ┌─────────────┐
             ┌─────>│ CONNECTING  │
             │      └──────┬──────┘
             │             │ SLC established
             │             ▼
             │      ┌─────────────┐
             │      │  CONNECTED  │<────────────┐
             │      └──────┬──────┘             │
             │             │ Disconnect event   │
             │             ▼                    │
             │      ┌─────────────┐             │
             │      │DISCONNECTED │             │
             │      └──────┬──────┘             │
             │             │ Auto-reconnect     │
             │             ▼                    │
             │      ┌─────────────┐             │
             └──────│ RECONNECTING│─────────────┘
                    └─────────────┘   Success
                           │
                           │ Max attempts exceeded
                           ▼
                    ┌─────────────┐
                    │    IDLE     │ (User must re-pair)
                    └─────────────┘
```

### 14.2 Reconnection Implementation

```cpp
// In BluetoothManager.h
class BluetoothManager {
private:
    esp_bd_addr_t m_lastConnectedDevice;    // MAC address of last phone
    uint8_t m_reconnectAttempts = 0;
    bool m_hasStoredDevice = false;

    static constexpr uint8_t MAX_RECONNECT_ATTEMPTS = 5;
    static constexpr uint32_t RECONNECT_BASE_DELAY_MS = 1000;

public:
    void onDisconnect();
    void attemptReconnect();
    void saveConnectedDevice(esp_bd_addr_t& addr);
};

// In BluetoothManager.cpp
void BluetoothManager::onDisconnect() {
    if (!m_hasStoredDevice) return;

    m_reconnectAttempts = 0;
    attemptReconnect();
}

void BluetoothManager::attemptReconnect() {
    if (m_reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
        m_board->log("Reconnect failed - manual pair required");
        m_board->setLedStatus(StatusState::Disconnected);
        return;
    }

    // Exponential backoff: 1s, 2s, 4s, 8s, 16s
    uint32_t delayMs = RECONNECT_BASE_DELAY_MS * (1 << m_reconnectAttempts);
    m_board->logf("Reconnect attempt %d in %dms", m_reconnectAttempts + 1, delayMs);

    vTaskDelay(pdMS_TO_TICKS(delayMs));

    esp_hf_client_connect(m_lastConnectedDevice);
    m_reconnectAttempts++;
}

void BluetoothManager::saveConnectedDevice(esp_bd_addr_t& addr) {
    memcpy(m_lastConnectedDevice, addr, sizeof(esp_bd_addr_t));
    m_hasStoredDevice = true;
    m_reconnectAttempts = 0;

    // Optionally persist to NVS for reconnection after reboot
    // nvs_set_blob(handle, "last_device", addr, sizeof(esp_bd_addr_t));
}
```

### 14.3 NVS Persistence (Optional)

For reconnection after device reboot:

```cpp
void BluetoothManager::loadStoredDevice() {
    nvs_handle_t handle;
    if (nvs_open("bluetooth", NVS_READONLY, &handle) == ESP_OK) {
        size_t size = sizeof(esp_bd_addr_t);
        if (nvs_get_blob(handle, "last_device", m_lastConnectedDevice, &size) == ESP_OK) {
            m_hasStoredDevice = true;
            m_board->log("Loaded stored device from NVS");
        }
        nvs_close(handle);
    }
}
```

---

## Memory Architecture

### 15.1 Memory Map

| Region | Size | Usage |
|--------|------|-------|
| **Internal SRAM** | 512KB | Bluetooth stack, critical code, small buffers |
| **PSRAM** | 8MB | Display framebuffer, large audio buffers (if needed) |
| **Flash** | 16MB | Firmware, NVS storage |

### 15.2 Memory Allocation Strategy

```cpp
// For small, latency-critical buffers: Use internal RAM
uint8_t audioFrame[240];  // Stack or static allocation

// For large buffers (if ring buffer needed): Use PSRAM
#include "esp_heap_caps.h"

void* largeBuffer = heap_caps_malloc(
    LARGE_BUFFER_SIZE,
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
);

// Verify allocation
if (largeBuffer == nullptr) {
    // Fallback to internal RAM
    largeBuffer = heap_caps_malloc(LARGE_BUFFER_SIZE, MALLOC_CAP_8BIT);
}
```

### 15.3 Stack Sizes

```cpp
// In platformio.ini
build_flags =
    -DARDUINO_LOOP_STACK_SIZE=16384  // Main loop needs more for BT

// FreeRTOS task stack recommendations:
// - Audio processing task: 4096 bytes
// - Bluetooth callbacks: Handled by ESP-IDF (8192 bytes default)
```

### 15.4 Memory Monitoring

```cpp
void logMemoryStats() {
    m_board->logf("Free heap: %d bytes", esp_get_free_heap_size());
    m_board->logf("Min free: %d bytes", esp_get_minimum_free_heap_size());
    m_board->logf("PSRAM free: %d bytes",
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
```

---

## Error Handling

### 16.1 State Transition Guards

```cpp
// Prevent invalid state transitions
bool BluetoothManager::canTrigger() {
    // Only allow trigger in IDLE state
    if (!m_slcConnected) {
        m_board->log("Cannot trigger - not connected");
        return false;
    }

    if (m_scoConnected) {
        m_board->log("Session already active - ignoring");
        return false;
    }

    return true;
}

// In main.cpp
if (g_board->isActionTriggered()) {
    if (g_btManager.canTrigger()) {
        g_board->setLedStatus(StatusState::Listening);
        g_btManager.sendMediaButton();
    }
}
```

### 16.2 Error State Recovery

| State | Error Condition | Recovery Action |
|-------|-----------------|-----------------|
| CONNECTING | Timeout (10s) | Return to IDLE, show "Connection failed" |
| LISTENING | SCO disconnects unexpectedly | Return to IDLE if SLC still connected |
| LISTENING | Button pressed again | Ignore (debounce) |
| ANY | Bluetooth stack error | Log error, attempt soft reset |

### 16.3 Debounce Implementation

```cpp
// In Board_M5CoreS3.cpp
bool Board_M5CoreS3::isActionTriggered() {
    static uint32_t lastTriggerTime = 0;
    const uint32_t DEBOUNCE_MS = 500;  // Minimum time between triggers

    auto touch = M5.Touch.getDetail();
    bool inStatusArea = (touch.y < STATUS_HEIGHT);
    bool currentTouch = (M5.Touch.getCount() > 0) && inStatusArea;

    bool triggered = currentTouch && !m_lastTouchState;
    m_lastTouchState = currentTouch;

    if (triggered) {
        uint32_t now = millis();
        if (now - lastTriggerTime < DEBOUNCE_MS) {
            // Too soon after last trigger
            return false;
        }
        lastTriggerTime = now;
        log(">>> Touch triggered!");
    }

    return triggered;
}
```

---

## Testing Strategy

### 17.1 Unit Tests (Desktop Mock)

```cpp
// test/test_state_machine.cpp
#include <unity.h>
#include "MockBoard.h"

void test_trigger_only_when_connected() {
    MockBoard board;
    BluetoothManager bt;
    bt.init("Test", &board);

    // Not connected - should fail
    TEST_ASSERT_FALSE(bt.canTrigger());

    // Simulate connection
    bt.handleConnectionState(ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED, addr);
    TEST_ASSERT_TRUE(bt.canTrigger());

    // Simulate SCO active - should fail (already in session)
    bt.handleAudioState(ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC);
    TEST_ASSERT_FALSE(bt.canTrigger());
}
```

### 17.2 Integration Tests (Hardware Required)

| Test | Procedure | Pass Criteria |
|------|-----------|---------------|
| **Multi-device pairing** | Pair with 3 different Android phones | All pair successfully |
| **Audio loopback** | Record 5s of speech, play back | Clear, no distortion |
| **Reconnection** | Disconnect BT, wait 30s, reconnect | Auto-reconnects within 5 attempts |
| **Power cycle** | Reboot device while connected | Reconnects to last device |
| **Range test** | Walk 10m away from phone | Audio maintains quality to 8m |

### 17.3 Field Tests

- [ ] **Battery drain (idle)**: Target <5mA average
- [ ] **Battery drain (active)**: Target <50mA average
- [ ] **Interference test**: Operate near WiFi router, microwave
- [ ] **Extended use**: 2-hour continuous voice session
- [ ] **Cold start time**: From power-on to "Ready to pair" <5s

---

## Performance Targets

### 18.1 Latency Budget

| Stage | Target | Measurement Method |
|-------|--------|-------------------|
| Button → AVRCP sent | <50ms | `millis()` delta |
| AVRCP → SCO connect | <500ms | (Phone-dependent) |
| Voice → Phone | <30ms | Audio timestamp analysis |
| TTS → Speaker | <30ms | Audio timestamp analysis |
| **Total E2E** | <3s | User perception |

### 18.2 Audio Quality

| Metric | Target | Test |
|--------|--------|------|
| Sample rate | 16kHz (mSBC preferred) | Check log output |
| Dropouts | 0 per minute | Continuous speech test |
| Distortion | None audible | Listening test |
| Echo/feedback | None | Full-duplex test |

### 18.3 Battery Life

| Mode | Target | Measurement |
|------|--------|-------------|
| Standby (connected, idle) | >24 hours | Timed discharge |
| Active (continuous voice) | >4 hours | Timed discharge |
| Current (idle) | <10mA | Multimeter |
| Current (active) | <80mA | Multimeter |

### 18.4 Reliability

| Metric | Target |
|--------|--------|
| Successful connections | >95% first attempt |
| Auto-reconnect success | >90% within 5 attempts |
| Crash-free runtime | >99.9% (1 crash per 1000 hours) |
| NVS write cycles | <1000/day (for pairing data) |

---

## Appendix A: ESP-IDF API Reference

### HFP Client Events

| Event | Description |
|-------|-------------|
| `ESP_HF_CLIENT_CONNECTION_STATE_EVT` | SLC state change |
| `ESP_HF_CLIENT_AUDIO_STATE_EVT` | SCO state change |
| `ESP_HF_CLIENT_BVRA_EVT` | Voice recognition activation |
| `ESP_HF_CLIENT_VOLUME_CONTROL_EVT` | Volume change request |
| `ESP_HF_CLIENT_CIND_CALL_EVT` | Call state indicator |
| `ESP_HF_CLIENT_RING_IND_EVT` | Incoming call ring |

### AVRCP Passthrough Commands

| Command | Value | Android Keycode |
|---------|-------|-----------------|
| `ESP_AVRC_PT_CMD_PLAY` | 0x44 | KEYCODE_MEDIA_PLAY |
| `ESP_AVRC_PT_CMD_PAUSE` | 0x46 | KEYCODE_MEDIA_PAUSE |
| `ESP_AVRC_PT_CMD_STOP` | 0x45 | KEYCODE_MEDIA_STOP |
| `ESP_AVRC_PT_CMD_FORWARD` | 0x4B | KEYCODE_MEDIA_NEXT |
| `ESP_AVRC_PT_CMD_BACKWARD` | 0x4C | KEYCODE_MEDIA_PREVIOUS |

---

## Appendix B: Troubleshooting

### Issue: No audio output (speaker silent)
**Cause:** AXP2101 power rail not enabled
**Fix:** Ensure `M5.Power.begin()` is called in `init()`

### Issue: "Chipmunk" or "slow-motion" audio
**Cause:** Sample rate mismatch between codec and I2S
**Fix:** Call `setSampleRate()` when SCO connects based on negotiated codec

### Issue: AVRCP commands not received by GlassBridge
**Cause:** A2DP not enabled (required for AVRCP)
**Fix:** Add `-DCONFIG_BT_A2DP_ENABLE=1` to build flags

### Issue: WBS/mSBC not negotiating
**Cause:** HCI data path not configured
**Fix:** Add `-DCONFIG_BT_HFP_AUDIO_DATA_PATH_HCI=1` to build flags

### Issue: Device not discoverable
**Cause:** GAP scan mode not set
**Fix:** Call `esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE)`

---

## Appendix C: References

- [ESP-IDF HFP Client API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_hf_client.html)
- [ESP-IDF AVRCP API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_avrc.html)
- [M5Stack CoreS3 Documentation](https://docs.m5stack.com/en/core/CoreS3)
- [M5Unified GitHub](https://github.com/m5stack/M5Unified)
- [Bluetooth HFP 1.7 Specification](https://www.bluetooth.com/specifications/specs/hands-free-profile-1-7-1/)
- [Bluetooth AVRCP 1.6 Specification](https://www.bluetooth.com/specifications/specs/audio-video-remote-control-profile-1-6-2/)
