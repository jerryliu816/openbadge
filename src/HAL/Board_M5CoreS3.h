#pragma once

#include "IBoard.h"
#include <M5Unified.h>
#include <vector>
#include <string>

/**
 * M5Stack CoreS3 Board Implementation
 *
 * Screen Layout (320x240 landscape):
 * ┌─────────────────────────────────┐
 * │      STATUS DISPLAY SECTION     │  (Top 100px)
 * │   [Colored background + text]   │
 * ├─────────────────────────────────┤
 * │      TEXT LOG SECTION           │  (Bottom 140px)
 * │   [Scrolling log messages]      │
 * │   > BT initialized              │
 * │   > Connected to phone          │
 * │   > SCO audio active            │
 * └─────────────────────────────────┘
 *
 * Uses M5Unified library for hardware abstraction.
 * Handles: ILI9342C display, touch input, AW88298 speaker, PDM microphones
 */
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
    // Audio settings
    int m_sampleRate = 16000;

    // UI state
    StatusState m_currentState = StatusState::Disconnected;
    bool m_lastTouchState = false;

    // Screen layout constants
    static constexpr int16_t SCREEN_WIDTH = 320;
    static constexpr int16_t SCREEN_HEIGHT = 240;
    static constexpr int16_t STATUS_HEIGHT = 100;
    static constexpr int16_t LOG_HEIGHT = SCREEN_HEIGHT - STATUS_HEIGHT;
    static constexpr int16_t LOG_Y_START = STATUS_HEIGHT;
    static constexpr int16_t LOG_LINE_HEIGHT = 16;
    static constexpr int16_t LOG_MAX_LINES = LOG_HEIGHT / LOG_LINE_HEIGHT;  // ~8 lines
    static constexpr int16_t LOG_PADDING = 4;

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
