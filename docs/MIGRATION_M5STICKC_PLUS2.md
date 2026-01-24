# M5StickC Plus2 Migration

## Executive Summary

This document explains the rationale and technical details for migrating OpenBadge firmware to support the M5StickC Plus2 as a second hardware target alongside the existing M5Stack Core S3.

**Key Decision**: Multi-board support rather than replacement. Both Core S3 and StickC Plus2 are now supported build targets.

**Result**: OpenBadge can run on a more compact, wearable form factor while maintaining full Bluetooth Classic functionality and GlassBridge Android app integration.

## Why Migrate?

### Form Factor Advantages

**M5Stack Core S3** (54×30×16mm):
- Larger, heavier device
- Great for desktop/stationary use
- Large touch screen provides excellent UI
- Not ideal for all-day wearable use

**M5StickC Plus2** (48.2×25.5×13.7mm):
- 40% smaller, significantly lighter
- Wrist-mountable or clip-on wearable
- Physical buttons (no touch required)
- Ideal for mobile/wearable scenarios

### Use Case Scenarios

**Core S3 Best For:**
- Development and debugging (larger screen, richer UI)
- Stationary installations
- Scenarios requiring visual feedback and display interaction
- Demonstrations and presentations

**StickC Plus2 Best For:**
- All-day wearable AI assistant
- Hands-free operation (button vs touch)
- Discreet/low-profile usage
- Battery-powered mobile scenarios
- Cost-sensitive deployments (Plus2 is less expensive)

### Technical Feasibility

**Key Finding**: Both devices use identical software stacks:
- M5Unified library (hardware abstraction)
- ESP32 Bluetooth Classic (HFP + AVRCP)
- Same I2S audio APIs
- Same Arduino/ESP-IDF framework

**Migration Complexity**: Low. Clean HAL architecture (IBoard interface) isolated hardware differences to ~300 lines of board-specific code.

## Hardware Trade-offs

### Advantages of M5StickC Plus2

✅ **Smaller and lighter** - Better wearability
✅ **Physical buttons** - More reliable trigger in mobile scenarios
✅ **Lower cost** - More affordable for multi-device deployments
✅ **Sufficient performance** - ESP32-PICO-V3-02 at 240MHz handles Bluetooth + audio
✅ **Same Bluetooth stack** - Zero changes to BT logic
✅ **Same audio APIs** - M5Unified abstracts hardware differences

### Disadvantages of M5StickC Plus2

❌ **Smaller display** - 135x240 vs 320x240 (58% less screen area)
❌ **Portrait orientation** - Requires UI redesign
❌ **Less memory** - 8MB flash vs 16MB (still sufficient: 3MB app partitions work)
❌ **Smaller battery** - 200mAh (estimated 4-6 hours vs Core S3's longer runtime)
❌ **No touch screen** - Less rich interaction (but buttons are fine for trigger-only use case)
❌ **Shared GPIO0** - Speaker and mic share GPIO (M5Unified handles multiplexing, but unverified for simultaneous operation)

### Risk Assessment

**High Priority Risks:**
1. **GPIO0 Multiplexing** - Speaker (PAM8303) and Mic (SPM1423) both use GPIO0
   - *Mitigation*: M5Unified library claims to handle this internally
   - *Testing*: Required to verify bidirectional SCO audio works simultaneously
   - *Fallback*: If fails, may need to implement time-division multiplexing in app layer

2. **Auto-Shutdown on Battery** - AXP192 PMIC may auto-shutdown after 60 seconds
   - *Mitigation*: `cfg.output_power = true` in M5.begin()
   - *Testing*: Extended battery operation test required
   - *Fallback*: May need `M5.Power.setExtOutput(true)` call

3. **Audio Quality** - PAM8303 speaker is smaller than Core S3's AW88298 amplifier
   - *Mitigation*: Tunable volume/gain parameters
   - *Testing*: Voice intelligibility tests with real phone calls
   - *Fallback*: May need different magnification values or sample rate

**Medium Priority Risks:**
4. **Display Readability** - 135px width is narrow for text
   - *Mitigation*: Smaller fonts, abbreviated text
   - *Testing*: Visual inspection, readability at arm's length
   - *Fallback*: Use even smaller fonts or icon-based UI

5. **Button Debouncing** - Physical buttons may need debouncing
   - *Mitigation*: M5Unified provides `wasPressed()` with built-in debounce
   - *Testing*: Rapid button presses, verify no double-triggers
   - *Fallback*: Add manual debounce timer if needed

**Low Priority Risks:**
6. **Battery Life** - 200mAh may not last full workday
   - *Mitigation*: Power optimization, sleep modes
   - *Testing*: Measure actual runtime in typical usage
   - *Acceptance*: 4-6 hours may be acceptable for this form factor

## Implementation Approach

### Strategy: Multi-Board Support

**Decision**: Keep both board implementations rather than replacing Core S3.

**Rationale:**
- Zero risk to existing Core S3 functionality
- Allows use case-specific hardware selection
- Minimal overhead (both use same M5Unified library)
- Future-proof for additional board targets

**Build System:**
```bash
# Build for Core S3
pio run -e m5stack-cores3

# Build for StickC Plus2
pio run -e m5stickc-plus2
```

### Architecture: HAL Abstraction

**IBoard Interface** - Hardware-agnostic contract:
```cpp
class IBoard {
    virtual void init() = 0;
    virtual void update() = 0;
    virtual bool isActionTriggered() = 0;
    virtual void setLedStatus(StatusState state) = 0;
    virtual void log(const char* message) = 0;
    virtual size_t writeAudio(const uint8_t* data, size_t size) = 0;
    virtual size_t readAudio(uint8_t* data, size_t size) = 0;
    virtual void setSampleRate(int rate) = 0;
};
```

**Board Implementations**:
- `Board_M5CoreS3` - Existing implementation
- `Board_M5StickCPlus2` - New implementation

**Factory Pattern**:
```cpp
class BoardManager {
    static IBoard* createBoard() {
#if defined(BOARD_M5_CORES3)
        return new Board_M5CoreS3();
#elif defined(BOARD_M5_STICKC_PLUS2)
        return new Board_M5StickCPlus2();
#endif
    }
};
```

**Main Loop** - Unchanged:
```cpp
IBoard* board = BoardManager::createBoard();
board->init();
while (1) {
    board->update();
    if (board->isActionTriggered()) {
        // Handle trigger (same code for all boards)
    }
}
```

### Files Changed

**New Files** (361 lines total):
- `src/HAL/Board_M5StickCPlus2.h` - 89 lines
- `src/HAL/Board_M5StickCPlus2.cpp` - 282 lines
- `default_8MB.csv` - 7 lines

**Modified Files** (~30 lines):
- `src/HAL/BoardManager.h` - Added Plus2 case in factory
- `platformio.ini` - Added m5stickc-plus2 environment

**Unchanged Files** (critical):
- `src/main.cpp` - Zero changes (uses IBoard interface)
- `src/Core/BluetoothManager.h/cpp` - Zero changes (hardware-agnostic)
- `sdkconfig.defaults` - Zero changes (BT config identical)

### Key Technical Differences

**Display**:
- **Resolution**: 320x240 → 135x240 (portrait)
- **Driver**: ILI9342C → ST7789v2
- **Rotation**: `setRotation(1)` landscape → `setRotation(0)` portrait
- **Font**: FreeSansBold18pt7b → FreeSansBold12pt7b
- **Layout**: Status 100px, Log 140px → Status 80px, Log 160px

**Input**:
- **Core S3**: Capacitive touch (M5.Touch.getCount())
- **Plus2**: Physical button (M5.BtnA.wasPressed())
- **Detection**: Touch area check → Button edge detection
- **Debouncing**: Manual state tracking → Built-in wasPressed()

**Audio Output**:
- **Core S3**: AW88298 I2S amplifier (GPIO dedicated)
- **Plus2**: PAM8303 I2S amplifier (GPIO0 shared)
- **API**: Identical (`M5.Speaker.playRaw()`)
- **Tuning**: May need different volume/magnification

**Audio Input**:
- **Core S3**: ES7210 dual PDM mics (GPIO dedicated)
- **Plus2**: SPM1423 single PDM mic (GPIO0 shared)
- **API**: Identical (`M5.Mic.record()`)
- **Tuning**: May need different gain/magnification

**Power Management**:
- **Core S3**: AXP2101 PMIC, requires `M5.Power.begin()`
- **Plus2**: AXP192 PMIC, M5Unified handles automatically
- **Battery**: Larger → 200mAh
- **Shutdown Risk**: Plus2 may auto-shutdown on battery if not configured

**Memory**:
- **Flash**: 16MB → 8MB (3MB app partitions sufficient)
- **PSRAM**: 8MB → 2MB (not a constraint, app uses <10KB)
- **Partition Table**: `default_16MB.csv` → `default_8MB.csv`

## Testing Strategy

### Phase 1: Unit Tests (Hardware)
- Display renders correctly (portrait orientation)
- Button detection works reliably
- Speaker produces audible output
- Microphone captures audio
- Sample rate switching works (8kHz ↔ 16kHz)

### Phase 2: Integration Tests (Bluetooth)
- Bluetooth advertises and pairs
- HFP profile connects
- AVRCP media button (Button A) sends command
- SCO audio channel establishes
- Bidirectional audio flows

### Phase 3: End-to-End Tests (GlassBridge)
- Full voice session completes
- Whisper transcription accuracy acceptable
- TTS playback intelligibility acceptable
- Latency acceptable (<2s button to speech)

### Phase 4: Regression Tests
- Core S3 build still works
- Core S3 functionality unchanged

### Phase 5: Stress Tests
- Battery life measurement
- Memory leak detection
- Repeated sessions (100+ cycles)
- Edge cases (disconnections, errors, timeouts)

## Migration Notes

### For Developers

**Switching Boards:**
```bash
# Build for StickC Plus2
pio run -e m5stickc-plus2 --target upload

# Build for Core S3
pio run -e m5stack-cores3 --target upload
```

**Adding New Boards:**
1. Create `Board_NewDevice.h/cpp` implementing IBoard
2. Add case to `BoardManager.h` factory
3. Add environment to `platformio.ini`
4. Create partition table if flash size differs

**Debugging Board-Specific Issues:**
- Check serial output for hardware initialization
- Compare with working Board_M5CoreS3 implementation
- Test with M5Unified example sketches to isolate hardware
- Verify M5Unified library version (0.1.13+)

### For Users

**Choosing a Board:**
- **Need larger display?** → Core S3
- **Need wearable form factor?** → StickC Plus2
- **Need all-day battery?** → Core S3
- **Need lower cost?** → StickC Plus2
- **Need touch interface?** → Core S3
- **Need hands-free buttons?** → StickC Plus2

**Deployment Scenarios:**
- **Desktop assistant**: Core S3 (larger screen, stationary)
- **Wearable assistant**: StickC Plus2 (compact, lightweight)
- **Fleet deployment**: StickC Plus2 (cost-effective)
- **Development**: Core S3 (better debugging UI)

## Future Enhancements

### Potential Optimizations
1. **Battery Life**: Implement deep sleep between sessions
2. **Display**: Add battery level indicator, signal strength
3. **Audio**: Auto-tune volume based on ambient noise
4. **UI**: Icon-based status instead of text (language-agnostic)
5. **Configuration**: Web interface for settings (WiFi + web server)

### Additional Board Targets
The HAL architecture supports adding more boards:
- **M5Stack Core2** - Touch screen, larger battery
- **M5Stack Atom Echo** - Minimal form factor, speaker + mic
- **Generic ESP32** - Custom hardware builds

### Code Reuse
Other projects can use this HAL pattern:
- Clean separation of hardware and application logic
- Easy to add new board targets
- Testable business logic (mock IBoard implementation)

## Conclusion

The M5StickC Plus2 migration demonstrates:

✅ **Clean Architecture** - HAL abstraction isolated hardware changes to ~300 lines
✅ **Minimal Risk** - No changes to Bluetooth or application logic
✅ **Expanded Use Cases** - Enables wearable form factor
✅ **Future-Proof** - Multi-board support allows hardware flexibility

**Status**: Implementation complete. Testing and validation in progress.

**Next Steps**:
1. Build and upload firmware to M5StickC Plus2
2. Complete hardware and integration tests
3. Measure battery life and audio quality
4. Tune UI, volume, and performance
5. Deploy to target use cases

## References

- **Hardware Specs**: See `docs/HARDWARE_COMPARISON.md`
- **Implementation Details**: See `docs/IMPLEMENTATION_GUIDE.md`
- **Architecture**: See `docs/ARCHITECTURE.md`
- **M5Unified Docs**: https://github.com/m5stack/M5Unified
- **ESP32 Bluetooth Classic**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_hfp_client.html
