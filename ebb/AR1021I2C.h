/*
 *  Copyright 2013 Embedded Artists AB
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef AR1021I2C_H
#define AR1021I2C_H

#include "TouchPanel.h"

/**
 * Microchip Touch Screen Controller (AR1021).
 *
 * Please note that this touch panel has an on-board storage for
 * calibration data. Once a successful calibration has been performed
 * it is not needed to do additional calibrations since the stored
 * calibration data will be used.
 */
class AR1021I2C : public TouchPanel {
public:


    /**
     * Constructor
     *
     * @param mosi I2C SDA pin
     * @param miso I2C SCL pin
     * @param siq interrupt pin
     */
    AR1021I2C(PinName sda, PinName scl, PinName siq);

    bool info(int* verHigh, int* verLow, int* resBits, int* type);

    virtual bool init(uint16_t width, uint16_t height);
    virtual bool read(touchCoordinate_t &coord);
    virtual bool calibrateStart();
    virtual bool getNextCalibratePoint(uint16_t* x, uint16_t* y);
    virtual bool waitForCalibratePoint(bool* morePoints, uint32_t timeout);

private:


    I2C _i2c;
    DigitalIn _siq;
    InterruptIn _siqIrq;
    bool _initialized;


    int32_t _x;
    int32_t _y;
    int32_t _pen;

    uint16_t _width;
    uint16_t _height;
    uint8_t _inset;

    int _calibPoint;


    int cmd(char cmd, char* data, int len, char* respBuf, int* respLen, bool setCsOff=true);
    int waitForCalibResponse(uint32_t timeout);
    void readTouchIrq();
};

#endif

