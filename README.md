# QMC5883P - Minimal Arduino Library

Lightweight and efficient driver for the QMC5883P / QMC5883L 3-axis compass (a clone of the older HMC5883L, found on most "GY-271" boards).

Fork: Adds auto-calibration & examples.

## Features

- **MCU Compatibility**: Tested on ESP32 & AVR, runs on any board with Wire library
- **Size**: ~120 B Flash, no dynamic memory allocation
- **API**: 4 simple high-level methods
- **License**: MIT

## 1. Hardware Connection

| Pin (QMC5883P) | Breakout Board Label | MCU Connection |
|----------------|---------------------|----------------|
| Vcc 3V–3V3     | VCC / 3V3          | 3.3V (NOT 5V!) |
| GND            | GND                | GND |
| SDA            | SDA                | Any I²C SDA pin (e.g., GPIO 21 on ESP32) |
| SCL            | SCL                | Any I²C SCL pin (e.g., GPIO 22 on ESP32) |

**Important Notes:**
- Pull-up resistors (4.7 kΩ) are usually already present on the module
- Keep the sensor several centimeters away from interference sources like speakers, motors, batteries, or the ESP32's WiFi antenna

## 2. Installation

Place the library in the libraries folder of your Arduino installation:

```
Arduino
└── libraries
    └── QMC5883P
        ├── qmc5883p.h
        ├── qmc5883p.cpp
        ├── library.properties
        └── README.md 
```

The Arduino IDE will automatically detect the library on the next startup.

## 3. Quick Start Example

This example shows basic usage without calibration.

```cpp
#include <Wire.h>
#include <qmc5883p.h>

QMC5883P mag;

void setup() {
  Serial.begin(115200);
  
  // Start I²C bus (SDA, SCL - adapt for your MCU)
  Wire.begin(21, 22);
  Wire.setClock(100000);

  if (!mag.begin()) {
    Serial.println("QMC5883P initialization failed!");
    while (true) delay(1000);
  }
  Serial.println("Sensor is ready.");
}

void loop() {
  float xyz[3];
  if (mag.readXYZ(xyz)) {
    // Calculate heading with magnetic declination (for Germany ~2.5°)
    float heading = mag.getHeadingDeg(/*decl*/ 2.5);

    Serial.printf("X:%.2f  Y:%.2f  Z:%.2f µT  |  Heading: %3.0f°\n",
                  xyz[0], xyz[1], xyz[2], heading);
  }
  delay(250);
}
```

## 4. Field Calibration (Hard & Soft Iron)

For precise measurements, calibration is essential. Perform this step once after final assembly in the sensor's final environment.

### Step-by-Step Instructions

1. **Upload Sketch**: Upload the calibration sketch (see Section 5 below or under `examples/qmc_calibrate.ino`) to your microcontroller.

2. **Perform Calibration**: Open the Serial Monitor (baudrate 115200) and follow the instructions. Slowly rotate the sensor in large, horizontal figure-eight movements for about 30 seconds to expose all axes to Earth's magnetic field.

3. **Copy Results**: After 30 seconds, the sketch will output the finished code lines for calibration. The result will look something like this:

```
=== CALIBRATION RESULTS ===
Offset X = -15.241 µT
Offset Y = 42.087 µT
Scale  X = 63.882 µT
Scale  Y = 62.110 µT
Average  = 62.996 µT

Copy these into your main sketch:
mag.setHardIronOffsets(-15.241f, 42.087f);
// Soft-Iron:
const float SCALE_AVG = 62.996f;
const float SCALE_X   = 63.882f;
const float SCALE_Y   = 62.110f;
```

4. **Insert Values**: Copy these lines and insert them at the appropriate places in your own sketch.

### Application Example with Calibration

Here's where to place the copied values from calibration in your main program:

```cpp
#include <Wire.h>
#include <qmc5883p.h>

QMC5883P mag;

// ===================================================================
// INSERT "SOFT-IRON" CALIBRATION VALUES HERE
// ===================================================================
const float SCALE_AVG = 62.996f;
const float SCALE_X   = 63.882f;
const float SCALE_Y   = 62.110f;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // Adapt SDA, SCL
  
  if (!mag.begin()) {
    Serial.println("Initialization failed!");
    while (true);
  }

  // ===================================================================
  // INSERT "HARD-IRON" CALIBRATION COMMAND HERE
  // ===================================================================
  mag.setHardIronOffsets(-15.241f, 42.087f);
  
  Serial.println("Sensor is ready and calibrated.");
}

void loop() {
  float xyz[3];
  if (mag.readXYZ(xyz)) {
    // Apply soft-iron correction
    xyz[0] *= SCALE_AVG / SCALE_X;
    xyz[1] *= SCALE_AVG / SCALE_Y;

    float heading = mag.getHeadingDeg(2.5); // Adjust declination
    Serial.printf("X:%.2f Y:%.2f Z:%.2f µT | Heading: %3.0f°\n",
                  xyz[0], xyz[1], xyz[2], heading);
  }
  delay(250);
}
```

## 5. Calibration Sketch

This sketch is used to determine the calibration values described above.

```cpp
/**
 *  QMC5883P – Calibration Sketch
 *  -----------------------------
 *  1. Upload sketch & open Serial Monitor @115200
 *  2. Slowly rotate sensor in large, horizontal figure-eight movements for ~30s
 *  3. Read the output offset and scaling values
 *  4. Copy the finished code lines into your own project
 */

#include <Wire.h>
#include <qmc5883p.h>

/* ---------- Pins (adapt for your MCU) ---------- */
const int SDA_PIN = 21;
const int SCL_PIN = 22;

/* ---------- Global Variables ---------- */
QMC5883P mag;
float minX =  1e6, maxX = -1e6;
float minY =  1e6, maxY = -1e6;
unsigned long t0;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!mag.begin()) {
    Serial.println("Init FAILED – check wiring!");
    while (1) delay(1000);
  }
  Serial.println("\n>>> START CALIBRATION <<<");
  Serial.println("Rotate module in large figure-eight movements for 30s.");
  t0 = millis();
}

void loop() {
  float v[3];
  if (mag.readXYZ(v)) {               // Values are already in µT
    minX = min(minX, v[0]);  maxX = max(maxX, v[0]);
    minY = min(minY, v[1]);  maxY = max(maxY, v[1]);
  }

  /* Stop after 30 seconds */
  if (millis() - t0 > 30000) {
    float offX   = (maxX + minX) / 2.0f;
    float offY   = (maxY + minY) / 2.0f;
    float scaleX = (maxX - minX) / 2.0f;
    float scaleY = (maxY - minY) / 2.0f;
    float avg    = (scaleX + scaleY) / 2.0f;

    Serial.println("\n=== CALIBRATION RESULTS ===");
    Serial.printf("Offset X = %.3f µT\n", offX);
    Serial.printf("Offset Y = %.3f µT\n", offY);
    Serial.printf("Scale  X = %.3f µT\n", scaleX);
    Serial.printf("Scale  Y = %.3f µT\n", scaleY);
    Serial.printf("Average  = %.3f µT\n", avg);

    Serial.println("\nCopy these lines into your main sketch:");
    Serial.println("mag.setHardIronOffsets(" + String(offX, 3) +
                   "f, " + String(offY, 3) + "f);");
    Serial.println("// Soft-Iron:");
    Serial.println("const float SCALE_AVG = " + String(avg, 3) + "f;");
    Serial.println("const float SCALE_X   = " + String(scaleX, 3) + "f;");
    Serial.println("const float SCALE_Y   = " + String(scaleY, 3) + "f;");

    while (1);                        // Done
  }
}
```

## 6. API Reference

| Method | Description |
|--------|-------------|
| `bool begin()` | Initializes the sensor (200 Hz, ±2 G). Returns false if chip ID is not 0x80. |
| `bool readXYZ(float xyz[3])` | Reads the latest raw magnetic field data (in µT) and subtracts set offsets. Returns false if no new data is available. |
| `float getHeadingDeg(float decl=0)` | Helper function to calculate heading in degrees (0…360°). Accepts magnetic declination in degrees (East = positive). |
| `void setHardIronOffsets(float xOff, float yOff)` | Stores offsets for hard-iron interference (in µT), which are automatically subtracted from raw values on each `readXYZ()` call. |

## 7. Tips for Low-Noise Measurements

| Tip | Effect |
|-----|--------|
| Lower ODR to 50 Hz (`mag.writeReg(0x0A, 0x9F)`) | Less sensor self-noise |
| Apply averaging or low-pass filter over ~10 measurements | Reduces jitter to ~±0.3° |
| Keep sensor >3 cm away from WiFi antennas, coils, etc. | Reduces drift and interference |
| Use 4.7 kΩ pull-up resistors and 100 kHz I²C clock | Ensures clean bus signals |

For tilt compensation, the sensor can be combined with an accelerometer (e.g., MPU-6050) or an IMU (e.g., BNO-055).

## 8. License

MIT – You can do whatever you want with this library as long as the original author is mentioned.
