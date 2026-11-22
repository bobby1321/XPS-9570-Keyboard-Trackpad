
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TinyUSB.h>
#include <string.h>

// ============================================================
// USER CONFIGURATION
// ============================================================

// Set to 1 to print key press/release events to Serial.
// Set to 0 to disable key event messages.
#define DEBUG_KEY_EVENTS 0

#define ECE1117_ADDR 0x38

#define SERIAL_BAUD 115200

// ============================================================
// RASPBERRY PI PICO PIN CONFIGURATION
// ============================================================

// ECE1117 keyboard controller: I2C0
#define KEYBOARD_I2C_SDA 4       // GP4, physical pin 6
#define KEYBOARD_I2C_SCL 5       // GP5, physical pin 7

// Synaptics trackpad: I2C1
#define TRACKPAD_I2C_SDA 2       // GP2, physical pin 4
#define TRACKPAD_I2C_SCL 3       // GP3, physical pin 5
#define TRACKPAD_INT_PIN 15      // GP15, physical pin 20

#define TRACKPAD_ADDR       0x2C
#define INPUT_REGISTER      0x0024
#define MAX_INPUT_LENGTH    60

#define REPORT_ID_KEYBOARD  0x01
#define REPORT_ID_CONSUMER  0x02
#define REPORT_ID_TOUCHPAD  0x03
#define REPORT_ID_PTPHQA    0x07
#define REPORT_ID_CAPS      0x08

#define CONTACT_COUNT       5
#define USB_TOUCHPAD_PAYLOAD_LENGTH 29
#define I2C_TOUCHPAD_REPORT_LENGTH  32

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

// Host Caps Lock state is received through the TinyUSB output report.

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
// USB HID REPORT STATE
// ============================================================

// Raw HID usages are sent directly in the TinyUSB keyboard report.
// Modifier usages 0xE0-0xE7 are represented in the modifier byte.

bool isModifier(uint8_t usage)
{
    return usage >= 0xE0 && usage <= 0xE7;
}

Adafruit_USBD_HID usb_hid;

uint8_t fnRemappedUsage[MATRIX_ROWS][MATRIX_COLS] = {};
uint16_t activeConsumerUsage = 0;

uint8_t lastKeyboardReport[8] = {};
uint8_t lastConsumerReport[2] = {};

void setFnRemappedKey(uint8_t row, uint8_t col,
                      uint8_t usage, bool pressed)
{
    fnRemappedUsage[row][col] = pressed ? usage : 0;
}

void setConsumerUsage(uint16_t usage, bool pressed)
{
    activeConsumerUsage = pressed ? usage : 0;
}

void sendKeyboardReport()
{
    uint8_t report[8] = {};

    uint8_t keyIndex = 0;

    for (uint8_t i = 0; i < KEYMAP_SIZE; i++)
    {
        const KeyMapEntry &entry = keyMap[i];

        uint8_t row = entry.row;
        uint8_t col = entry.col;

        if (!hidActive[row][col])
            continue;

        uint8_t usage = fnRemappedUsage[row][col];

        // Fn-consumed keys are omitted unless they have a remapped
        // keyboard usage (for example Fn + Right Ctrl -> Menu).
        if (usage == 0)
        {
            if (fnConsumed[row][col])
                continue;

            usage = entry.usage;
        }

        if (isModifier(usage))
        {
            report[0] |= (1 << (usage - 0xE0));
        }
        else if (keyIndex < 6)
        {
            report[2 + keyIndex] = usage;
            keyIndex++;
        }
    }

    if (memcmp(report, lastKeyboardReport, sizeof(report)) == 0)
        return;

    if (usb_hid.ready() &&
        usb_hid.sendReport(REPORT_ID_KEYBOARD, report, sizeof(report)))
    {
        memcpy(lastKeyboardReport, report, sizeof(report));
    }
}

void sendConsumerReport()
{
    uint8_t report[2] = {
        (uint8_t)(activeConsumerUsage & 0xFF),
        (uint8_t)(activeConsumerUsage >> 8)
    };

    if (memcmp(report, lastConsumerReport, sizeof(report)) == 0)
        return;

    if (usb_hid.ready() &&
        usb_hid.sendReport(REPORT_ID_CONSUMER, report, sizeof(report)))
    {
        memcpy(lastConsumerReport, report, sizeof(report));
    }
}

#define FINGER_BLOCK \
    0x05, 0x0D,             /* Usage Page (Digitizers) */ \
    0x09, 0x22,             /* Usage (Finger) */ \
    0xA1, 0x02,             /* Collection (Logical) */ \
    0x15, 0x00,             /* Logical Minimum (0) */ \
    0x25, 0x01,             /* Logical Maximum (1) */ \
    0x09, 0x47,             /* Usage (Confidence) */ \
    0x09, 0x42,             /* Usage (Tip Switch) */ \
    0x95, 0x02,             /* Report Count (2) */ \
    0x75, 0x01,             /* Report Size (1) */ \
    0x81, 0x02,             /* Input (Data, Variable, Absolute) */ \
    0x95, 0x01,             /* Report Count (1) */ \
    0x75, 0x03,             /* Report Size (3) */ \
    0x25, 0x05,             /* Logical Maximum (5) */ \
    0x09, 0x51,             /* Usage (Contact ID) */ \
    0x81, 0x02,             /* Input (Data, Variable, Absolute) */ \
    0x75, 0x01,             /* Report Size (1) */ \
    0x95, 0x03,             /* Report Count (3) */ \
    0x81, 0x03,             /* Input (Constant) */ \
    0x05, 0x01,             /* Usage Page (Generic Desktop) */ \
    0x15, 0x00,             /* Logical Minimum (0) */ \
    0x26, 0xCC, 0x04,       /* Logical Maximum (1228) */ \
    0x75, 0x10,             /* Report Size (16) */ \
    0x55, 0x0E,             /* Unit Exponent (-2) */ \
    0x65, 0x11,             /* Unit (SI Linear) */ \
    0x09, 0x30,             /* Usage (X) */ \
    0x35, 0x00,             /* Physical Minimum (0) */ \
    0x46, 0x00, 0x04,       /* Physical Maximum (1024) */ \
    0x95, 0x01,             /* Report Count (1) */ \
    0x81, 0x02,             /* Input (Data, Variable, Absolute) */ \
    0x46, 0x06, 0x03,       /* Physical Maximum (774) */ \
    0x26, 0xA0, 0x03,       /* Logical Maximum (928) */ \
    0x09, 0x31,             /* Usage (Y) */ \
    0x81, 0x02,             /* Input (Data, Variable, Absolute) */ \
    0xC0                    /* End Collection */


// ============================================================
// COMBINED USB HID REPORT DESCRIPTOR
// ============================================================

const uint8_t combinedReportDescriptor[] = {

    // --------------------------------------------------------
    // Keyboard input report (Report ID 1)
    // --------------------------------------------------------

    0x05, 0x01,             // Usage Page (Generic Desktop)
    0x09, 0x06,             // Usage (Keyboard)
    0xA1, 0x01,             // Collection (Application)
    0x85, REPORT_ID_KEYBOARD,

    0x05, 0x07,             // Usage Page (Keyboard)
    0x19, 0xE0,             // Usage Minimum (Left Control)
    0x29, 0xE7,             // Usage Maximum (Right GUI)
    0x15, 0x00,             // Logical Minimum (0)
    0x25, 0x01,             // Logical Maximum (1)
    0x75, 0x01,             // Report Size (1)
    0x95, 0x08,             // Report Count (8)
    0x81, 0x02,             // Input (Data, Variable, Absolute)

    0x75, 0x08,             // Report Size (8)
    0x95, 0x01,             // Report Count (1)
    0x81, 0x01,             // Input (Constant)

    // Keyboard LED output report: Num, Caps, Scroll, Compose, Kana
    0x05, 0x08,             // Usage Page (LEDs)
    0x19, 0x01,             // Usage Minimum (Num Lock)
    0x29, 0x05,             // Usage Maximum (Kana)
    0x75, 0x01,             // Report Size (1)
    0x95, 0x05,             // Report Count (5)
    0x91, 0x02,             // Output (Data, Variable, Absolute)
    0x75, 0x03,             // Report Size (3)
    0x95, 0x01,             // Report Count (1)
    0x91, 0x01,             // Output (Constant)

    0x05, 0x07,             // Usage Page (Keyboard)
    0x19, 0x00,             // Usage Minimum (0)
    0x29, 0x65,             // Usage Maximum (101)
    0x15, 0x00,             // Logical Minimum (0)
    0x25, 0x65,             // Logical Maximum (101)
    0x75, 0x08,             // Report Size (8)
    0x95, 0x06,             // Report Count (6)
    0x81, 0x00,             // Input (Data, Array, Absolute)
    0xC0,                   // End Collection

    // --------------------------------------------------------
    // Consumer control input report (Report ID 2)
    // --------------------------------------------------------

    0x05, 0x0C,             // Usage Page (Consumer)
    0x09, 0x01,             // Usage (Consumer Control)
    0xA1, 0x01,             // Collection (Application)
    0x85, REPORT_ID_CONSUMER,
    0x15, 0x00,             // Logical Minimum (0)
    0x26, 0xFF, 0x03,       // Logical Maximum (1023)
    0x19, 0x00,             // Usage Minimum (0)
    0x2A, 0xFF, 0x03,       // Usage Maximum (1023)
    0x75, 0x10,             // Report Size (16)
    0x95, 0x01,             // Report Count (1)
    0x81, 0x00,             // Input (Data, Array, Absolute)
    0xC0,                   // End Collection

    // --------------------------------------------------------
    // Windows Precision Touchpad (Report ID 3, 7, 8)
    // --------------------------------------------------------


    0x05, 0x0D,             // Usage Page (Digitizers)
    0x09, 0x05,             // Usage (Touch Pad)
    0xA1, 0x01,             // Collection (Application)

    0x85, REPORT_ID_TOUCHPAD,

    // Five contact collections
    FINGER_BLOCK,
    FINGER_BLOCK,
    FINGER_BLOCK,
    FINGER_BLOCK,
    FINGER_BLOCK,

    // --------------------------------------------------------
    // Scan Time
    // --------------------------------------------------------

    0x55, 0x0C,             // Unit Exponent (-4)
    0x66, 0x01, 0x10,       // Unit (Seconds)
    0x47, 0xFF, 0xFF, 0x00, 0x00, // Physical Maximum (65535)
    0x27, 0xFF, 0xFF, 0x00, 0x00, // Logical Maximum (65535)
    0x75, 0x10,             // Report Size (16)
    0x95, 0x01,             // Report Count (1)
    0x05, 0x0D,             // Usage Page (Digitizers)
    0x09, 0x56,             // Usage (Scan Time)
    0x81, 0x02,             // Input (Data, Variable, Absolute)

    // --------------------------------------------------------
    // Contact Count
    // --------------------------------------------------------

    0x09, 0x54,             // Usage (Contact Count)
    0x15, 0x00,             // Logical Minimum (0)
    0x25, 0x05,             // Logical Maximum (5)
    0x75, 0x08,             // Report Size (8)
    0x95, 0x01,             // Report Count (1)
    0x81, 0x02,             // Input (Data, Variable, Absolute)

    // --------------------------------------------------------
    // Integrated clickpad button
    // --------------------------------------------------------

    0x05, 0x09,             // Usage Page (Buttons)
    0x09, 0x01,             // Usage (Button 1)
    0x15, 0x00,             // Logical Minimum (0)
    0x25, 0x01,             // Logical Maximum (1)
    0x75, 0x01,             // Report Size (1)
    0x95, 0x01,             // Report Count (1)
    0x81, 0x02,             // Input (Data, Variable, Absolute)

    0x95, 0x07,             // Report Count (7)
    0x81, 0x03,             // Input (Constant)

    // --------------------------------------------------------
    // Device capabilities feature report
    //
    // Low nibble: Contact Count Maximum = 5
    // High nibble: Pad Type = 0 (depressible clickpad)
    // --------------------------------------------------------

    0x05, 0x0D,             // Usage Page (Digitizers)
    0x85, REPORT_ID_CAPS,
    0x09, 0x55,             // Usage (Contact Count Maximum)
    0x09, 0x59,             // Usage (Pad Type)
    0x15, 0x00,             // Logical Minimum (0)
    0x25, 0x0F,             // Logical Maximum (15)
    0x75, 0x04,             // Report Size (4)
    0x95, 0x02,             // Report Count (2)
    0xB1, 0x02,             // Feature (Data, Variable, Absolute)

    // --------------------------------------------------------
    // PTPHQA certification-status feature report
    //
    // Segmented into eight 32-byte chunks.
    // --------------------------------------------------------

    0x06, 0x00, 0xFF,       // Usage Page (Vendor Defined)
    0x85, REPORT_ID_PTPHQA,

    0x09, 0xC6,             // Usage (Segment Number)
    0x15, 0x00,             // Logical Minimum (0)
    0x25, 0x08,             // Logical Maximum (8)
    0x75, 0x08,             // Report Size (8)
    0x95, 0x01,             // Report Count (1)
    0xB1, 0x02,             // Feature

    0x09, 0xC7,             // Usage (Segment Data)
    0x26, 0xFF, 0x00,       // Logical Maximum (255)
    0x95, 0x20,             // Report Count (32)
    0xB1, 0x02,             // Feature

    0xC0                    // End Collection
};


// ============================================================
// TRACKPAD USB REPORT STATE
// ============================================================

const uint8_t ptphqaBlob[256] = { 0 };
uint8_t requestedPtpHqaSegment = 1;

uint8_t pendingReport[USB_TOUCHPAD_PAYLOAD_LENGTH] = {};
bool pendingReportValid = false;

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
            setConsumerUsage(CONSUMER_MUTE, true);
        else
            setConsumerUsage(0, false);

        return true;
    }

    if (row == 2 && col == 3)  // F2: Volume Down
    {
        if (pressed)
            setConsumerUsage(CONSUMER_VOLUME_DOWN, true);
        else
            setConsumerUsage(0, false);

        return true;
    }

    if (row == 2 && col == 1)  // F3: Volume Up
    {
        if (pressed)
            setConsumerUsage(CONSUMER_VOLUME_UP, true);
        else
            setConsumerUsage(0, false);

        return true;
    }

    if (row == 2 && col == 0)  // F4: Previous Track
    {
        if (pressed)
            setConsumerUsage(CONSUMER_PREVIOUS, true);
        else
            setConsumerUsage(0, false);

        return true;
    }

    if (row == 12 && col == 0)  // F5: Play/Pause
    {
        if (pressed)
            setConsumerUsage(CONSUMER_PLAY_PAUSE, true);
        else
            setConsumerUsage(0, false);

        return true;
    }

    if (row == 15 && col == 0)  // F6: Next Track
    {
        if (pressed)
            setConsumerUsage(CONSUMER_NEXT, true);
        else
            setConsumerUsage(0, false);

        return true;
    }

    if (row == 12 && col == 3)  // F9: Search
    {
        if (pressed)
            setConsumerUsage(CONSUMER_SEARCH, true);
        else
            setConsumerUsage(0, false);

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
        setFnRemappedKey(row, col, 0x65, pressed);

        return true;
    }

    if (row == 9 && col == 7)  // Left Arrow: Home
    {
        setFnRemappedKey(row, col, 0x4A, pressed);

        return true;
    }

    if (row == 10 && col == 7)  // Right Arrow: End
    {
        setFnRemappedKey(row, col, 0x4D, pressed);

        return true;
    }

    if (row == 9 && col == 0)  // Up Arrow: Page Up
    {
        setFnRemappedKey(row, col, 0x4B, pressed);

        return true;
    }

    if (row == 11 && col == 7)  // Down Arrow: Page Down
    {
        setFnRemappedKey(row, col, 0x4E, pressed);

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
        
            // The combined keyboard report is generated after all
            // matrix transitions have been processed.
        
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
                // The combined keyboard report is generated after all
                // matrix transitions have been processed.
            }

            hidActive[row][col] = false;
        }
    }

    sendKeyboardReport();
    sendConsumerReport();
}


// ============================================================
// HID feature report callbacks
// ============================================================

uint16_t getHidReport(
    uint8_t reportId,
    hid_report_type_t reportType,
    uint8_t *buffer,
    uint16_t requestedLength
) {
    if (reportType != HID_REPORT_TYPE_FEATURE) {
        return 0;
    }

    // Device capabilities
    if (reportId == REPORT_ID_CAPS) {

        if (requestedLength < 1) {
            return 0;
        }

        // Contact Count Maximum = 5
        // Pad Type = 0 (depressible)
        buffer[0] = 0x05;

        return 1;
    }

    // PTPHQA certification-status data
    if (reportId == REPORT_ID_PTPHQA) {

        if (requestedLength < 33) {
            return 0;
        }

        uint8_t segment = requestedPtpHqaSegment;

        if (segment < 1 || segment > 8) {
            segment = 1;
        }

        buffer[0] = segment;

        memcpy(
            &buffer[1],
            &ptphqaBlob[(segment - 1) * 32],
            32
        );

        return 33;
    }

    return 0;
}

void setHidReport(
    uint8_t reportId,
    hid_report_type_t reportType,
    const uint8_t *buffer,
    uint16_t bufferSize
) {
    // Keyboard LED output report (Report ID 1).
    if (reportId == REPORT_ID_KEYBOARD &&
        reportType == HID_REPORT_TYPE_OUTPUT &&
        bufferSize >= 1) {

        hostCapsLock = (buffer[0] & 0x02) != 0;
        capsLockLEDUpdatePending = true;
        return;
    }

    if (reportType != HID_REPORT_TYPE_FEATURE) {
        return;
    }

    // The host selects which PTPHQA segment it wants.
    if (reportId == REPORT_ID_PTPHQA && bufferSize >= 1) {

        uint8_t segment = buffer[0];

        if (segment >= 1 && segment <= 8) {
            requestedPtpHqaSegment = segment;
        }
    }
}

// ============================================================
// Enable multitouch mode on the Synaptics trackpad
// ============================================================

bool enableMultitouchMode() {

    const uint8_t command[] = {
        0x22, 0x00,           // Command register
        0x34, 0x03,           // SET_REPORT
        0x23, 0x00,           // Data register
        0x04, 0x00,           // Report length
        0x04,                 // Report ID
        0x03                  // Input Mode
    };

    Wire1.beginTransmission(TRACKPAD_ADDR);
    Wire1.write(command, sizeof(command));

    uint8_t error = Wire1.endTransmission();

    delay(10);

    return error == 0;
}

// ============================================================
// Read one HID-over-I2C input report
// ============================================================

bool readTrackpadReport(uint8_t *payload) {

    uint8_t packet[MAX_INPUT_LENGTH];

    // Select the input register.
    Wire1.beginTransmission(TRACKPAD_ADDR);
    Wire1.write((uint8_t)(INPUT_REGISTER & 0xFF));
    Wire1.write((uint8_t)(INPUT_REGISTER >> 8));

    if (Wire1.endTransmission(false) != 0) {
        return false;
    }

    // Read the maximum input report length.
    int received = Wire1.requestFrom(
        (uint8_t)TRACKPAD_ADDR,
        (uint8_t)MAX_INPUT_LENGTH
    );

    if (received < I2C_TOUCHPAD_REPORT_LENGTH) {
        while (Wire1.available()) {
            Wire1.read();
        }

        return false;
    }

    uint8_t bytesRead = 0;

    while (Wire1.available() && bytesRead < MAX_INPUT_LENGTH) {
        packet[bytesRead++] = Wire1.read();
    }

    // The I2C report starts with a 16-bit length field.
    uint16_t reportLength =
        packet[0] | ((uint16_t)packet[1] << 8);

    if (reportLength != I2C_TOUCHPAD_REPORT_LENGTH ||
        bytesRead < reportLength) {
        return false;
    }

    // We only forward the multitouch report.
    if (packet[2] != REPORT_ID_TOUCHPAD) {
        return false;
    }

    // Contact count is at I2C packet offset 30.
    if (packet[30] > CONTACT_COUNT) {
        return false;
    }

    // Strip:
    //   packet[0..1] = I2C length prefix
    //   packet[2]   = HID report ID
    //
    // USB TinyUSB adds the report ID itself.
    //
    // Copy:
    //   packet[3..31] = 29-byte input payload

    memcpy(
        payload,
        &packet[3],
        USB_TOUCHPAD_PAYLOAD_LENGTH
    );

    return true;
}


// ============================================================
// ARDUINO SETUP
// ============================================================

void setup()
{
    Serial.begin(SERIAL_BAUD);

    delay(1000);

    Serial.println();
    Serial.println("XPS 15 9570 Keyboard + Precision Touchpad");
    Serial.println("-----------------------------------------");

    // --------------------------------------------------------
    // Initialize the two independent I2C buses.
    // --------------------------------------------------------

    // ECE1117 keyboard controller on I2C0.
    Wire.setSDA(KEYBOARD_I2C_SDA);
    Wire.setSCL(KEYBOARD_I2C_SCL);
    Wire.begin();
    Wire.setClock(100000);

    // Synaptics trackpad on I2C1.
    Wire1.setSDA(TRACKPAD_I2C_SDA);
    Wire1.setSCL(TRACKPAD_I2C_SCL);
    Wire1.begin();
    Wire1.setClock(400000);
    Wire1.setTimeout(1000);

    pinMode(TRACKPAD_INT_PIN, INPUT);

    // --------------------------------------------------------
    // Initialize the ECE1117 keyboard controller.
    // --------------------------------------------------------

    if (!initializeECE1117())
    {
        Serial.println("ECE1117 initialization failed. Halting.");

        while (1)
            delay(100);
    }

    initializeBacklightPWM1();
    initializeCapsLockLED();

    setBacklightBrightness(3);

    // --------------------------------------------------------
    // Initialize the Synaptics trackpad.
    // --------------------------------------------------------

    delay(50);

    if (!enableMultitouchMode())
        Serial.println("Failed to enable trackpad multitouch mode.");
    else
        Serial.println("Trackpad multitouch mode enabled.");

    // --------------------------------------------------------
    // Initialize the combined TinyUSB HID interface.
    // --------------------------------------------------------

    if (!TinyUSBDevice.isInitialized())
        TinyUSBDevice.begin(0);

    usb_hid.setPollInterval(1);
    usb_hid.setReportDescriptor(
        combinedReportDescriptor,
        sizeof(combinedReportDescriptor)
    );
    usb_hid.setReportCallback(
        getHidReport,
        setHidReport
    );
    usb_hid.begin();

    if (TinyUSBDevice.mounted())
    {
        TinyUSBDevice.detach();
        delay(10);
        TinyUSBDevice.attach();
    }

    fnHeld = false;
    fnLocked = false;
    fnLockKeyDown = false;

    Serial.println("Combined TinyUSB HID interface initialized.");
    Serial.println("Keyboard scanning started.");
}

void loop()
{
#ifdef TINYUSB_NEED_POLLING_TASK
    TinyUSBDevice.task();
#endif

    // --------------------------------------------------------
    // Service the trackpad first so its active-low interrupt
    // is handled before the full keyboard matrix scan.
    // --------------------------------------------------------

    if (digitalRead(TRACKPAD_INT_PIN) == LOW)
    {
        uint8_t report[USB_TOUCHPAD_PAYLOAD_LENGTH];

        if (readTrackpadReport(report))
        {
            memcpy(
                pendingReport,
                report,
                USB_TOUCHPAD_PAYLOAD_LENGTH
            );

            pendingReportValid = true;
        }
    }

    // --------------------------------------------------------
    // Scan and process the keyboard matrix.
    // --------------------------------------------------------

    scanKeyboardMatrix();
    updateKeyboardReports();

    // Apply deferred Caps Lock LED updates after keyboard I2C
    // scanning, keeping all ECE1117 transactions serialized.
    updateCapsLockLED();

    // --------------------------------------------------------
    // Forward the newest pending touchpad report.
    // --------------------------------------------------------

    if (pendingReportValid && usb_hid.ready())
    {
        if (usb_hid.sendReport(
            REPORT_ID_TOUCHPAD,
            pendingReport,
            USB_TOUCHPAD_PAYLOAD_LENGTH
        ))
        {
            pendingReportValid = false;
        }
    }
}
