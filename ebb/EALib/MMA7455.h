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

#ifndef MMA7455_H
#define MMA7455_H


/**
 * Freescale Accelerometer MMA7455.
 */
class MMA7455 {
public:

    enum Mode {
        ModeStandby = 0,
        ModeMeasurement = 1,
    };

    /** Acceleration range */
    enum Range {
        Range_8g = 0,
        Range_2g = 1,
        Range_4g = 2
    };

    /**
     * Create an interface to the MMA7455 accelerometer
     *
     * @param sda I2C data line pin
     * @param scl I2C clock line pin
     */
    MMA7455(PinName sda, PinName scl);

    bool setMode(Mode mode);
    bool setRange(Range range);

    bool read(int32_t& x, int32_t& y, int32_t& z);

    /**
     * Calibrate for 0g, that is, calculate offset to achieve
     * 0g values when accelerometer is placed on flat surface.
     *
     * Please make sure the accelerometer is placed on a flat surface before
     * calling this function.
     *
     * @return true if request was successful; otherwise false
     */
    bool calibrate();

    /**
     * Get calculated offset values. Offsets will be calculated by the
     * calibrate() method.
     *
     * Use these values and put them in persistent storage to avoid
     * having to calibrate the accelerometer after a reset/power cycle.
     *
     * @param xOff x offset is written to this argument
     * @param yOff y offset is written to this argument
     * @param zOff z offset is written to this argument
     *
     * @return true if request was successful; otherwise false
     */
    bool getCalibrationOffsets(int32_t& xOff, int32_t& yOff, int32_t& zOff);

    /**
     * Set calibration offset values. These values should normally
     * at one point in time have been retrieved by calling the
     * getCalibrationOffsets method.
     *
     *
     * @param xOff x offset
     * @param yOff y offset
     * @param zOff z offset
     *
     * @return true if request was successful; otherwise false
     */
    bool setCalibrationOffsets(int32_t xOff, int32_t yOff, int32_t zOff);



private:

    I2C _i2c;
    Mode _mode;
    Range _range;
    int32_t _xOff;
    int32_t _yOff;
    int32_t _zOff;

    int getStatus();
    int getModeControl();
    int setModeControl(uint8_t mctl);

};

#endif
