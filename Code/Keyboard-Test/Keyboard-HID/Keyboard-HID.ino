
#include <Wire.h>
#include <Keyboard.h>

// ============================================================
// USER CONFIGURATION
// ============================================================

// Set to 1 to print key press/release events to Serial.
// Set to 0 to disable key event messages.
#define DEBUG_KEY_EVENTS 1

#define ECE1117_ADDR 0x38

#define SERIAL_BAUD 115200

// Keyboard scan timing
#define KSO_SETTLE_US 500
#define DEBOUNCE_MS 8

// Your observed matrix rollover limit.
// More than five ordinary keys causes all ordinary key
// reports to be temporarily suppressed.
#define MAX_NONMODIFIER_KEYS 5

// ============================================================
// ECE1117 REGISTER DEFINITIONS
// ============================================================

#define REG_KSO_SELECT       0x40
#define REG_KSI_INPUT        0x41
#define REG_UPSTREAM_CONTROL 0xFA

// ============================================================
// KEYBOARD MATRIX CONFIGURATION
// ============================================================

// These are the physical KSO outputs.
// Array index = matrix row used in your recorded keymap.

const uint8_t ksoLines[] = {
    0, 1, 2, 3, 4, 5, 6,
    11, 12, 13, 14, 15, 16, 17,
    18, 19, 20, 21, 22
};

#define MATRIX_ROWS (sizeof(ksoLines) / sizeof(ksoLines[0]))
#define MATRIX_COLS 8

// Fn key is tracked separately.
// It is not sent as a normal USB HID key.

#define FN_ROW 5
#define FN_COL 4

// ============================================================
// USB HID KEYMAP
// ============================================================

// HID usage codes:
// Letters:       0x04 - 0x1D
// Numbers:       0x1E - 0x27
// Enter:         0x28
// Escape:        0x29
// Backspace:     0x2A
// Tab:           0x2B
// Space:         0x2C
// Punctuation:   0x2D - 0x38
// Caps Lock:     0x39
// F1-F12:        0x3A - 0x45
// Print Screen:  0x46
// Insert:        0x49
// Delete:        0x4C
// Arrows:        0x4F - 0x52
// Modifiers:     0xE0 - 0xE7

struct KeyMapEntry
{
    uint8_t row;
    uint8_t col;
    uint8_t usage;
    const char *name;
};

#define KEY_ENTRY(r, c, u, n) {r, c, u, n}

const KeyMapEntry keyMap[] = {

    // --------------------------------------------------------
    // Letters
    // --------------------------------------------------------

    KEY_ENTRY(3, 4, 0x04, "A"),
    KEY_ENTRY(0, 7, 0x05, "B"),
    KEY_ENTRY(2, 6, 0x06, "C"),
    KEY_ENTRY(2, 4, 0x07, "D"),
    KEY_ENTRY(2, 2, 0x08, "E"),
    KEY_ENTRY(0, 4, 0x09, "F"),
    KEY_ENTRY(0, 0, 0x0A, "G"),
    KEY_ENTRY(16, 0, 0x0B, "H"),
    KEY_ENTRY(15, 2, 0x0C, "I"),
    KEY_ENTRY(16, 4, 0x0D, "J"),
    KEY_ENTRY(15, 4, 0x0E, "K"),
    KEY_ENTRY(14, 4, 0x0F, "L"),
    KEY_ENTRY(16, 6, 0x10, "M"),
    KEY_ENTRY(16, 7, 0x11, "N"),
    KEY_ENTRY(14, 2, 0x12, "O"),
    KEY_ENTRY(13, 2, 0x13, "P"),
    KEY_ENTRY(3, 2, 0x14, "Q"),
    KEY_ENTRY(0, 2, 0x15, "R"),
    KEY_ENTRY(1, 4, 0x16, "S"),
    KEY_ENTRY(0, 1, 0x17, "T"),
    KEY_ENTRY(16, 2, 0x18, "U"),
    KEY_ENTRY(0, 6, 0x19, "V"),
    KEY_ENTRY(1, 2, 0x1A, "W"),
    KEY_ENTRY(1, 6, 0x1B, "X"),
    KEY_ENTRY(16, 1, 0x1C, "Y"),
    KEY_ENTRY(3, 6, 0x1D, "Z"),

    // --------------------------------------------------------
    // Number row
    // --------------------------------------------------------

    KEY_ENTRY(3, 3, 0x35, "Grave/Tilde"),
    KEY_ENTRY(3, 5, 0x1E, "1"),
    KEY_ENTRY(1, 5, 0x1F, "2"),
    KEY_ENTRY(2, 5, 0x20, "3"),
    KEY_ENTRY(0, 5, 0x21, "4"),
    KEY_ENTRY(0, 3, 0x22, "5"),
    KEY_ENTRY(16, 3, 0x23, "6"),
    KEY_ENTRY(16, 5, 0x24, "7"),
    KEY_ENTRY(15, 5, 0x25, "8"),
    KEY_ENTRY(14, 5, 0x26, "9"),
    KEY_ENTRY(13, 5, 0x27, "0"),
    KEY_ENTRY(13, 3, 0x2D, "Minus"),
    KEY_ENTRY(15, 3, 0x2E, "Equals"),

    // --------------------------------------------------------
    // Main keyboard keys
    // --------------------------------------------------------

    KEY_ENTRY(3, 0, 0x29, "Escape"),
    KEY_ENTRY(3, 1, 0x2B, "Tab"),
    KEY_ENTRY(12, 1, 0x2A, "Backspace"),
    KEY_ENTRY(13, 1, 0x2F, "Left Bracket"),
    KEY_ENTRY(15, 1, 0x30, "Right Bracket"),
    KEY_ENTRY(12, 4, 0x31, "Backslash"),
    KEY_ENTRY(1, 1, 0x39, "Caps Lock"),
    KEY_ENTRY(13, 4, 0x33, "Semicolon"),
    KEY_ENTRY(13, 0, 0x34, "Apostrophe"),
    KEY_ENTRY(12, 6, 0x28, "Enter"),

    // --------------------------------------------------------
    // Function keys
    // --------------------------------------------------------

    KEY_ENTRY(1, 3, 0x3A, "F1"),
    KEY_ENTRY(2, 3, 0x3B, "F2"),
    KEY_ENTRY(2, 1, 0x3C, "F3"),
    KEY_ENTRY(2, 0, 0x3D, "F4"),
    KEY_ENTRY(12, 0, 0x3E, "F5"),
    KEY_ENTRY(15, 0, 0x3F, "F6"),
    KEY_ENTRY(14, 1, 0x40, "F7"),
    KEY_ENTRY(14, 3, 0x41, "F8"),
    KEY_ENTRY(12, 3, 0x42, "F9"),
    KEY_ENTRY(12, 5, 0x43, "F10"),
    KEY_ENTRY(11, 5, 0x44, "F11"),
    KEY_ENTRY(10, 5, 0x45, "F12"),

    // --------------------------------------------------------
    // Navigation keys
    // --------------------------------------------------------

    KEY_ENTRY(8, 5, 0x46, "Print Screen"),
    KEY_ENTRY(10, 3, 0x49, "Insert"),
    KEY_ENTRY(11, 3, 0x4C, "Delete"),
    KEY_ENTRY(9, 7, 0x50, "Left Arrow"),
    KEY_ENTRY(10, 7, 0x4F, "Right Arrow"),
    KEY_ENTRY(9, 0, 0x52, "Up Arrow"),
    KEY_ENTRY(11, 7, 0x51, "Down Arrow"),

    // --------------------------------------------------------
    // Punctuation
    // --------------------------------------------------------

    KEY_ENTRY(15, 6, 0x36, "Comma"),
    KEY_ENTRY(14, 6, 0x37, "Period"),
    KEY_ENTRY(13, 7, 0x38, "Slash"),

    // --------------------------------------------------------
    // Modifiers
    // --------------------------------------------------------

    KEY_ENTRY(6, 1, 0xE1, "Left Shift"),
    KEY_ENTRY(6, 6, 0xE5, "Right Shift"),
    KEY_ENTRY(7, 3, 0xE0, "Left Ctrl"),
    KEY_ENTRY(7, 6, 0xE4, "Right Ctrl"),
    KEY_ENTRY(4, 5, 0xE3, "Windows"),
    KEY_ENTRY(8, 0, 0xE2, "Left Alt"),
    KEY_ENTRY(8, 7, 0xE6, "Right Alt"),

    // --------------------------------------------------------
    // Remaining keys
    // --------------------------------------------------------

    KEY_ENTRY(12, 7, 0x2C, "Space")
};

#define KEYMAP_SIZE (sizeof(keyMap) / sizeof(keyMap[0]))


// ============================================================
// FN KEY STATE
// ============================================================

bool fnHeld = false;
bool fnLocked = false;

// Tracks whether a key was intercepted by the Fn layer.
// This ensures its release is handled correctly even if Fn
// is released before the other key.
bool fnConsumed[19][8] = {false};

// Tracks the Fn+Esc press/release sequence.
bool fnLockKeyDown = false;

// Consumer Control HID usage values.
#define CONSUMER_MUTE          0x00E2
#define CONSUMER_VOLUME_DOWN   0x00EA
#define CONSUMER_VOLUME_UP     0x00E9
#define CONSUMER_PREVIOUS      0x00B6
#define CONSUMER_PLAY_PAUSE    0x00CD
#define CONSUMER_NEXT          0x00B5
#define CONSUMER_SEARCH        0x0221


// ============================================================
// MATRIX STATE
// ============================================================

// Raw/candidate state is used for debounce.
// Stable state is the debounced keyboard state.

bool candidateState[MATRIX_ROWS][MATRIX_COLS] = {};
bool stableState[MATRIX_ROWS][MATRIX_COLS] = {};

uint32_t candidateSince[MATRIX_ROWS][MATRIX_COLS] = {};

// Tracks which keys have actually been sent to USB.

bool hidActive[MATRIX_ROWS][MATRIX_COLS] = {};

// Tracks Fn-handled keys so their release is routed
// to the same handler as their press.

bool rolloverActive = false;

// ============================================================
// SMBUS REGISTER ACCESS
// ============================================================


bool writeRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(ECE1117_ADDR);

    Wire.write(reg);
    Wire.write(value);

    uint8_t result = Wire.endTransmission();

#if DEBUG_KEY_EVENTS
    if (result != 0)
    {
        Serial.print("SMBus WRITE ERROR: Register 0x");
        Serial.print(reg, HEX);
        Serial.print(", error code: ");
        Serial.println(result);
    }
#endif

    return result == 0;
}

bool readRegister(uint8_t reg, uint8_t *value)
{
    Wire.beginTransmission(ECE1117_ADDR);
    Wire.write(reg);

    uint8_t result = Wire.endTransmission(false);

    if (result != 0)
    {
        Serial.print("SMBus READ ADDRESS ERROR: 0x");
        Serial.println(result);
        return false;
    }

    uint8_t received = Wire.requestFrom(
        ECE1117_ADDR, (uint8_t)1
    );

    if (received != 1)
    {
        Serial.println("SMBus READ: No data received.");
        return false;
    }

    *value = Wire.read();

    return true;
}


// ============================================================
// ECE1117 GPIO / KSO CONFIGURATION
// ============================================================

bool configureAdditionalKSO()
{
    // GPIO multiplexing registers for KSO18-KSO22.

    const uint8_t registers[] = {
        0x0B, // GPIO01 / KSO18
        0x0A, // GPIO00 / KSO19
        0x1D, // GPIO23 / KSO20
        0x1C, // GPIO22 / KSO21
        0x1B  // GPIO21 / KSO22
    };

    for (uint8_t i = 0; i < 5; i++)
    {
        uint8_t value;

        if (!readRegister(registers[i], &value))
            return false;

        // Set bits 7:6 to 01 for KSO function.
        // Preserve the other register bits.

        uint8_t newValue = (value & 0x3F) | 0x40;

        if (!writeRegister(registers[i], newValue))
            return false;
    }

    return true;
}

// ============================================================
// KEYBOARD INITIALIZATION
// ============================================================


bool initializeECE1117()
{
    Serial.println("Initializing ECE1117 SMBus...");

    // First ECE1117 access after power-up.
    if (!writeRegister(0xFA, 0x03))
    {
        Serial.println("SMBus initialization write failed.");
        return false;
    }

    Serial.println("SMBus interface configured.");

    // Verify communication using device ID.
    uint8_t deviceID;

    if (!readRegister(0xFC, &deviceID))
    {
        Serial.println("Device ID read failed.");
        return false;
    }

    Serial.print("Device ID: 0x");
    Serial.println(deviceID, HEX);

    if (deviceID != 0x43)
    {
        Serial.println("Unexpected device ID.");
        return false;
    }

    // Configure additional KSO outputs.
    if (!configureAdditionalKSO())
    {
        Serial.println("KSO configuration failed.");
        return false;
    }

    // Disable keyboard scanner initially.
    if (!writeRegister(0x40, 0x40))
    {
        Serial.println("Failed to disable keyboard scanner.");
        return false;
    }

    Serial.println("ECE1117 initialization successful.");

    return true;
}


// ============================================================
// KEYMAP LOOKUP
// ============================================================

const KeyMapEntry *findKey(uint8_t row, uint8_t col)
{
    for (uint8_t i = 0; i < KEYMAP_SIZE; i++)
    {
        if (keyMap[i].row == row &&
            keyMap[i].col == col)
        {
            return &keyMap[i];
        }
    }

    return nullptr;
}

const char *getKeyName(uint8_t row, uint8_t col)
{
    if (row == FN_ROW && col == FN_COL)
        return "Fn";

    const KeyMapEntry *entry = findKey(row, col);

    if (entry != nullptr)
        return entry->name;

    return "Unknown";
}

// ============================================================
// CAPS LOCK LED INITIALIZATION
// PWM2 (GPIO13), active-low
// ============================================================

volatile bool hostCapsLock = false;
volatile bool capsLockLEDUpdatePending = false;

void keyboardLEDCallback(
    bool numlock,
    bool capslock,
    bool scrolllock,
    bool compose,
    bool kana,
    void *cbData)
{
    hostCapsLock = capslock;
    capsLockLEDUpdatePending = true;

#if DEBUG_KEY_EVENTS
    Serial.print("Host Caps Lock: ");
    Serial.println(hostCapsLock ? "ON" : "OFF");
#endif
}

#define CAPS_LED_CONTROL_REG 0x68
#define CAPS_LED_GPIO_CONFIG 0x15
#define CAPS_LED_GPIO_MUX    0x40

void initializeCapsLockLED()
{
    uint8_t gpioConfig;

    // Read current GPIO13 configuration.
    if (!readRegister(CAPS_LED_GPIO_CONFIG, &gpioConfig))
    {
        Serial.println("Failed to read Caps Lock LED GPIO configuration.");
        return;
    }

    // Configure GPIO13 for PWM2.
    gpioConfig = (gpioConfig & 0x3F) | CAPS_LED_GPIO_MUX;

    if (!writeRegister(CAPS_LED_GPIO_CONFIG, gpioConfig))
    {
        Serial.println("Failed to configure Caps Lock LED PWM2.");
        return;
    }

    // Initialize LED to match the current host Caps Lock state.
    setCapsLockLED(hostCapsLock);

    Serial.println("Caps Lock LED initialized.");
}

void setCapsLockLED(bool enabled)
{
    // PWM2 controls the Caps Lock LED.
    // Active-low behavior.
    writeRegister(CAPS_LED_CONTROL_REG, enabled ? 0x00 : 0x03);
}

void updateCapsLockLED()
{
    static bool lastLEDState = false;

    if (!capsLockLEDUpdatePending)
        return;

    // Clear the pending flag.
    capsLockLEDUpdatePending = false;

    // Only update the hardware if the state actually changed.
    if (hostCapsLock == lastLEDState)
        return;

    setCapsLockLED(hostCapsLock);

    lastLEDState = hostCapsLock;
}

// ============================================================
// USB HID KEYCODE CONVERSION
// ============================================================

// Earle Philhower's built-in Keyboard library accepts:
//   0x80-0x87: modifier keys
//   136 + HID usage: non-printing/raw HID keycodes
//
// We use raw HID usages rather than ASCII characters.
// This lets the physical Shift keys control capitalization
// and punctuation normally.

uint8_t convertHIDUsage(uint8_t usage)
{
    if (usage >= 0xE0 && usage <= 0xE7)
    {
        return 0x80 + (usage - 0xE0);
    }

    return 136 + usage;
}

bool isModifier(uint8_t usage)
{
    return usage >= 0xE0 && usage <= 0xE7;
}

// ============================================================
// SERIAL DEBUGGING
// ============================================================

void printKeyEvent(uint8_t row, uint8_t col, bool pressed)
{
#if DEBUG_KEY_EVENTS

    Serial.print(getKeyName(row, col));

    Serial.print(pressed ? " PRESSED" : " RELEASED");

    Serial.print(" | Matrix: ");
    Serial.print(row);
    Serial.print(",");
    Serial.print(col);

    Serial.print(" | KSO");
    Serial.print(ksoLines[row]);

    Serial.print(" / KSI");
    Serial.println(col);

#endif
}

// ============================================================
// KEY DEBOUNCING
// ============================================================

void updateMatrixState(uint8_t row, uint8_t col, bool rawPressed)
{
    uint32_t now = millis();

    // Raw state changed. Start debounce timer.

    if (rawPressed != candidateState[row][col])
    {
        candidateState[row][col] = rawPressed;
        candidateSince[row][col] = now;
    }

    // Accept the new state after remaining stable.

    if (candidateState[row][col] != stableState[row][col])
    {
        if ((uint32_t)(now - candidateSince[row][col]) >= DEBOUNCE_MS)
        {
            stableState[row][col] = candidateState[row][col];

            printKeyEvent(row, col, stableState[row][col]);
        }
    }
}

// ============================================================
// KEYBOARD MATRIX SCANNING
// ============================================================

void scanKeyboardMatrix()
{
    for (uint8_t row = 0; row < MATRIX_ROWS; row++)
    {
        uint8_t ksi;

        // Select one KSO output.

        if (!writeRegister(REG_KSO_SELECT, ksoLines[row]))
            continue;

        delayMicroseconds(KSO_SETTLE_US);

        // Read the eight KSI inputs.

        if (!readRegister(REG_KSI_INPUT, &ksi))
            continue;

        for (uint8_t col = 0; col < MATRIX_COLS; col++)
        {
            // KSI inputs are active low.

            bool pressed = !(ksi & (1 << col));

            updateMatrixState(row, col, pressed);
        }
    }

    // Disable scanner after completing the matrix scan.

    writeRegister(REG_KSO_SELECT, 0x40);
}

// ============================================================
// KEYBOARD BACKLIGHT CONTROL
// PWM1 (GPIO12)
// ============================================================

#define BACKLIGHT_CONTROL_REG  0x60
#define BACKLIGHT_DUTY_REG     0x64
#define BACKLIGHT_PRESCALE_LSB 0x66
#define BACKLIGHT_PRESCALE_MSB 0x67
#define BACKLIGHT_GPIO_CONFIG  0x14
#define BACKLIGHT_GPIO_MUX     0x40

bool backlightSelectionActive = false;


// Configure PWM1 and initialize the backlight.
bool initializeBacklightPWM1()
{
    // Configure GPIO12 as PWM1, output, open-drain.
    uint8_t gpioConfig;
    
    if (!readRegister(BACKLIGHT_GPIO_CONFIG, &gpioConfig))
        return false;
    
    // MUX = 01: PWM1
    // DIR = 1: Output
    // TYPE = 11: Open drain
    // PU = 1: Enable internal pull-up
    gpioConfig = (gpioConfig & 0x04) | 0x79;
    
    if (!writeRegister(BACKLIGHT_GPIO_CONFIG, gpioConfig))
        return false;

    // Configure prescaler for 128 Hz operation.
    if (!writeRegister(BACKLIGHT_PRESCALE_LSB, 0x00))
        return false;

    if (!writeRegister(BACKLIGHT_PRESCALE_MSB, 0x00))
        return false;

    // Initialize backlight to 0%.
    return setBacklightBrightness(0);
}


// Set backlight brightness from 0 to 5.
bool setBacklightBrightness(uint8_t level)
{
    if (level > 5)
        return false;

    uint8_t dutyCycle = (uint16_t)level * 255 / 5;

    // Configure PWM1 timing.
    if (!writeRegister(BACKLIGHT_PRESCALE_LSB, 0x00))
        return false;

    if (!writeRegister(BACKLIGHT_PRESCALE_MSB, 0x00))
        return false;

    // Set duty cycle.
    if (!writeRegister(BACKLIGHT_DUTY_REG, dutyCycle))
        return false;

    // Select operating mode.
    uint8_t controlMode;

    if (level == 0)
        controlMode = 0x00;       // Always off
    else if (level == 5)
        controlMode = 0x03;       // Always on
    else
        controlMode = 0x01;       // Blink/PWM mode

    // Explicitly clear synchronization bit (bit 2).
    if (!writeRegister(BACKLIGHT_CONTROL_REG, controlMode))
        return false;

    Serial.print("Backlight brightness: ");
    Serial.print(level * 20);
    Serial.print("%, duty register: 0x");
    Serial.println(dutyCycle, HEX);

    return true;
}

// Return brightness level for number keys 0-5.
// Return -1 for any other key.

int8_t getBacklightLevelFromUsage(uint8_t usage)
{
    switch (usage)
    {
        case 0x27: return 0;  // 0
        case 0x1E: return 1;  // 1
        case 0x1F: return 2;  // 2
        case 0x20: return 3;  // 3
        case 0x21: return 4;  // 4
        case 0x22: return 5;  // 5

        default: return -1;
    }
}


// ============================================================
// FN KEY FUNCTION HANDLING
// Returns true if the key was handled by the Fn layer.
// ============================================================


bool handleFnFunction(uint8_t row, uint8_t col, bool pressed)
{
    // Fn itself is never sent to the PC.
    if (row == FN_ROW && col == FN_COL)
        return true;

    // --------------------------------------------------------
    // Fn + Esc: Toggle Fn Lock.
    // --------------------------------------------------------

    if (row == 3 && col == 0)
    {
        if (pressed)
        {
            if (fnHeld || fnLocked)
            {
                fnLocked = !fnLocked;
                fnLockKeyDown = true;

#if DEBUG_KEY_EVENTS
                Serial.print("Fn Lock: ");
                Serial.println(fnLocked ? "ON" : "OFF");
#endif
                return true;
            }

            return false;
        }

        if (fnLockKeyDown)
        {
            fnLockKeyDown = false;
            return true;
        }

        return false;
    }

    // --------------------------------------------------------
    // Consumer Control keys
    // --------------------------------------------------------

    if (row == 1 && col == 3)  // F1: Mute
    {
        if (pressed)
            Keyboard.consumerPress(KEY_MUTE);
        else
            Keyboard.consumerRelease();

        return true;
    }

    if (row == 2 && col == 3)  // F2: Volume Down
    {
        if (pressed)
            Keyboard.consumerPress(KEY_VOLUME_DECREMENT);
        else
            Keyboard.consumerRelease();

        return true;
    }

    if (row == 2 && col == 1)  // F3: Volume Up
    {
        if (pressed)
            Keyboard.consumerPress(KEY_VOLUME_INCREMENT);
        else
            Keyboard.consumerRelease();

        return true;
    }

    if (row == 2 && col == 0)  // F4: Previous Track
    {
        if (pressed)
            Keyboard.consumerPress(KEY_SCAN_PREVIOUS);
        else
            Keyboard.consumerRelease();

        return true;
    }

    if (row == 12 && col == 0)  // F5: Play/Pause
    {
        if (pressed)
            Keyboard.consumerPress(KEY_PLAY_PAUSE);
        else
            Keyboard.consumerRelease();

        return true;
    }

    if (row == 15 && col == 0)  // F6: Next Track
    {
        if (pressed)
            Keyboard.consumerPress(KEY_SCAN_NEXT);
        else
            Keyboard.consumerRelease();

        return true;
    }

    if (row == 12 && col == 3)  // F9: Search
    {
        if (pressed)
            Keyboard.consumerPress(KEY_AC_SEARCH);
        else
            Keyboard.consumerRelease();

        return true;
    }

    // --------------------------------------------------------
    // Backlight
    // --------------------------------------------------------

    // Fn + F10: Enter backlight brightness selection.
    if (row == 12 && col == 5)
    {
        if (pressed)
        {
            backlightSelectionActive = true;
    
            Serial.println("Backlight selection active: press 0-5");
        }
    
        return true;
    }

    // --------------------------------------------------------
    // Brightness controls
    // --------------------------------------------------------

    if ((row == 11 && col == 5) ||  // F11
        (row == 10 && col == 5))    // F12
    {
        // Intentionally consume these combinations.
        return true;
    }

    // --------------------------------------------------------
    // Navigation and system keys
    // --------------------------------------------------------

    if (row == 7 && col == 6)  // Right Ctrl: Context Menu
    {
        uint8_t usage = convertHIDUsage(0x65);

        if (pressed)
            Keyboard.press(usage);
        else
            Keyboard.release(usage);

        return true;
    }

    if (row == 9 && col == 7)  // Left Arrow: Home
    {
        uint8_t usage = convertHIDUsage(0x4A);

        if (pressed)
            Keyboard.press(usage);
        else
            Keyboard.release(usage);

        return true;
    }

    if (row == 10 && col == 7)  // Right Arrow: End
    {
        uint8_t usage = convertHIDUsage(0x4D);

        if (pressed)
            Keyboard.press(usage);
        else
            Keyboard.release(usage);

        return true;
    }

    if (row == 9 && col == 0)  // Up Arrow: Page Up
    {
        uint8_t usage = convertHIDUsage(0x4B);

        if (pressed)
            Keyboard.press(usage);
        else
            Keyboard.release(usage);

        return true;
    }

    if (row == 11 && col == 7)  // Down Arrow: Page Down
    {
        uint8_t usage = convertHIDUsage(0x4E);

        if (pressed)
            Keyboard.press(usage);
        else
            Keyboard.release(usage);

        return true;
    }

    // F8 and Print Screen remain normal keys.
    // All other unassigned keys also pass through normally.

    return false;
}

// ============================================================
// USB KEYBOARD REPORT PROCESSING
// ============================================================


void updateKeyboardReports()
{
    bool fnDown = stableState[FN_ROW][FN_COL];

    // Update the global Fn state before processing other keys.
    fnHeld = fnDown;

    // Fn layer is active when Fn is held or Fn Lock is enabled.
    bool fnLayerActive = fnHeld || fnLocked;

    // --------------------------------------------------------
    // Count currently pressed non-modifier keys.
    // --------------------------------------------------------

    uint8_t nonModifierCount = 0;

    for (uint8_t i = 0; i < KEYMAP_SIZE; i++)
    {
        const KeyMapEntry &entry = keyMap[i];

        if (!stableState[entry.row][entry.col])
            continue;

        if (!isModifier(entry.usage))
            nonModifierCount++;
    }

    // --------------------------------------------------------
    // Conservative rollover protection.
    // --------------------------------------------------------

    bool newRolloverState =
        nonModifierCount > MAX_NONMODIFIER_KEYS;

    if (newRolloverState != rolloverActive)
    {
        rolloverActive = newRolloverState;

#if DEBUG_KEY_EVENTS
        if (rolloverActive)
            Serial.println("WARNING: Rollover limit exceeded. Ordinary keys suppressed.");
        else
            Serial.println("Rollover recovered.");
#endif
    }

    // --------------------------------------------------------
    // Update USB HID key states.
    // --------------------------------------------------------

    for (uint8_t i = 0; i < KEYMAP_SIZE; i++)
    {
        const KeyMapEntry &entry = keyMap[i];

        uint8_t row = entry.row;
        uint8_t col = entry.col;

        bool pressed = stableState[row][col];

        bool modifier = isModifier(entry.usage);

        // Preserve modifiers during rollover.
        bool desiredHIDState =
            pressed && (!rolloverActive || modifier);

        // Only process changes in the desired USB state.
        if (desiredHIDState == hidActive[row][col])
            continue;

        if (desiredHIDState)
        {
            bool consumedByFn = false;
            bool cancelledBacklightSelection = false;
        
            // Check whether backlight brightness selection is active.
            if (backlightSelectionActive)
            {
                int8_t selectedLevel =
                    getBacklightLevelFromUsage(entry.usage);
        
                // Exit selection mode immediately.
                backlightSelectionActive = false;
        
                if (selectedLevel >= 0)
                {
                    // Valid brightness selection.
                    Serial.print("Selected backlight level: ");
                    Serial.println(selectedLevel);
        
                    if (!setBacklightBrightness(selectedLevel))
                        Serial.println("Backlight update failed.");
        
                    // Consume the number key.
                    consumedByFn = true;
                }
                else
                {
                    // Any other key cancels selection.
                    Serial.println("Backlight selection cancelled.");
        
                    cancelledBacklightSelection = true;
                }
            }
        
            bool fnRightCtrl = (row == 7 && col == 6);
        
            // Process normal Fn combinations unless selection
            // was just cancelled or a brightness was selected.
            if (!consumedByFn &&
                !cancelledBacklightSelection &&
                fnLayerActive &&
                (!modifier || fnRightCtrl))
            {
                consumedByFn = handleFnFunction(row, col, true);
            }
        
            fnConsumed[row][col] = consumedByFn;
        
            if (!consumedByFn)
                Keyboard.press(convertHIDUsage(entry.usage));
        
            hidActive[row][col] = true;
        }
        else
        {
            // Release keys through the same path used for their
            // original press, even if Fn has since been released.

            if (fnConsumed[row][col])
            {
                handleFnFunction(row, col, false);
                fnConsumed[row][col] = false;
            }
            else
            {
                uint8_t keycode = convertHIDUsage(entry.usage);
                Keyboard.release(keycode);
            }

            hidActive[row][col] = false;
        }
    }
}


// ============================================================
// ARDUINO SETUP
// ============================================================

void setup()
{
    Serial.begin(SERIAL_BAUD);

    delay(1000);

    Serial.println();
    Serial.println("ECE1117 Keyboard Controller");
    Serial.println("---------------------------");

    // Initialize SMBus.

    Wire.begin();
    Wire.setClock(100000);

    if (!initializeECE1117())
    {
        Serial.println("Initialization failed. Halting.");

        while (1)
            delay(100);
    }

    initializeBacklightPWM1();
    initializeCapsLockLED();

    setBacklightBrightness(3);

    // Initialize USB HID keyboard.

    Keyboard.begin();
    Keyboard.onLED(keyboardLEDCallback);
    Keyboard.releaseAll();
    Keyboard.consumerRelease();

    fnHeld = false;
    fnLocked = false;
    fnLockKeyDown = false;

    Serial.println("USB HID keyboard initialized.");
    Serial.println("Keyboard scanning started.");
}

// ============================================================
// ARDUINO MAIN LOOP
// ============================================================

void loop()
{
    // Scan the complete keyboard matrix.

    scanKeyboardMatrix();

    // Update USB HID reports based on debounced states.

    updateKeyboardReports();

    // No additional delay is required here.

    updateCapsLockLED();
}
