
#include <Wire.h>

#define ECE1117_ADDR  0x38  // Change to 0x39 if SMB_ADDR is high

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

    // Repeated start, followed by a one-byte read
    if (Wire.endTransmission(false) != 0)
        return false;

    if (Wire.requestFrom(ECE1117_ADDR, (uint8_t)1) != 1)
        return false;

    *value = Wire.read();
    return true;
}


void scanKeyboard()
{
    const uint8_t ksoLines[] = {
        0, 1, 2, 3, 4, 5, 6,
        11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22
    };

    Serial.println("\nScanning keyboard matrix...");
    Serial.println("Hold a key while scanning.");

    for (uint8_t i = 0; i < sizeof(ksoLines); i++)
    {
        uint8_t kso = ksoLines[i];
        uint8_t ksi;

        if (!writeRegister(0x40, kso))
        {
            Serial.println("KSO write failed.");
            continue;
        }

        delay(2);

        if (readRegister(0x41, &ksi))
        {
            Serial.print("KSO");
            Serial.print(kso);
            Serial.print(" : KSI = 0x");
            Serial.println(ksi, HEX);
        }
    }

    // Disable keyboard scanning again.
    writeRegister(0x40, 0x40);

    Serial.println("Scan complete.");
}


void setup()
{
    Serial.begin(115200);
    delay(1000);

    Wire.begin();
    Wire.setClock(100000);

    // IMPORTANT:
    // This must be the first access to the ECE1117.
    Serial.println("Configuring ECE1117 SMBus interface...");

    if (!writeRegister(0xFA, 0x03))
    {
        Serial.println("ERROR: Failed to configure SMBus.");
        return;
    }

    Serial.println("SMBus configured.");

    const uint8_t registers[] = {
        0xFC, 0xFD, 0xFE, 0xFF
    };

    for (uint8_t i = 0; i < sizeof(registers); i++)
    {
        uint8_t value;

        if (readRegister(registers[i], &value))
        {
            Serial.print("Register 0x");
            Serial.print(registers[i], HEX);
            Serial.print(" = 0x");
            Serial.println(value, HEX);
        }
        else
        {
            Serial.print("Failed to read register 0x");
            Serial.println(registers[i], HEX);
        }
    }

    
    Serial.println("\nKeyboard scanner registers:");

    const uint8_t keyRegisters[] = {
        0x40, 0x41, 0x42, 0x43
    };

    for (uint8_t i = 0; i < sizeof(keyRegisters); i++)
    {
        uint8_t value;

        if (readRegister(keyRegisters[i], &value))
        {
            Serial.print("Register 0x");
            Serial.print(keyRegisters[i], HEX);
            Serial.print(" = 0x");
            Serial.println(value, HEX);
        }
        else
        {
            Serial.print("Failed to read register 0x");
            Serial.println(keyRegisters[i], HEX);
        }
    }

    
    Serial.println("\nTesting KSO00...");

    if (writeRegister(0x40, 0x00))
    {
        delay(10);

        uint8_t kso;
        uint8_t ksi;

        if (readRegister(0x40, &kso))
        {
            Serial.print("KSO Select = 0x");
            Serial.println(kso, HEX);
        }

        if (readRegister(0x41, &ksi))
        {
            Serial.print("KSI Input  = 0x");
            Serial.println(ksi, HEX);
        }
    }
    else
    {
        Serial.println("Failed to enable KSO00.");
    }

    // Restore the original disabled state.
    writeRegister(0x40, 0x40);

    scanKeyboard();


}

void loop()
{
}
