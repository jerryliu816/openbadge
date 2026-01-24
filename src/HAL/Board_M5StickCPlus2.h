#pragma once

#include "IBoard.h"
#include <M5Unified.h>
#include <vector>
#include <string>

/**
 * M5StickC Plus2 Board Implementation
 *
 * Screen Layout (135x240 portrait):
 * ┌───────────────┐
 * │    STATUS     │  (Top 80px)
 * │    DISPLAY    │
 * │   [Colored    │
 * │  background   │
 * │   + text]     │
 * ├───────────────┤
 * │   TEXT LOG    │  (Bottom 160px)
 * │   SECTION     │
 * │  [Scrolling   │
 * │    log        │
 * │  messages]    │
 * │ > BT init     │
 * │ > Connected   │
 * │ > SCO active  │
 * │               │
 * └───────────────┘
 *
 * Uses M5Unified library for hardware abstraction.
 * Handles: ST7789v2 display, button input, PAM8303 speaker, SPM1423 PDM mic
 *
 * Key Differences from Core S3:
 * - Portrait orientation (135x240 vs 320x240 landscape)
 * - Physical buttons (GPIO37/39/35) instead of touch screen
 * - Smaller display requires compact UI layout
 * - Shared GPIO0 for speaker/mic (M5Unified handles multiplexing)
 */
class Board_M5StickCPlus2 : public IBoard {
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
    // Audio settings
    int m_sampleRate = 16000;

    // UI state
    StatusState m_currentState = StatusState::Disconnected;

    // Screen layout constants (portrait orientation)
    static constexpr int16_t SCREEN_WIDTH = 135;
    static constexpr int16_t SCREEN_HEIGHT = 240;
    static constexpr int16_t STATUS_HEIGHT = 80;
    static constexpr int16_t LOG_HEIGHT = SCREEN_HEIGHT - STATUS_HEIGHT;
    static constexpr int16_t LOG_Y_START = STATUS_HEIGHT;
    static constexpr int16_t LOG_LINE_HEIGHT = 16;
    static constexpr int16_t LOG_MAX_LINES = LOG_HEIGHT / LOG_LINE_HEIGHT;  // ~10 lines
    static constexpr int16_t LOG_PADDING = 4;

    // Button GPIO (for reference - M5Unified handles these)
    static constexpr int BTN_A_GPIO = 37;  // Main action button
    static constexpr int BTN_B_GPIO = 39;  // Power button
    static constexpr int BTN_C_GPIO = 35;  // Side button

    // Log buffer (circular)
    std::vector<std::string> m_logLines;
    static constexpr size_t LOG_BUFFER_SIZE = 50;  // Keep last 50 lines in memory

    // Audio buffer for mic recording
    static constexpr size_t MIC_BUFFER_SAMPLES = 256;
    int16_t m_micBuffer[MIC_BUFFER_SAMPLES];

    // Internal methods
    void drawStatusSection(const char* text, uint32_t bgColor);
    void drawLogSection();
    void addLogLine(const char* message);
};
