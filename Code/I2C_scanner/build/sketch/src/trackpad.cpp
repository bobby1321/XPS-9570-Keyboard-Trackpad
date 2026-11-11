#line 1 "/media/windowsmount/Users/bapel/Documents/GitHub/XPS_KBTP_Combo/Code/I2C_scanner/src/trackpad.cpp"
#include <Arduino.h>
#include <Wire.h>
#include "trackpad.h"

#define TRACKPAD_ADDRESS 0x2C

void trackpad_initialize() {
    Serial.println("Initializing trackpad investigation...");
    Serial.println("I2C address: 0x2C");
}

void trackpad_probe_registers() {
    Serial.println("\nProbing possible descriptor locations...");

    // Read-only diagnostic.
    // These are candidate addresses, NOT confirmed
    // S910ZB register definitions.

    const uint8_t candidates[] = {
        0x00, 0x01, 0x02, 0x03,
        0x10, 0x20, 0x30, 0x40
    };

    for (uint8_t reg : candidates) {
        Wire.beginTransmission(TRACKPAD_ADDRESS);
        Wire.write(reg);

        uint8_t error = Wire.endTransmission(false);

        if (error != 0) {
            Serial.print("Register selection failed: 0x");
            Serial.println(reg, HEX);
            continue;
        }

        uint8_t received =
            Wire.requestFrom(TRACKPAD_ADDRESS, (uint8_t)16);

        Serial.print("Register 0x");
        Serial.print(reg, HEX);
        Serial.print(": ");

        while (Wire.available()) {
            uint8_t value = Wire.read();

            if (value < 0x10) Serial.print("0");
            Serial.print(value, HEX);
            Serial.print(" ");
        }

        Serial.println();
    }
}
