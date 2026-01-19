// qmc5883p.cpp
#include "qmc5883p.h"
#include <Arduino.h>

/* Register addresses */
#define REG_CHIP_ID        0x00
#define REG_DATA_OUT_X_LSB 0x01
#define REG_STATUS         0x09
#define REG_CTL1           0x0A
#define REG_CTL2           0x0B

QMC5883P::QMC5883P(ODR odr, uint8_t addr, TwoWire &bus)
    : _odr(odr),_addr(addr), _bus(&bus),
      _lastRawX(0), _lastRawY(0), _lastRawZ(0)
       {}

bool QMC5883P::begin() {
    _bus->begin(); // Set pins beforehand in sketch

    uint8_t id;
    if (!readReg(REG_CHIP_ID, &id, 1) || id != 0x80) return false;

    // Standard configuration: Continuous, ±2G
    uint8_t control = 0xC3;
    if (_odr == ODR::ODR_10Hz)       control |= 0x00;
    else if (_odr == ODR::ODR_50Hz)  control |= 0x04;
    else if (_odr == ODR::ODR_100Hz) control |= 0x08;
    else if (_odr == ODR::ODR_200Hz) control |= 0x0C;

    writeReg(0x0D, 0x40); delay(10);
    writeReg(0x29, 0x06); delay(10);
    writeReg(REG_CTL1, control); delay(10);
    writeReg(REG_CTL2, 0x00); delay(10);

    return true;
}

bool QMC5883P::readRaw() {
    uint8_t status;
    if (!readReg(REG_STATUS, &status, 1) || !(status & 0x01)) {
        // no new data
        return false;
    }

    uint8_t buf[6];
    if (!readReg(REG_DATA_OUT_X_LSB, buf, 6)) return false;

    _lastRawX = int16_t(buf[1] << 8 | buf[0]);
    _lastRawY = int16_t(buf[3] << 8 | buf[2]);
    _lastRawZ = int16_t(buf[5] << 8 | buf[4]);

    if (!_autoCalibrate) 
        return true;

    // --- auto calibration ---
    int16_t minRawX = min(_minRawX, _lastRawX);
    int16_t maxRawX = max(_maxRawX, _lastRawX);
    int16_t minRawY = min(_minRawY, _lastRawY);
    int16_t maxRawY = max(_maxRawY, _lastRawY);
    int16_t minRawZ = min(_minRawZ, _lastRawZ);
    int16_t maxRawZ = max(_maxRawZ, _lastRawZ);

    if (minRawY < _minRawX || maxRawX > _maxRawX ||
        minRawY < _minRawY || maxRawY > _maxRawY ||
        minRawZ < _minRawZ || maxRawZ > _maxRawZ)
    {
        _minRawX = minRawX;
        _maxRawX = maxRawX;
        _minRawY = minRawY;
        _maxRawY = maxRawY;
        _minRawZ = minRawZ;
        _maxRawZ = maxRawZ;

        if (_maxRawX - _minRawX > 2000 ||
            _maxRawY - _minRawY > 2000 ||
            _maxRawZ - _minRawZ > 2000) {
            // reset automatic recalibration
            _minRawX = INT16_MAX;
            _maxRawX = INT16_MIN;
            _minRawY = INT16_MAX;
            _maxRawY = INT16_MIN;
            _minRawZ = INT16_MAX;
            _maxRawZ = INT16_MIN;
#if 0            
            Serial.println("QMC5883P: Auto recalibration reset");
#endif
        }
        else if (_maxRawX - _minRawX > 200 
            && _maxRawY - _minRawY > 200 
            && _maxRawZ - _minRawZ > 500) 
        {
            // auto update hard-iron offsets
            _offX = (_maxRawX + _minRawX) / 2.0f / 1000.0f;
            _offY = (_maxRawY + _minRawY) / 2.0f / 1000.0f;
            _offZ = (_maxRawZ + _minRawZ) / 2.0f / 1000.0f;

            _scaleX = (_maxRawX - _minRawX) / 2000.0f;
            _scaleY = (_maxRawY - _minRawY) / 2000.0f;
            _scaleZ = (_maxRawZ - _minRawZ) / 2000.0f;

#if 0            
            Serial.print("QMC5883P: updated auto calibration");
            Serial.print(" Offesets: ");
            Serial.print(_offX, 3); Serial.print(", ");
            Serial.print(_offY, 3); Serial.print(", ");
            Serial.print(_offZ, 3); Serial.print(" | Scales: ");
            Serial.print(_scaleX, 3); Serial.print(", ");   
            Serial.print(_scaleY, 3); Serial.print(", ");
            Serial.println(_scaleZ, 3);
#endif
        }
    }
    return true;
}

bool QMC5883P::readXYZ(float *xyz) {
    if (!readRaw()) return false; // no new data

    // Raw data → µT and calibrate (Hard- + Soft-Iron)
    float x = _lastRawX / 1000.0f;
    float y = _lastRawY / 1000.0f;
    float z = _lastRawZ / 1000.0f;

    xyz[0] = (x - _offX) * _scaleX;
    xyz[1] = (y - _offY) * _scaleY;
    xyz[2] = (z - _offZ) * _scaleZ;
    return true;
}

float QMC5883P::getHeadingDeg(float declDeg) {
    // try to read new data, otherwise use last cache
    readRaw();

    // same conversion as in readXYZ, without return value
    float x = (_lastRawX / 1000.0f - _offX) * _scaleX;
    float y = (_lastRawY / 1000.0f - _offY) * _scaleY;

    // 1) Basic angle (-π … π)
    float hdg = atan2(y, x);
    // 2) Add declination (deg → rad)
    hdg += declDeg * DEG_TO_RAD;
    // 3) Normalize to 0 … 2π
    if (hdg < 0)        hdg += TWO_PI;
    else if (hdg > TWO_PI) hdg -= TWO_PI;
    // 4) to degrees
    return hdg * RAD_TO_DEG;
}

void QMC5883P::setHardIronOffsets(float xOff, float yOff, float zOff) {
    _offX = xOff;
    _offY = yOff;
    _offZ = zOff;
    _autoCalibrate = false;
}

void QMC5883P::setSoftIronScales(float scaleX, float scaleY, float scaleZ) {
    _scaleX = scaleX;
    _scaleY = scaleY;
    _scaleZ = scaleZ;
    _autoCalibrate = false;
}

bool QMC5883P::readReg(uint8_t reg, uint8_t *buf, uint8_t len) {
    _bus->beginTransmission(_addr);
    _bus->write(reg);
    if (_bus->endTransmission(false) != 0) return false;
    if (_bus->requestFrom(_addr, len) != len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = _bus->read();
    return true;
}

bool QMC5883P::writeReg(uint8_t reg, uint8_t val) {
    _bus->beginTransmission(_addr);
    _bus->write(reg);
    _bus->write(val);
    return (_bus->endTransmission() == 0);
}
