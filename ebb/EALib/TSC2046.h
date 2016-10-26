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

#ifndef TSC2046_H
#define TSC2046_H

#include "TouchPanel.h"

#define TSC2046_NUM_CALIB_POINTS (3)

/**
 * Texas Instruments Touch Screen Controller (TSC2046).
 */
class TSC2046 : public TouchPanel {
public:


    /**
     * Constructor
     *
     * @param mosi SPI MOSI pin
     * @param miso SPI MISO pin
     * @param sck SPI SCK pin
     * @param cs chip-select pin
     */
    TSC2046(PinName mosi, PinName miso, PinName sck, PinName cs);

    virtual bool init(uint16_t width, uint16_t height);

    virtual bool read(touchCoordinate_t &coord);
    virtual bool calibrateStart();
    virtual bool getNextCalibratePoint(uint16_t* x, uint16_t* y);
    virtual bool waitForCalibratePoint(bool* morePoints, uint32_t timeout);

    /**
     * Calibrate the touch panel with already gathered calibration values.
     * The list with calibration points must at one point have been retrieved
     * by calling getCalibrationValues;
     *
     * @param values list with calibration values
     * @param numValues the size of the list must match the number of
     * calibration points needed for this touch panel (TSC2046_NUM_CALIB_POINTS)
     *
     * @return true if the request was successful; otherwise false
     */
    bool calibrate(touchCoordinate_t* values, int numValues);

    /**
     * Get calibration values for the calibration points used by this touch
     * panel. This method may only be called after a successful calibration
     * has been performed.
     *
     * The list with values can be written to persistent storage and used
     * to calibrate the display without involving the user.
     *
     * @param values calibration values will be written to this list
     * @param numValues the size of the list must match the number of
     * calibration points needed for this touch panel (TSC2046_NUM_CALIB_POINTS)
     *
     * @return true if the request was successful; otherwise false
     */
    bool getCalibrationValues(touchCoordinate_t* values, int numValues);


private:

    typedef struct {
        int64_t x;
        int64_t y;
    } calibPoint_t;

    typedef struct {
        int64_t An;
        int64_t Bn;
        int64_t Cn;
        int64_t Dn;
        int64_t En;
        int64_t Fn;
        int64_t Divider;
    } calibMatrix_t;

    SPI _spi;
    DigitalOut _cs;
    bool _calibrated;
    bool _initialized;
    calibMatrix_t _calibMatrix;

    uint16_t _width;
    uint16_t _height;
    int _calibPoint;
    int _insetPx;

    touchCoordinate_t _calibrateValues[TSC2046_NUM_CALIB_POINTS][2];

    void readAndFilter(touchCoordinate_t &coord);
    int32_t getFilteredValue(int cmd);
    uint16_t spiTransfer(uint8_t cmd);

    void calibrate(touchCoordinate_t &ref1,
            touchCoordinate_t &ref2,
            touchCoordinate_t &ref3,
            touchCoordinate_t &scr1,
            touchCoordinate_t &scr2,
            touchCoordinate_t &scr3);

    void getCalibratePoint(int pointNum, int32_t* x, int32_t *y);
    int waitForTouch(int32_t* x, int32_t* y, uint32_t timeout);



    int setCalibrationMatrix( calibPoint_t * displayPtr,
            calibPoint_t * screenPtr,
            calibMatrix_t * matrixPtr);
    int getDisplayPoint( calibPoint_t * displayPtr,
            calibPoint_t * screenPtr,
            calibMatrix_t * matrixPtr );



};

#endif
