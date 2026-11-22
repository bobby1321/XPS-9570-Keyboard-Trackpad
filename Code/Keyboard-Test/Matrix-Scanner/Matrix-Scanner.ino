
#include <Wire.h>

#define ECE1117_ADDR 0x38

// Keyboard scan output lines
const uint8_t ksoLines[] = {
    0, 1, 2, 3, 4, 5, 6,
    11, 12, 13, 14, 15, 16, 17,
    18, 19, 20, 21, 22
};

#define KSO_COUNT (sizeof(ksoLines) / sizeof(ksoLines[0]))

// Previous state of each matrix position.
// 1 = released, 0 = pressed
uint8_t previousState[KSO_COUNT][8];

// ------------------------------------------------------------
// SMBus register access
// ------------------------------------------------------------

bool writeRegister(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(ECE1117_ADDR);
    Wire.write(reg);
    Wire.write(value);

    return Wire.endTransmission() == 0;
}

bool readRegister(uint8_t reg, uint8_t *value)
{
    Wire.beginTransmission(ECE1117_ADDR);
    Wire.write(reg);

    if (Wire.endTransmission(false) != 0)
        return false;

    if (Wire.requestFrom(ECE1117_ADDR, (uint8_t)1) != 1)
        return false;

    *value = Wire.read();
    return true;
}

// ------------------------------------------------------------
// Keyboard matrix initialization
// ------------------------------------------------------------

void initializeKeyboard()
{
    // Enable SMBus interface
    writeRegister(0xFA, 0x03);

    // Enable keyboard scanner, initially disabled for setup
    writeRegister(0x40, 0x40);

    // Initialize all positions as released
    for (uint8_t row = 0; row < KSO_COUNT; row++)
    {
        for (uint8_t col = 0; col < 8; col++)
        {
            previousState[row][col] = 1;
        }
    }
}

// ------------------------------------------------------------
// Keyboard matrix scanning
// ------------------------------------------------------------

void scanKeyboard()
{
    for (uint8_t row = 0; row < KSO_COUNT; row++)
    {
        uint8_t ksi;

        // Select KSO output and enable scanning
        if (!writeRegister(0x40, ksoLines[row]))
            continue;

        delayMicroseconds(200);

        // Read the eight KSI inputs
        if (!readRegister(0x41, &ksi))
            continue;

        for (uint8_t col = 0; col < 8; col++)
        {
            // KSI input is active low
            uint8_t currentState = (ksi >> col) & 0x01;

            // Detect new key press
            if (previousState[row][col] == 1 &&
                currentState == 0)
            {
                Serial.print("KEY PRESS: KSO");
                Serial.print(ksoLines[row]);

                Serial.print(" / KSI");
                Serial.print(col);

                Serial.print("  (Matrix: ");
                Serial.print(row);
                Serial.print(", ");
                Serial.print(col);
                Serial.println(")");
            }

            // Update previous state
            previousState[row][col] = currentState;
        }
    }

    // Disable scanning between complete scans
    writeRegister(0x40, 0x40);
}


void configureAdditionalKSO()
{
    const uint8_t registers[] = {
        0x0B, // GPIO01 / KSO18
        0x0A, // GPIO00 / KSO19
        0x1D, // GPIO23 / KSO20
        0x1C, // GPIO22 / KSO21
        0x1B  // GPIO21 / KSO22
    };

    const char *names[] = {
        "KSO18", "KSO19", "KSO20", "KSO21", "KSO22"
    };

    Serial.println("\nConfiguring additional KSO outputs:");

    for (uint8_t i = 0; i < 5; i++)
    {
        uint8_t value;

        if (!readRegister(registers[i], &value))
        {
            Serial.print("Failed to read ");
            Serial.println(names[i]);
            continue;
        }

        Serial.print(names[i]);
        Serial.print(" register before: 0x");
        Serial.println(value, HEX);

        // Set mux bits [7:6] to 01 (KSO function).
        // Preserve all other configuration bits.
        uint8_t newValue = (value & 0x3F) | 0x40;

        if (!writeRegister(registers[i], newValue))
        {
            Serial.print("Failed to configure ");
            Serial.println(names[i]);
            continue;
        }

        Serial.print(names[i]);
        Serial.print(" register after: 0x");
        Serial.println(newValue, HEX);
    }
}

// ------------------------------------------------------------
// Arduino setup and loop
// ------------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Wire.begin();
    Wire.setClock(100000);

    Serial.println("Initializing ECE1117...");

    initializeKeyboard();

    configureAdditionalKSO();

    Serial.println("Keyboard matrix scanner ready.");
    Serial.println("Press keys to display their matrix coordinates.");
}

void loop()
{
    scanKeyboard();
}
