#include <Wire.h>
#include <qmc5883p.h>

QMC5883P mag;

void setup() {
    Serial.begin(115200);
    
    // Start I²C bus (SDA, SCL - adapt for your MCU)
    Wire.begin(21, 22);
    Wire.setClock(100000);

    Serial.println("Initializing QMC5883P magnetometer...");
    if (!mag.begin()) {
        Serial.println("QMC5883P initialization failed!");
        while (true);
    }
    Serial.println("QMC5883P initialized successfully.");
}

void loop() {
    float xyz[3];
    if (mag.readXYZ(xyz)) {
        // Calculate heading with magnetic declination (for Germany ~2.5°)
        float heading = mag.getHeadingDeg(/*decl*/ 2.5);

        Serial.print("X:");
        Serial.print(xyz[0], 2);
        Serial.print("  Y:");
        Serial.print(xyz[1], 2);
        Serial.print("  Z:");
        Serial.print(xyz[2], 2);
        Serial.print(" µT  |  Heading: ");
        Serial.print(heading, 1);
        Serial.println("°");
    }
    delay(250);
}