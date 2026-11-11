
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TinyUSB.h>
#include <string.h>

// ============================================================
// Trackpad configuration
// ============================================================

#define TRACKPAD_ADDR       0x2C
#define TRACKPAD_INT_PIN    D6

#define HID_DESC_REGISTER   0x0020
#define INPUT_REGISTER      0x0024

#define MAX_INPUT_LENGTH    60

#define REPORT_ID_TOUCHPAD  0x03
#define REPORT_ID_PTPHQA    0x07
#define REPORT_ID_CAPS      0x08

#define CONTACT_COUNT       5

// I2C input report:
//   2 bytes length
//   1 byte report ID
//   25 bytes contact data (5 x 5)
//   2 bytes scan time
//   1 byte contact count
//   1 byte button + padding
//
// USB report
//   1 byte report ID (added by TinyUSB)
//   29 bytes payload

#define USB_TOUCHPAD_PAYLOAD_LENGTH 29
#define I2C_TOUCHPAD_REPORT_LENGTH  32

// ============================================================
// USB HID report descriptor
// ============================================================

// One Finger collection:
//
//   Confidence: 1 bit
//   Tip Switch: 1 bit
//   Contact ID: 3 bits
//   Padding:    3 bits
//   X:          16 bits
//   Y:          16 bits
//
// This matches the original Synaptics report layout.

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

// Windows Precision Touchpad top-level collection.

const uint8_t touchpadReportDescriptor[] = {

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
// USB HID
// ============================================================

Adafruit_USBD_HID usb_hid;

// PTPHQA data is 256 bytes, divided into eight 32-byte chunks.
//
// This zero-filled blob is a placeholder, not a signed
// certification blob.

const uint8_t ptphqaBlob[256] = { 0 };

uint8_t requestedPtpHqaSegment = 1;

// ============================================================
// Pending USB report
// ============================================================

uint8_t pendingReport[USB_TOUCHPAD_PAYLOAD_LENGTH];
bool pendingReportValid = false;

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

    Wire.beginTransmission(TRACKPAD_ADDR);
    Wire.write(command, sizeof(command));

    uint8_t error = Wire.endTransmission();

    delay(10);

    return error == 0;
}

// ============================================================
// Read one HID-over-I2C input report
// ============================================================

bool readTrackpadReport(uint8_t *payload) {

    uint8_t packet[MAX_INPUT_LENGTH];

    // Select the input register.
    Wire.beginTransmission(TRACKPAD_ADDR);
    Wire.write((uint8_t)(INPUT_REGISTER & 0xFF));
    Wire.write((uint8_t)(INPUT_REGISTER >> 8));

    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    // Read the maximum input report length.
    int received = Wire.requestFrom(
        (uint8_t)TRACKPAD_ADDR,
        (uint8_t)MAX_INPUT_LENGTH
    );

    if (received < I2C_TOUCHPAD_REPORT_LENGTH) {
        while (Wire.available()) {
            Wire.read();
        }

        return false;
    }

    uint8_t bytesRead = 0;

    while (Wire.available() && bytesRead < MAX_INPUT_LENGTH) {
        packet[bytesRead++] = Wire.read();
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
// Setup
// ============================================================

void setup() {

    Serial.begin(115200);

    // --------------------------------------------------------
    // I2C setup
    // --------------------------------------------------------

    Wire.begin();
    Wire.setClock(400000);
    Wire.setTimeout(1000);

    pinMode(TRACKPAD_INT_PIN, INPUT);

    delay(50);

    if (!enableMultitouchMode()) {
        Serial.println("Failed to enable multitouch mode");
    } else {
        Serial.println("Multitouch mode enabled");
    }

    // --------------------------------------------------------
    // USB HID setup
    // --------------------------------------------------------

    if (!TinyUSBDevice.isInitialized()) {
        TinyUSBDevice.begin(0);
    }

    usb_hid.setPollInterval(1);

    usb_hid.setReportDescriptor(
        touchpadReportDescriptor,
        sizeof(touchpadReportDescriptor)
    );

    usb_hid.setReportCallback(
        getHidReport,
        setHidReport
    );

    usb_hid.begin();

    // Re-enumerate if the USB device was already mounted.
    if (TinyUSBDevice.mounted()) {
        TinyUSBDevice.detach();
        delay(10);
        TinyUSBDevice.attach();
    }

    Serial.println("USB Precision Touchpad initialized");
}

// ============================================================
// Main loop
// ============================================================

void loop() {

    // --------------------------------------------------------
    // Read trackpad when its active-low interrupt is asserted.
    // --------------------------------------------------------

    if (digitalRead(TRACKPAD_INT_PIN) == LOW) {

        uint8_t report[USB_TOUCHPAD_PAYLOAD_LENGTH];

        if (readTrackpadReport(report)) {

            // Keep the newest report if USB is temporarily busy.
            memcpy(
                pendingReport,
                report,
                USB_TOUCHPAD_PAYLOAD_LENGTH
            );

            pendingReportValid = true;
        }
    }

    // --------------------------------------------------------
    // Forward the report to the USB host.
    // --------------------------------------------------------

    if (pendingReportValid && usb_hid.ready()) {

        if (usb_hid.sendReport(
            REPORT_ID_TOUCHPAD,
            pendingReport,
            USB_TOUCHPAD_PAYLOAD_LENGTH
        )) {
            pendingReportValid = false;
        }
    }
}
