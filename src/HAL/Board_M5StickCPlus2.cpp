#include "Board_M5StickCPlus2.h"
#include <Arduino.h>
#include <cstdarg>

void Board_M5StickCPlus2::init() {
    // Configure M5Unified for StickC Plus2
    auto cfg = M5.config();
    // TEMPORARILY DISABLE speaker/mic to avoid I2S conflicts with Bluetooth
    // Will re-enable after Bluetooth is stable
    cfg.internal_spk = false;  // Disable internal speaker (PAM8303)
    cfg.internal_mic = false;  // Disable internal microphone (SPM1423)
    cfg.output_power = true;   // CRITICAL: Prevent auto-shutdown on battery
    M5.begin(cfg);

    // Note: M5StickC Plus2 has AXP192 PMIC, M5Unified handles power automatically
    // Unlike Core S3, we don't need explicit M5.Power.begin() call
    // cfg.output_power = true prevents 60-second auto-shutdown on battery

    // TEMPORARILY COMMENTED OUT: Speaker/mic initialization
    // Causes stack overflow in spk_task when combined with Bluetooth
    // Will re-enable after Bluetooth is working
    /*
    // Configure speaker for voice audio (PAM8303 via I2S GPIO0)
    auto spk_cfg = M5.Speaker.config();
    spk_cfg.sample_rate = m_sampleRate;
    spk_cfg.stereo = false;              // Mono for voice
    spk_cfg.buzzer = false;              // Not using buzzer mode
    spk_cfg.magnification = 16;          // Volume multiplier (may need tuning)
    M5.Speaker.config(spk_cfg);
    M5.Speaker.begin();
    M5.Speaker.setVolume(200);           // 0-255 (may need tuning for PAM8303)

    // Configure microphone (SPM1423 PDM via GPIO34/GPIO0)
    auto mic_cfg = M5.Mic.config();
    mic_cfg.sample_rate = m_sampleRate;
    mic_cfg.stereo = false;              // Mono
    mic_cfg.magnification = 16;          // Gain (may need tuning for sensitivity)
    M5.Mic.config(mic_cfg);
    M5.Mic.begin();
    */

    // Initialize display (portrait orientation)
    // Try rotation 0 first; if display is upside down, try rotation 2
    M5.Display.setRotation(0);           // Portrait (135x240)
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
    log("M5StickC Plus2");
    log("Audio: BT only");  // Using Bluetooth for audio, not built-in speaker/mic
    log("Hardware ready");
    log("Press Button A");
}

void Board_M5StickCPlus2::update() {
    // CRITICAL: Update M5 state for button detection
    // This must be called every loop for wasPressed() to work correctly
    M5.update();
}

bool Board_M5StickCPlus2::isActionTriggered() {
    // Use M5Unified's built-in edge detection and debouncing
    // wasPressed() returns true once per button press (rising edge)
    // This is cleaner than manual current && !last logic
    bool triggered = M5.BtnA.wasPressed();

    if (triggered) {
        log(">>> Button A pressed!");
    }

    return triggered;
}

void Board_M5StickCPlus2::setLedStatus(StatusState state) {
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

void Board_M5StickCPlus2::drawStatusSection(const char* text, uint32_t bgColor) {
    // Fill status area with background color
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, STATUS_HEIGHT, bgColor);

    // Draw main status text (smaller font for narrow screen)
    // Core S3 uses FreeSansBold18pt7b, Plus2 uses FreeSansBold12pt7b
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.setTextColor(TFT_WHITE, bgColor);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString(text, SCREEN_WIDTH / 2, STATUS_HEIGHT / 2 - 8);

    // Draw "OpenBadge" label (small, bottom of status area)
    M5.Display.setFont(&fonts::Font0);  // Tiny font for narrow screen
    M5.Display.setTextColor(TFT_LIGHTGREY, bgColor);
    M5.Display.drawString("OpenBadge", SCREEN_WIDTH / 2, STATUS_HEIGHT - 10);

    // Redraw separator line
    M5.Display.drawFastHLine(0, LOG_Y_START, SCREEN_WIDTH, TFT_DARKGREY);
}

void Board_M5StickCPlus2::addLogLine(const char* message) {
    // Add to circular buffer
    if (m_logLines.size() >= LOG_BUFFER_SIZE) {
        m_logLines.erase(m_logLines.begin());
    }
    m_logLines.push_back(std::string(message));

    // Redraw log section
    drawLogSection();
}

void Board_M5StickCPlus2::drawLogSection() {
    // Clear log area (preserve separator line)
    M5.Display.fillRect(0, LOG_Y_START + 1, SCREEN_WIDTH, LOG_HEIGHT - 1, TFT_BLACK);

    // Configure text for log display
    M5.Display.setFont(&fonts::Font0);  // Small font for narrow screen
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
        // Truncate long lines (narrow screen: ~22 chars fit)
        std::string line = m_logLines[i];
        if (line.length() > 22) {
            line = line.substr(0, 19) + "...";
        }

        M5.Display.drawString(line.c_str(), LOG_PADDING, y);
        y += LOG_LINE_HEIGHT;
    }
}

void Board_M5StickCPlus2::log(const char* message) {
    // Output to serial (always)
    Serial.println(message);

    // Add to screen log
    addLogLine(message);
}

void Board_M5StickCPlus2::logf(const char* format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    log(buffer);
}

size_t Board_M5StickCPlus2::writeAudio(const uint8_t* data, size_t size) {
    if (size == 0) return 0;

    // M5.Speaker.playRaw expects int16_t samples
    size_t samples = size / sizeof(int16_t);

    // playRaw parameters: (data, samples, sample_rate, stereo, repeat_count, channel)
    // PAM8303 speaker via I2S on GPIO0 (M5Unified handles routing)
    bool success = M5.Speaker.playRaw((const int16_t*)data, samples, m_sampleRate, false, 1, -1);

    return success ? size : 0;
}

size_t Board_M5StickCPlus2::readAudio(uint8_t* data, size_t size) {
    if (size == 0) return 0;

    size_t samplesToRead = size / sizeof(int16_t);
    if (samplesToRead > MIC_BUFFER_SAMPLES) {
        samplesToRead = MIC_BUFFER_SAMPLES;
    }

    // Record synchronously (blocking)
    // SPM1423 PDM mic via GPIO34/GPIO0 (M5Unified handles multiplexing with speaker)
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

void Board_M5StickCPlus2::setSampleRate(int rate) {
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
