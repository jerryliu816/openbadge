#include "BluetoothManager.h"
#include <Arduino.h>

// ESP-IDF Bluetooth headers
extern "C" {
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_hf_client_api.h"
#include "esp_avrc_api.h"
}

// Global instance for C callbacks
BluetoothManager* g_btManager = nullptr;

// Helper macro for logging (uses board if available, falls back to Serial)
#define BT_LOG(msg) do { \
    if (g_btManager && g_btManager->getBoard()) { \
        g_btManager->getBoard()->log(msg); \
    } else { \
        Serial.println(msg); \
    } \
} while(0)

#define BT_LOGF(fmt, ...) do { \
    if (g_btManager && g_btManager->getBoard()) { \
        g_btManager->getBoard()->logf(fmt, ##__VA_ARGS__); \
    } else { \
        Serial.printf(fmt "\n", ##__VA_ARGS__); \
    } \
} while(0)

// ============================================================
// C CALLBACK WRAPPERS
// ============================================================

static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT:
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                BT_LOGF("[GAP] Auth OK: %s", param->auth_cmpl.device_name);
            } else {
                BT_LOGF("[GAP] Auth failed: %d", param->auth_cmpl.stat);
            }
            break;

        case ESP_BT_GAP_PIN_REQ_EVT:
            BT_LOG("[GAP] PIN request - using 0000");
            {
                esp_bt_pin_code_t pin = {'0', '0', '0', '0'};
                esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin);
            }
            break;

        case ESP_BT_GAP_CFM_REQ_EVT:
            BT_LOGF("[GAP] Confirm: %d", param->cfm_req.num_val);
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;

        case ESP_BT_GAP_KEY_NOTIF_EVT:
            BT_LOGF("[GAP] Passkey: %d", param->key_notif.passkey);
            break;

        case ESP_BT_GAP_MODE_CHG_EVT:
            BT_LOGF("[GAP] Mode: %d", param->mode_chg.mode);
            break;

        default:
            break;
    }
}

static void hf_client_callback(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t* param) {
    if (!g_btManager) return;

    switch (event) {
        case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
            g_btManager->handleConnectionState(param->conn_stat.state, param->conn_stat.remote_bda);
            break;

        case ESP_HF_CLIENT_AUDIO_STATE_EVT:
            g_btManager->handleAudioState(param->audio_stat.state);
            break;

        case ESP_HF_CLIENT_BVRA_EVT:
            BT_LOGF("[HFP] Voice recog: %d", param->bvra.value);
            break;

        case ESP_HF_CLIENT_VOLUME_CONTROL_EVT:
            BT_LOGF("[HFP] Vol: %d", param->volume_control.volume);
            break;

        case ESP_HF_CLIENT_CIND_CALL_EVT:
            BT_LOGF("[HFP] Call: %d", param->call.status);
            break;

        case ESP_HF_CLIENT_RING_IND_EVT:
            BT_LOG("[HFP] Ring!");
            break;

        default:
            break;
    }
}

static void hf_client_incoming_data_callback(const uint8_t* data, uint32_t len) {
    if (g_btManager) {
        g_btManager->handleIncomingAudio(data, len);
    }
}

static uint32_t hf_client_outgoing_data_callback(uint8_t* data, uint32_t len) {
    if (g_btManager) {
        return g_btManager->handleOutgoingAudio(data, len);
    }
    return 0;
}

static void avrc_ct_callback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param) {
    switch (event) {
        case ESP_AVRC_CT_CONNECTION_STATE_EVT:
            BT_LOGF("[AVRCP] Connected: %d", param->conn_stat.connected);
            break;

        case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
            BT_LOGF("[AVRCP] Key 0x%02X resp", param->psth_rsp.key_code);
            break;

        case ESP_AVRC_CT_REMOTE_FEATURES_EVT:
            BT_LOG("[AVRCP] Remote features OK");
            break;

        default:
            break;
    }
}

// ============================================================
// INITIALIZATION
// ============================================================

void BluetoothManager::init(const char* deviceName, IBoard* board) {
    m_board = board;
    g_btManager = this;

    m_board->log("==== Bluetooth Init ====");

    initNvs();
    initController();
    initBluedroid();

    // Set device name
    esp_bt_dev_set_device_name(deviceName);
    m_board->logf("Device: %s", deviceName);

    // Register GAP callback
    esp_bt_gap_register_callback(gap_callback);

    // Set SSP (Secure Simple Pairing) mode
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;  // Just Works pairing
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

    initHfpClient();
    initAvrcpController();
    setDiscoverable();

    m_board->log("==== BT Ready ====");
}

void BluetoothManager::initNvs() {
    m_board->log("NVS init...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        m_board->log("NVS erase...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    m_board->log("NVS OK");
}

void BluetoothManager::initController() {
    m_board->log("BT controller init...");

    // Release BLE memory (we only use Classic BT)
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    // Note: mode field removed in newer ESP-IDF versions
    // Mode is set via BT_CONTROLLER_INIT_CONFIG_DEFAULT() and controller_mem_release()

    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        m_board->logf("Controller fail: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        m_board->logf("Enable fail: %s", esp_err_to_name(ret));
        return;
    }

    m_board->log("BT controller OK");
}

void BluetoothManager::initBluedroid() {
    m_board->log("Bluedroid init...");

    esp_err_t ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        m_board->logf("Bluedroid fail: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        m_board->logf("Enable fail: %s", esp_err_to_name(ret));
        return;
    }

    m_board->log("Bluedroid OK");
}

void BluetoothManager::initHfpClient() {
    m_board->log("HFP Client init...");

    // Register HFP Client callback
    esp_hf_client_register_callback(hf_client_callback);

    // Initialize HFP Client
    esp_err_t ret = esp_hf_client_init();
    if (ret != ESP_OK) {
        m_board->logf("HFP fail: %s", esp_err_to_name(ret));
        return;
    }

    // Register audio data callbacks
    esp_hf_client_register_data_callback(
        hf_client_incoming_data_callback,
        hf_client_outgoing_data_callback
    );

    m_board->log("HFP Client OK");
}

void BluetoothManager::initAvrcpController() {
    m_board->log("AVRCP init...");

    esp_avrc_ct_register_callback(avrc_ct_callback);

    esp_err_t ret = esp_avrc_ct_init();
    if (ret != ESP_OK) {
        m_board->logf("AVRCP fail: %s", esp_err_to_name(ret));
        return;
    }

    m_board->log("AVRCP OK");
}

void BluetoothManager::setDiscoverable() {
    m_board->log("Setting discoverable...");

    // Set scan mode: connectable + discoverable
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    // Set Class of Device: Audio - Hands-Free
    esp_bt_cod_t cod = {0};
    cod.major = ESP_BT_COD_MAJOR_DEV_AV;          // Audio/Video device
    cod.minor = 0x04;                              // Hands-free
    cod.service = ESP_BT_COD_SRVC_AUDIO |         // Audio service
                  ESP_BT_COD_SRVC_RENDERING |     // Rendering service
                  ESP_BT_COD_SRVC_TELEPHONY;      // Telephony service
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);

    m_board->log("Discoverable!");
}

// ============================================================
// EVENT HANDLERS
// ============================================================

void BluetoothManager::handleConnectionState(uint8_t state, esp_bd_addr_t& addr) {
    switch (state) {
        case ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED:
            m_board->log("[HFP] Disconnected");
            m_slcConnected = false;
            m_scoConnected = false;
            m_board->setLedStatus(StatusState::Disconnected);
            break;

        case ESP_HF_CLIENT_CONNECTION_STATE_CONNECTING:
            m_board->log("[HFP] Connecting...");
            break;

        case ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED:
            m_board->logf("[HFP] Connected %02X:%02X",
                addr[4], addr[5]);  // Show last 2 bytes of MAC
            memcpy(m_peerAddr, addr, 6);
            break;

        case ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED:
            m_board->log("[HFP] SLC Ready");
            m_slcConnected = true;
            m_board->setLedStatus(StatusState::Idle);
            break;

        case ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTING:
            m_board->log("[HFP] Disconnecting...");
            break;
    }
}

void BluetoothManager::handleAudioState(uint8_t state) {
    switch (state) {
        case ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED:
            m_board->log("[SCO] Disconnected");
            m_scoConnected = false;
            if (m_slcConnected) {
                m_board->setLedStatus(StatusState::Idle);
            }
            break;

        case ESP_HF_CLIENT_AUDIO_STATE_CONNECTING:
            m_board->log("[SCO] Connecting...");
            break;

        case ESP_HF_CLIENT_AUDIO_STATE_CONNECTED:
            m_board->log("[SCO] CVSD 8kHz");
            m_scoConnected = true;
            m_wideband = false;
            m_board->setSampleRate(8000);
            break;

        case ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC:
            m_board->log("[SCO] mSBC 16kHz");
            m_scoConnected = true;
            m_wideband = true;
            m_board->setSampleRate(16000);
            break;
    }
}

void BluetoothManager::handleIncomingAudio(const uint8_t* data, uint32_t len) {
    // Phone -> Speaker
    if (m_board && len > 0) {
        m_board->writeAudio(data, len);
    }
}

uint32_t BluetoothManager::handleOutgoingAudio(uint8_t* data, uint32_t len) {
    // Mic -> Phone
    if (m_board && len > 0) {
        return m_board->readAudio(data, len);
    }
    return 0;
}

// ============================================================
// TRIGGER COMMANDS
// ============================================================

void BluetoothManager::sendMediaButton() {
    if (!m_slcConnected) {
        m_board->log("Not connected!");
        return;
    }

    m_board->log("Sending AVRCP Play...");

    // Send key press
    esp_avrc_ct_send_passthrough_cmd(
        0,                              // Transaction label
        ESP_AVRC_PT_CMD_PLAY,           // Play command
        ESP_AVRC_PT_CMD_STATE_PRESSED   // Key pressed
    );

    vTaskDelay(pdMS_TO_TICKS(100));

    // Send key release
    esp_avrc_ct_send_passthrough_cmd(
        0,
        ESP_AVRC_PT_CMD_PLAY,
        ESP_AVRC_PT_CMD_STATE_RELEASED
    );

    m_board->log("AVRCP sent");
}

void BluetoothManager::sendHfpButton() {
    if (!m_slcConnected) {
        m_board->log("Not connected!");
        return;
    }

    m_board->log("Sending HFP button...");
    // Note: esp_hf_client_send_key_pressed() doesn't exist in ESP-IDF API
    // Alternative: Use sendBvra() or sendMediaButton() instead
    // For now, using voice recognition activation as alternative
    esp_hf_client_start_voice_recognition();
    m_board->log("HFP sent");
}

void BluetoothManager::sendBvra() {
    if (!m_slcConnected) {
        m_board->log("Not connected!");
        return;
    }

    m_board->log("Sending BVRA activate...");
    // AT+BVRA=1 activates voice recognition on the phone
    // Note: Not all phones support this command
    esp_hf_client_start_voice_recognition();
    m_board->log("BVRA sent");
}

bool BluetoothManager::canTrigger() {
    if (!m_slcConnected) {
        m_board->log("Cannot trigger - not connected");
        return false;
    }

    if (m_scoConnected) {
        m_board->log("Session active - ignoring trigger");
        return false;
    }

    return true;
}

void BluetoothManager::update() {
    // Event processing happens in callbacks
}
