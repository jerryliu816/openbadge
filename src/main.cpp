/**
 * OpenBadge Firmware
 *
 * ESP32-S3 based Bluetooth Hands-Free headset for GlassBridge AI Assistant
 *
 * Target Hardware: M5Stack CoreS3
 * Bluetooth Profiles: HFP 1.7 (Client), AVRCP 1.6 (Controller)
 *
 * Screen Layout:
 * ┌─────────────────────────────────┐
 * │      STATUS DISPLAY             │  Color + "Tap to Speak"
 * ├─────────────────────────────────┤
 * │      LOG TEXT                   │  Scrolling debug output
 * └─────────────────────────────────┘
 *
 * Workflow:
 * 1. Device boots and advertises as "OpenBadge"
 * 2. User pairs via Android Bluetooth settings
 * 3. GlassBridge app detects connected headset
 * 4. User taps STATUS section -> sends AVRCP Play/Pause
 * 5. Phone establishes SCO audio link
 * 6. Voice flows: Mic -> Phone -> OpenAI -> TTS -> Speaker
 */

// CRITICAL: Override Arduino's weak btInUse() to prevent BT memory release
// Arduino's initArduino() calls esp_bt_controller_mem_release(ESP_BT_MODE_BTDM)
// if btInUse() returns false, which prevents our Bluetooth init from working
extern "C" bool btInUse() { return true; }

#include <Arduino.h>
#include "HAL/BoardManager.h"
#include "Core/BluetoothManager.h"

// Global instances
IBoard* g_board = nullptr;
// g_btManager is declared in BluetoothManager.cpp

// Device name advertised over Bluetooth
static const char* DEVICE_NAME = "OpenBadge";

void setup() {
    // Initialize serial for debugging (also shown on screen)
    Serial.begin(115200);
    delay(500);  // Brief delay for serial

    // Initialize hardware abstraction layer first
    // This sets up the display so we can show logs
    g_board = BoardManager::createBoard();
    g_board->init();

    // Allocate Bluetooth manager
    g_btManager = new BluetoothManager();

    // Now initialize Bluetooth (logs will appear on screen)
    g_btManager->init(DEVICE_NAME, g_board);

    g_board->log("Ready to pair!");
    g_board->log("Scan for 'OpenBadge'");
}

void loop() {
    // Update hardware (polls touch)
    g_board->update();

    // Update Bluetooth manager
    g_btManager->update();

    // Track SCO state changes for UI updates
    static bool lastScoState = false;
    bool currentScoState = g_btManager->isScoConnected();

    // Handle user trigger (Button A press)
    if (g_board->isActionTriggered()) {
        if (currentScoState) {
            // Push-to-talk: Button A pressed while SCO is active
            // This means user wants to STOP speaking
            g_board->log(">>> Stopping voice...");
            g_board->setLedStatus(StatusState::Idle);
            g_btManager->stopBvra();    // Send AT+BVRA=0 to end voice recognition
        } else if (g_btManager->canTrigger()) {
            // Button A pressed when idle - START speaking
            // Update UI to show we're activating
            g_board->setLedStatus(StatusState::Listening);

            // Try multiple trigger methods (AVRCP might not be connected)
            g_btManager->sendMediaButton();     // Try AVRCP Play/Pause first
            g_btManager->sendBvra();            // Also send HFP voice recognition (works without AVRCP)

            // Note: sendBvra() sends AT+BVRA=1 which tells the phone to start voice recognition
            // This works even if AVRCP isn't connected, since it uses HFP which IS connected
        }
        // canTrigger() logs the reason if it returns false
    }

    // Update UI based on SCO state changes
    if (currentScoState != lastScoState) {
        if (currentScoState) {
            // SCO just connected - voice session active
            g_board->setLedStatus(StatusState::Listening);
            g_board->log("Voice session started");
        } else {
            // SCO disconnected - session ended
            if (g_btManager->isConnected()) {
                g_board->setLedStatus(StatusState::Idle);
                g_board->log("Voice session ended");
            } else {
                g_board->setLedStatus(StatusState::Disconnected);
            }
        }
        lastScoState = currentScoState;
    }

    // Small delay to prevent tight loop
    delay(10);
}
