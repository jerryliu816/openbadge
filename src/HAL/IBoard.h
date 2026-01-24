#pragma once

#include <cstdint>
#include <cstddef>

/**
 * Visual status states for UI feedback
 */
enum class StatusState {
    Disconnected,  // Gray  - "Not Connected"
    Idle,          // Blue  - "Tap to Speak"
    Listening,     // Red   - "Listening..."
    Speaking       // Green - "Speaking..."
};

/**
 * Hardware Abstraction Layer Interface
 *
 * Defines the contract for board-specific implementations.
 * Allows the Bluetooth/Core logic to remain hardware-agnostic.
 */
class IBoard {
public:
    virtual ~IBoard() = default;

    // ===== Lifecycle =====

    /**
     * Initialize all hardware (power, display, audio, touch)
     * MUST be called before any other method
     */
    virtual void init() = 0;

    /**
     * Called every loop iteration for input polling, display refresh
     * Should complete quickly (<5ms)
     */
    virtual void update() = 0;

    // ===== User Input =====

    /**
     * Returns true ONCE when user triggers the AI action
     * Must debounce internally - return true only on rising edge
     */
    virtual bool isActionTriggered() = 0;

    // ===== Visual Feedback =====

    /**
     * Update the status display section to reflect current state
     */
    virtual void setLedStatus(StatusState state) = 0;

    /**
     * Write a log message to the text section of the display
     * Also outputs to Serial for debugging
     * @param message The message to display (will be truncated if too long)
     */
    virtual void log(const char* message) = 0;

    /**
     * Printf-style logging to the text section
     * @param format Printf format string
     * @param ... Format arguments
     */
    virtual void logf(const char* format, ...) = 0;

    // ===== Audio Output (Phone -> Speaker) =====

    /**
     * Write PCM audio data to the speaker
     * @param data PCM 16-bit signed samples (little-endian)
     * @param size Number of bytes (not samples)
     * @return Number of bytes actually written
     */
    virtual size_t writeAudio(const uint8_t* data, size_t size) = 0;

    // ===== Audio Input (Mic -> Phone) =====

    /**
     * Read PCM audio data from the microphone
     * @param data Buffer to fill with PCM 16-bit signed samples
     * @param size Maximum bytes to read
     * @return Number of bytes actually read
     */
    virtual size_t readAudio(uint8_t* data, size_t size) = 0;

    // ===== Audio Configuration =====

    /**
     * Dynamically reconfigure I2S sample rate
     * Called when SCO codec is negotiated (8000 for CVSD, 16000 for mSBC)
     * @param rate Sample rate in Hz (8000 or 16000)
     */
    virtual void setSampleRate(int rate) = 0;
};
