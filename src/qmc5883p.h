// qmc5883p.h
#pragma once

#include <limits.h>

#include <Arduino.h>
#include <Wire.h>

class QMC5883P {
public:
    enum class ODR {
        ODR_10Hz,
        ODR_50Hz,
        ODR_100Hz,
        ODR_200Hz
    };

    // Constructor: optionally other SDA/SCL pins & I²C address
    QMC5883P(ODR odr, uint8_t addr = 0x2C, TwoWire &bus = Wire);

    bool begin();                              // Init, true = OK
    bool readXYZ(float *xyz);                  // xyz[3] → µT, true = new data and calibrated
    float getHeadingDeg(float declDeg = 0.0f); // Heading calculation with internal data caching

    // Calling either of these disables auto-calibration
    void setHardIronOffsets(float xOff, float yOff, float zOff = 0.0f);
    void setSoftIronScales(float scaleX, float scaleY, float scaleZ = 1.0f);

private:
    ODR _odr;
    uint8_t _addr;
    TwoWire *_bus;
    bool _autoCalibrate = true;

    // Calibration parameters
    float _offX = 0.0f;
    float _offY = 0.0f;
    float _offZ = 0.0f;
    float _scaleX = 1.0f;
    float _scaleY = 1.0f;
    float _scaleZ = 1.0f;

    // Cache for raw measurement values and time
    int16_t _lastRawX, _lastRawY, _lastRawZ;

    int16_t _minRawX = INT16_MAX;
    int16_t _maxRawX = INT16_MIN;
    int16_t _minRawY = INT16_MAX;   
    int16_t _maxRawY = INT16_MIN;
    int16_t _minRawZ = INT16_MAX;
    int16_t _maxRawZ = INT16_MIN;

    bool readReg(uint8_t reg, uint8_t *buf, uint8_t len);
    bool writeReg(uint8_t reg, uint8_t val);
    bool readRaw(); // reads raw data, updates cache only if DRDY is set and enough time has passed
};
