#pragma once

#include "IBoard.h"

// Include board-specific implementations based on build flags
#if defined(BOARD_M5_CORES3)
    #include "Board_M5CoreS3.h"
#elif defined(BOARD_M5_STICKC_PLUS2)
    #include "Board_M5StickCPlus2.h"
#else
    #error "No board defined! Add -DBOARD_M5_CORES3 or -DBOARD_M5_STICKC_PLUS2 to build_flags in platformio.ini"
#endif

/**
 * Board Factory
 *
 * Creates the appropriate board implementation based on compile-time flags.
 * Add new boards here as #elif branches.
 *
 * Supported boards:
 * - BOARD_M5_CORES3: M5Stack Core S3 (ESP32-S3, 320x240 touch screen)
 * - BOARD_M5_STICKC_PLUS2: M5StickC Plus2 (ESP32-PICO-V3-02, 135x240 buttons)
 */
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
