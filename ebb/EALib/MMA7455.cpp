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

/******************************************************************************
 * Includes
 *****************************************************************************/

#include "mbed.h"
#include "mbed_debug.h"

#include "MMA7455.h"

/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/

#define MMA7455_I2C_ADDR (0x1D << 1)

#define MMA7455_ADDR_XOUTL  0x00
#define MMA7455_ADDR_XOUTH  0x01
#define MMA7455_ADDR_YOUTL  0x02
#define MMA7455_ADDR_YOUTH  0x03
#define MMA7455_ADDR_ZOUTL  0x04
#define MMA7455_ADDR_ZOUTH  0x05
#define MMA7455_ADDR_XOUT8  0x06
#define MMA7455_ADDR_YOUT8  0x07
#define MMA7455_ADDR_ZOUT8  0x08
#define MMA7455_ADDR_STATUS 0x09
#define MMA7455_ADDR_DETSRC 0x0A
#define MMA7455_ADDR_TOUT   0x0B
#define MMA7455_ADDR_I2CAD  0x0D
#define MMA7455_ADDR_USRINF 0x0E
#define MMA7455_ADDR_WHOAMI 0x0F
#define MMA7455_ADDR_XOFFL  0x10
#define MMA7455_ADDR_XOFFH  0x11
#define MMA7455_ADDR_YOFFL  0x12
#define MMA7455_ADDR_YOFFH  0x13
#define MMA7455_ADDR_ZOFFL  0x14
#define MMA7455_ADDR_ZOFFH  0x15
#define MMA7455_ADDR_MCTL   0x16
#define MMA7455_ADDR_INTRST 0x17
#define MMA7455_ADDR_CTL1   0x18
#define MMA7455_ADDR_CTL2   0x19
#define MMA7455_ADDR_LDTH   0x1A
#define MMA7455_ADDR_PDTH   0x1B
#define MMA7455_ADDR_PW     0x1C
#define MMA7455_ADDR_LT     0x1D
#define MMA7455_ADDR_TW     0x1E

#define MMA7455_MCTL_MODE(m) ((m) << 0)
#define MMA7455_MCTL_GLVL(g) ((g) << 2)

#define MMA7455_STATUS_DRDY (1 << 0)
#define MMA7455_STATUS_DOVR (1 << 1)
#define MMA7455_STATUS_PERR (1 << 2)


MMA7455::MMA7455(PinName sda, PinName scl) : _i2c(sda, scl)
{
    _mode = ModeStandby;
    _range = Range_8g;

    _xOff = 0;
    _yOff = 0;
    _zOff = 0;
}

bool MMA7455::setMode(Mode mode) {
    bool result = false;
    int mCtrl = 0;

    do {
        mCtrl = getModeControl();
        if (mCtrl < 0) break;

        mCtrl &= ~(0x03 << 0);
        mCtrl |= MMA7455_MCTL_MODE(mode);

        if (setModeControl((uint8_t)mCtrl) < 0) {
            break;
        }

        _mode = mode;
        result = true;
    } while(0);



    return result;
}

bool MMA7455::setRange(Range range) {
    bool result = false;
    int mCtrl = 0;

    do {
        mCtrl = getModeControl();
        if (mCtrl < 0) break;

        mCtrl &= ~(0x03 << 2);
        mCtrl |= MMA7455_MCTL_GLVL(range);

        if (setModeControl((uint8_t)mCtrl) < 0) {
            break;
        }

        _range = range;
        result = true;
    } while(0);



    return result;

}

bool MMA7455::read(int32_t& x, int32_t& y, int32_t& z) {
    bool result = false;


    // nothing to read in standby mode
    if (_mode == ModeStandby) return false;

    // wait for ready flag
    int status = 0;
    do {
      status = getStatus();
    } while (status >= 0 && (status & MMA7455_STATUS_DRDY) == 0);


    do {
        if (status < 0) break;


        char buf[6];
        buf[0] = MMA7455_ADDR_XOUTL;
        if (_i2c.write(MMA7455_I2C_ADDR, buf, 1) != 0) break;
        if (_i2c.read(MMA7455_I2C_ADDR, buf, 6) != 0) break;

        // check if second bit is set in high byte -> negative value
        // expand negative value to full byte
        if (buf[1] & 0x02) buf[1] |= 0xFC;
        if (buf[3] & 0x02) buf[3] |= 0xFC;
        if (buf[5] & 0x02) buf[5] |= 0xFC;

        x = (int16_t)((buf[1] << 8) | buf[0]) + _xOff;
        y = (int16_t)((buf[3] << 8) | buf[2]) + _yOff;
        z = (int16_t)((buf[5] << 8) | buf[4]) + _zOff;


        result = true;

    } while(0);


    return result;
}

bool MMA7455::calibrate() {
    bool result = false;
    bool failed = false;

    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;

    int32_t xr = 0;
    int32_t yr = 0;
    int32_t zr = 0;

    int xOff = 0;
    int yOff = 0;
    int zOff = 16;
    if (_range == Range_2g) {
        zOff = 64;
    }
    if (_range == Range_4g) {
        zOff = 32;
    }

    do {

        // get an average of 6 values
        for (int i = 0; i < 6; i++) {
            if (!read(xr, yr, zr)) {
                failed = true;
                break;
            }
            x += xr;
            y += yr;
            z += zr;

            wait_ms(100);
        }

        if (failed) break;
        x /= 6;
        y /= 6;
        z /= 6;

        xOff -= x;
        yOff -= y;
        zOff -= z;

        /*
         * For some reason we have not got correct/reliable calibration
         * by using the offset drift registers. Instead we are
         * calculating the offsets and store them in member variables.
         *
         * These member variables are then used in the read() method
         */

        _xOff = xOff;
        _yOff = yOff;
        _zOff = zOff;


        result = true;

    } while (0);



    return result;
}

bool MMA7455::setCalibrationOffsets(int32_t xOff, int32_t yOff, int32_t zOff) {
    _xOff = xOff;
    _yOff = yOff;
    _zOff = zOff;

    return true;
}

bool MMA7455::getCalibrationOffsets(int32_t& xOff, int32_t& yOff, int32_t& zOff) {
    xOff = _xOff;
    yOff = _yOff;
    zOff = _zOff;

    return true;
}

int MMA7455::getStatus() {
    int result = -1;
    char data[1];

    do {
        data[0] = MMA7455_ADDR_STATUS;
        if (_i2c.write(MMA7455_I2C_ADDR, data, 1) != 0) break;

        if (_i2c.read(MMA7455_I2C_ADDR, data, 1) != 0) break;

        result = data[0];

    } while (0);



    return result;
}

int MMA7455::getModeControl() {

    int result = -1;
    char data[1];

    do {
        data[0] = MMA7455_ADDR_MCTL;
        if (_i2c.write(MMA7455_I2C_ADDR, data, 1) != 0) break;

        if (_i2c.read(MMA7455_I2C_ADDR, data, 1) != 0) break;

        result = data[0];

    } while (0);



    return result;
}

int MMA7455::setModeControl(uint8_t mctl) {
    int result = -1;
    char data[2];

    do {
        data[0] = MMA7455_ADDR_MCTL;
        data[1] = (char)mctl;
        if (_i2c.write(MMA7455_I2C_ADDR, data, 2) != 0) break;

        result = 0;

    } while (0);



    return result;
}


