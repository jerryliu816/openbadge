#include "Board_M5CoreS3.h"
#include <Arduino.h>
#include <cstdarg>

void Board_M5CoreS3::init() {
    // Configure M5Unified
    auto cfg = M5.config();
    cfg.internal_spk = true;   // Enable internal speaker
    cfg.internal_mic = true;   // Enable internal microphone
    cfg.output_power = true;   // Enable power output control
    M5.begin(cfg);

    // CRITICAL: Initialize power management
    // Without this, the AW88298 speaker amplifier has no power!
    M5.Power.begin();

    // Configure speaker for voice audio
    auto spk_cfg = M5.Speaker.config();
    spk_cfg.sample_rate = m_sampleRate;
    spk_cfg.stereo = false;              // Mono for voice
    spk_cfg.buzzer = false;              // Not using buzzer mode
    spk_cfg.magnification = 16;          // Volume multiplier
    M5.Speaker.config(spk_cfg);
    M5.Speaker.begin();
    M5.Speaker.setVolume(200);           // 0-255

    // Configure microphone
    auto mic_cfg = M5.Mic.config();
    mic_cfg.sample_rate = m_sampleRate;
    mic_cfg.stereo = false;              // Mono
    mic_cfg.magnification = 16;          // Gain
    M5.Mic.config(mic_cfg);
    M5.Mic.begin();

    // Initialize display
    M5.Display.setRotation(1);           // Landscape (320x240)
    M5.Display.setBrightness(128);

    // Clear screen and draw initial layout
    M5.Display.fillScreen(TFT_BLACK);

    // Draw initial status
    setLedStatus(StatusState::Disconnected);

    // Initialize log section background
    M5.Display.fillRect(0, LOG_Y_START, SCREEN_WIDTH, LOG_HEIGHT, TFT_BLACK);

    // Draw separator line
    M5.Display.drawFastHLine(0, LOG_Y_START, SCREEN_WIDTH, TFT_DARKGREY);

    // Reserve log buffer space
    m_logLines.reserve(LOG_BUFFER_SIZE);

    // Initial log messages
    log("OpenBadge v1.0");
    log("Initializing...");
    logf("Speaker: %d Hz mono", m_sampleRate);
    logf("Mic: %d Hz mono", m_sampleRate);
    log("Hardware ready");
}

void Board_M5CoreS3::update() {
    M5.update();  // Updates touch, buttons, power state
}

bool Board_M5CoreS3::isActionTriggered() {
    // Only detect touch in the STATUS section (top part of screen)
    // This prevents accidental triggers when scrolling logs
    auto touch = M5.Touch.getDetail();
    bool inStatusArea = (touch.y < STATUS_HEIGHT);
    bool currentTouch = (M5.Touch.getCount() > 0) && inStatusArea;

    // Detect rising edge (finger down)
    bool triggered = currentTouch && !m_lastTouchState;
    m_lastTouchState = currentTouch;

    if (triggered) {
        log(">>> Touch triggered!");
    }
    return triggered;
}

void Board_M5CoreS3::setLedStatus(StatusState state) {
    if (state == m_currentState) return;
    m_currentState = state;

    // Select color and text based on state
    uint32_t color;
    const char* text;

    switch (state) {
        case StatusState::Disconnected:
            color = TFT_DARKGREY;
            text = "Not Connected";
            break;
        case StatusState::Idle:
            color = TFT_BLUE;
            text = "Tap to Speak";
            break;
        case StatusState::Listening:
            color = TFT_RED;
            text = "Listening...";
            break;
        case StatusState::Speaking:
            color = TFT_GREEN;
            text = "Speaking...";
            break;
        default:
            color = TFT_BLACK;
            text = "Unknown";
            break;
    }

    drawStatusSection(text, color);
    logf("Status: %s", text);
}

void Board_M5CoreS3::drawStatusSection(const char* text, uint32_t bgColor) {
    // Fill status area with background color
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, STATUS_HEIGHT, bgColor);

    // Draw main status text (large, centered)
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.setTextColor(TFT_WHITE, bgColor);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString(text, SCREEN_WIDTH / 2, STATUS_HEIGHT / 2 - 10);

    // Draw "OpenBadge" label (small, bottom of status area)
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextColor(TFT_LIGHTGREY, bgColor);
    M5.Display.drawString("OpenBadge", SCREEN_WIDTH / 2, STATUS_HEIGHT - 15);

    // Redraw separator line
    M5.Display.drawFastHLine(0, LOG_Y_START, SCREEN_WIDTH, TFT_DARKGREY);
}

void Board_M5CoreS3::addLogLine(const char* message) {
    // Add to circular buffer
    if (m_logLines.size() >= LOG_BUFFER_SIZE) {
        m_logLines.erase(m_logLines.begin());
    }
    m_logLines.push_back(std::string(message));

    // Redraw log section
    drawLogSection();
}

void Board_M5CoreS3::drawLogSection() {
    // Clear log area (preserve separator line)
    M5.Display.fillRect(0, LOG_Y_START + 1, SCREEN_WIDTH, LOG_HEIGHT - 1, TFT_BLACK);

    // Configure text for log display
    M5.Display.setFont(&fonts::Font2);  // Small fixed-width-ish font
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setTextDatum(top_left);

    // Calculate how many lines to show (most recent)
    int startIdx = 0;
    if (m_logLines.size() > LOG_MAX_LINES) {
        startIdx = m_logLines.size() - LOG_MAX_LINES;
    }

    // Draw visible lines
    int y = LOG_Y_START + LOG_PADDING;
    for (size_t i = startIdx; i < m_logLines.size(); i++) {
        // Truncate long lines
        std::string line = m_logLines[i];
        if (line.length() > 38) {  // ~38 chars fit at this font size
            line = line.substr(0, 35) + "...";
        }

        M5.Display.drawString(line.c_str(), LOG_PADDING, y);
        y += LOG_LINE_HEIGHT;
    }
}

void Board_M5CoreS3::log(const char* message) {
    // Output to serial (always)
    Serial.println(message);

    // Add to screen log
    addLogLine(message);
}

void Board_M5CoreS3::logf(const char* format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    log(buffer);
}

size_t Board_M5CoreS3::writeAudio(const uint8_t* data, size_t size) {
    if (size == 0) return 0;

    // M5.Speaker.playRaw expects int16_t samples
    size_t samples = size / sizeof(int16_t);

    // playRaw parameters: (data, samples, sample_rate, stereo, repeat_count)
    bool success = M5.Speaker.playRaw((const int16_t*)data, samples, m_sampleRate, false, 1, -1);

    return success ? size : 0;
}

size_t Board_M5CoreS3::readAudio(uint8_t* data, size_t size) {
    if (size == 0) return 0;

    size_t samplesToRead = size / sizeof(int16_t);
    if (samplesToRead > MIC_BUFFER_SAMPLES) {
        samplesToRead = MIC_BUFFER_SAMPLES;
    }

    // Record synchronously (blocking)
    if (M5.Mic.record(m_micBuffer, samplesToRead, m_sampleRate)) {
        // Wait for recording to complete
        while (M5.Mic.isRecording()) {
            delayMicroseconds(100);
        }

        // Copy to output buffer
        size_t bytesToCopy = samplesToRead * sizeof(int16_t);
        memcpy(data, m_micBuffer, bytesToCopy);
        return bytesToCopy;
    }

    return 0;
}

void Board_M5CoreS3::setSampleRate(int rate) {
    if (rate == m_sampleRate) return;

    logf("Sample rate: %d -> %d Hz", m_sampleRate, rate);
    m_sampleRate = rate;

    // Stop current audio operations
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
