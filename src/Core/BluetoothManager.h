#pragma once

#include "../HAL/IBoard.h"
#include <cstdint>

// Forward declare ESP-IDF types to avoid including C headers in header
typedef uint8_t esp_bd_addr_t[6];

/**
 * Bluetooth Manager
 *
 * Handles HFP (Hands-Free Profile) and AVRCP (Audio/Video Remote Control Profile)
 * for communication with the GlassBridge Android app.
 *
 * Responsibilities:
 * - Initialize ESP-IDF Bluetooth Classic stack
 * - Manage HFP Client connection and SCO audio link
 * - Send AVRCP media button commands to trigger GlassBridge
 * - Route audio between SCO link and board I2S
 */
class BluetoothManager {
public:
    /**
     * Initialize Bluetooth stack
     * @param deviceName Name advertised during discovery (e.g., "OpenBadge")
     * @param board Pointer to hardware abstraction for audio routing
     */
    void init(const char* deviceName, IBoard* board);

    /**
     * Called every loop iteration to process events
     */
    void update();

    /**
     * Send AVRCP Play/Pause command to trigger GlassBridge
     * This sends KEYCODE_MEDIA_PLAY_PAUSE to Android
     */
    void sendMediaButton();

    /**
     * Alternative 1: Send HFP button press (KEYCODE_HEADSETHOOK)
     * Use if AVRCP doesn't work on specific Android versions
     */
    void sendHfpButton();

    /**
     * Alternative 2: Send HFP Voice Recognition Activation (AT+BVRA=1)
     * Cleanest HFP-only approach - no A2DP dependency
     */
    void sendBvra();

    /**
     * Check if trigger is allowed (debounce, state validation)
     * @return true if trigger should be processed
     */
    bool canTrigger();

    // Connection state queries
    bool isConnected() const { return m_slcConnected; }
    bool isScoConnected() const { return m_scoConnected; }
    bool isWidebandActive() const { return m_wideband; }

    // Get the board reference (for callbacks)
    IBoard* getBoard() { return m_board; }

    // Internal handlers called from C callbacks
    void handleConnectionState(uint8_t state, esp_bd_addr_t& addr);
    void handleAudioState(uint8_t state);
    void handleIncomingAudio(const uint8_t* data, uint32_t len);
    uint32_t handleOutgoingAudio(uint8_t* data, uint32_t len);

private:
    IBoard* m_board = nullptr;
    bool m_slcConnected = false;   // Service Level Connection (HFP control channel)
    bool m_scoConnected = false;   // SCO audio link
    bool m_wideband = false;       // mSBC (true) or CVSD (false)
    uint8_t m_peerAddr[6] = {0};   // Connected device address

    void initNvs();
    void initController();
    void initBluedroid();
    void initHfpClient();
    void initAvrcpController();
    void setDiscoverable();
};

// Global instance pointer (needed for C callbacks)
extern BluetoothManager* g_btManager;
