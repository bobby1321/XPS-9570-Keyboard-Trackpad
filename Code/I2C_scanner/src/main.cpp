
#include <Arduino.h>
#include <Wire.h>

// ============================================================
// Trackpad configuration
// ============================================================

#define TRACKPAD_ADDR           0x2C
#define INPUT_REGISTER          0x0024
#define MAX_INPUT_LENGTH        60

#define I2C_SDA_PIN             D4
#define I2C_SCL_PIN             D5
#define TRACKPAD_INT_PIN        D6

#define I2C_BUFFER_SIZE         64
#define MAX_REPORTS_PER_LOOP    8

uint8_t inputBuffer[MAX_INPUT_LENGTH];

// ============================================================
// Read a 16-bit little-endian value
// ============================================================

uint16_t readLE16(const uint8_t *data, uint8_t offset)
{
    return (uint16_t)data[offset] |
           ((uint16_t)data[offset + 1] << 8);
}

// ============================================================
// Read an input report from the trackpad
// ============================================================

bool readInputReport(uint16_t &reportLength)
{
    // Select input register
    Wire.beginTransmission(TRACKPAD_ADDR);

    Wire.write((uint8_t)(INPUT_REGISTER & 0xFF));
    Wire.write((uint8_t)(INPUT_REGISTER >> 8));

    // Repeated start
    uint8_t error = Wire.endTransmission(false);

    if (error != 0)
    {
        Serial.print("Input register write failed: ");
        Serial.println(error);
        return false;
    }

    // Read maximum input report length
    size_t received = Wire.requestFrom(
        (uint8_t)TRACKPAD_ADDR,
        (size_t)MAX_INPUT_LENGTH,
        (bool)true
    );

    if (received < 2)
    {
        Serial.println("Insufficient input data.");

        while (Wire.available())
            Wire.read();

        return false;
    }

    // Copy received data
    for (size_t i = 0; i < received; i++)
    {
        if (!Wire.available())
            return false;

        inputBuffer[i] = Wire.read();
    }

    // First two bytes contain the total report length,
    // including the two-byte length field.
    reportLength = readLE16(inputBuffer, 0);

    if (reportLength < 3 ||
        reportLength > received ||
        reportLength > MAX_INPUT_LENGTH)
    {
        Serial.print("Invalid report length: ");
        Serial.println(reportLength);
        return false;
    }

    return true;
}

// ============================================================
// Print raw input report
// ============================================================

void printRawReport(uint16_t reportLength)
{
    Serial.print("\nRaw report (");
    Serial.print(reportLength);
    Serial.println(" bytes):");

    for (uint16_t i = 0; i < reportLength; i++)
    {
        Serial.printf("%02X ", inputBuffer[i]);

        if ((i + 1) % 16 == 0)
            Serial.println();
    }

    Serial.println();
}

// ============================================================
// Decode multitouch report (Report ID 0x03)
// ============================================================

void decodeTouchReport(uint16_t reportLength)
{
    // Report layout:
    //
    // Bytes 0-1:  HID input length
    // Byte  2:    Report ID
    //
    // Five contact records, 5 bytes each:
    // Byte 0:     Confidence (bit 0)
    //             Tip switch (bit 1)
    //             Contact ID (bits 2-4)
    // Bytes 1-2:  X coordinate
    // Bytes 3-4:  Y coordinate
    //
    // Following the contact records:
    // Bytes 28-29: Scan time
    // Byte  30:    Contact count
    // Byte  31:    Button (bit 0)

    const uint8_t CONTACT_COUNT = 5;
    const uint8_t CONTACT_SIZE = 5;
    const uint8_t CONTACT_START = 3;

    const uint8_t SCAN_TIME_OFFSET =
        CONTACT_START + CONTACT_COUNT * CONTACT_SIZE;

    const uint8_t COUNT_OFFSET = SCAN_TIME_OFFSET + 2;
    const uint8_t BUTTON_OFFSET = COUNT_OFFSET + 1;

    if (reportLength < BUTTON_OFFSET + 1)
    {
        Serial.println("Multitouch report is too short.");
        return;
    }

    Serial.println("\n--- MULTITOUCH REPORT ---");

    for (uint8_t i = 0; i < CONTACT_COUNT; i++)
    {
        uint8_t offset = CONTACT_START + i * CONTACT_SIZE;

        uint8_t flags = inputBuffer[offset];

        bool confidence = flags & 0x01;
        bool tipSwitch = flags & 0x02;

        uint8_t contactID = (flags >> 2) & 0x07;

        uint16_t x = readLE16(inputBuffer, offset + 1);
        uint16_t y = readLE16(inputBuffer, offset + 3);

        Serial.printf(
            "Finger %u: ID=%u, Confidence=%u, Tip=%u, X=%u, Y=%u\n",
            i,
            contactID,
            confidence,
            tipSwitch,
            x,
            y
        );
    }

    uint16_t scanTime = readLE16(inputBuffer, SCAN_TIME_OFFSET);

    uint8_t activeContacts = inputBuffer[COUNT_OFFSET];

    bool buttonPressed = inputBuffer[BUTTON_OFFSET] & 0x01;

    Serial.printf("Scan time:       %u\n", scanTime);
    Serial.printf("Contact count:   %u\n", activeContacts);
    Serial.printf("Button pressed:  %u\n", buttonPressed);
}

// ============================================================
// Decode conventional mouse report (Report ID 0x02)
// ============================================================

void decodeMouseReport(uint16_t reportLength)
{
    // Bytes 0-1: HID input length
    // Byte 2:    Report ID
    // Byte 3:    Buttons
    // Byte 4:    Relative X
    // Byte 5:    Relative Y

    if (reportLength < 6)
    {
        Serial.println("Mouse report is too short.");
        return;
    }

    uint8_t buttons = inputBuffer[3];

    int8_t x = (int8_t)inputBuffer[4];
    int8_t y = (int8_t)inputBuffer[5];

    Serial.println("\n--- MOUSE REPORT ---");

    Serial.printf("Button 1: %u\n", buttons & 0x01);
    Serial.printf("Button 2: %u\n", (buttons >> 1) & 0x01);
    Serial.printf("Relative X: %d\n", x);
    Serial.printf("Relative Y: %d\n", y);
}

// ============================================================
// Process an incoming input report
// ============================================================

void processInputReport()
{
    uint16_t reportLength;

    if (!readInputReport(reportLength))
        return;

    uint8_t reportID = inputBuffer[2];

    printRawReport(reportLength);

    Serial.printf("Report ID: 0x%02X\n", reportID);

    switch (reportID)
    {
        case 0x02:
            decodeMouseReport(reportLength);
            break;

        case 0x03:
            decodeTouchReport(reportLength);
            break;

        default:
            Serial.println("Unrecognized report ID.");
            break;
    }
}


// ============================================================
// Set HID Input Mode to Precision Touchpad
// ============================================================

bool enableMultitouchMode()
{
    Serial.println("Enabling multitouch mode...");

    // HID-over-I2C SET_REPORT transaction:
    //
    // 22 00       Command register (0x0022)
    // 34          Feature report type (3), Report ID (4)
    // 03          SET_REPORT opcode
    // 23 00       Data register (0x0023)
    // 04 00       Report length (4 bytes)
    // 04          Report ID (0x04)
    // 03          Input Mode = Precision Touchpad

    const uint8_t command[] =
    {
        0x22, 0x00,
        0x34, 0x03,
        0x23, 0x00,
        0x04, 0x00,
        0x04,
        0x03
    };

    Wire.beginTransmission(TRACKPAD_ADDR);

    Wire.write(command, sizeof(command));

    uint8_t error = Wire.endTransmission();

    if (error != 0)
    {
        Serial.print("Failed to set input mode. I2C error: ");
        Serial.println(error);
        return false;
    }

    Serial.println("Input mode command acknowledged.");

    // Give the device time to change reporting modes.
    delay(100);

    return true;
}

// ============================================================
// Arduino setup / loop
// ============================================================

void setup()
{
    Serial.begin(115200);

    while (!Serial && millis() < 3000)
        delay(10);

    Serial.println("\nInitializing trackpad...");

    // Configure I2C
    Wire.setSDA(I2C_SDA_PIN);
    Wire.setSCL(I2C_SCL_PIN);

    size_t actualBufferSize = Wire.setBufferSize(I2C_BUFFER_SIZE);

    Serial.print("I2C buffer size: ");
    Serial.println(actualBufferSize);

    Wire.begin();
    Wire.setClock(400000);
    Wire.setTimeout(1000);

    pinMode(TRACKPAD_INT_PIN, INPUT);

    delay(100);

    // Make sure no fingers are touching the trackpad.
    enableMultitouchMode();

    Serial.println("Waiting for touch reports...");

}

void loop()
{
    // Read reports while the active-low interrupt is asserted.
    // Limit reads per iteration to avoid getting stuck if the
    // interrupt remains asserted.

    uint8_t reportsRead = 0;

    while (digitalRead(TRACKPAD_INT_PIN) == LOW &&
           reportsRead < MAX_REPORTS_PER_LOOP)
    {
        processInputReport();
        reportsRead++;
    }

    delay(1);
}
