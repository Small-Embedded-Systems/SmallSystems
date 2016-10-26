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

#include "mbed.h"
#include "mbed_debug.h"
#include "TSC2046.h"

#ifndef ABS
#define ABS(x) (  ((int32_t)(x)) < 0 ? (-(x)) : (x))
#endif

#define ADS_START         (1 << 7)
#define ADS_A2A1A0_d_y    (1 << 4)  /* differential */
#define ADS_A2A1A0_d_z1   (3 << 4)  /* differential */
#define ADS_A2A1A0_d_z2   (4 << 4)  /* differential */
#define ADS_A2A1A0_d_x    (5 << 4)  /* differential */
#define ADS_A2A1A0_temp0  (0 << 4)  /* non-differential */
#define ADS_A2A1A0_vbatt  (2 << 4)  /* non-differential */
#define ADS_A2A1A0_vaux   (6 << 4)  /* non-differential */
#define ADS_A2A1A0_temp1  (7 << 4)  /* non-differential */
#define ADS_8_BIT         (1 << 3)
#define ADS_12_BIT        (0 << 3)
#define ADS_SER           (1 << 2)  /* non-differential */
#define ADS_DFR           (0 << 2)  /* differential */
#define ADS_PD10_PDOWN    (0 << 0)  /* lowpower mode + penirq */
#define ADS_PD10_ADC_ON   (1 << 0)  /* ADC on */
#define ADS_PD10_REF_ON   (2 << 0)  /* vREF on + penirq */
#define ADS_PD10_ALL_ON   (3 << 0)  /* ADC + vREF on */


#define READ_12BIT_DFR(d, adc, vref) (ADS_START | d \
  | ADS_12_BIT | ADS_DFR | \
  (adc ? ADS_PD10_ADC_ON : 0) | (vref ? ADS_PD10_REF_ON : 0))

#define READ_Y(vref)  (READ_12BIT_DFR(ADS_A2A1A0_d_y,  1, vref))
#define READ_Z1(vref) (READ_12BIT_DFR(ADS_A2A1A0_d_z1, 1, vref))
#define READ_Z2(vref) (READ_12BIT_DFR(ADS_A2A1A0_d_z2, 1, vref))
#define READ_X(vref)  (READ_12BIT_DFR(ADS_A2A1A0_d_x,  1, vref))
#define PWRDOWN       (READ_12BIT_DFR(ADS_A2A1A0_d_y,  0, 0))  /* LAST */

/* single-ended samples need to first power up reference voltage;
 * we leave both ADC and VREF powered
 */
#define READ_12BIT_SER(x) (ADS_START | x \
  | ADS_12_BIT | ADS_SER)

#define REF_ON  (READ_12BIT_DFR(ADS_A2A1A0_d_x, 1, 1))
#define REF_OFF (READ_12BIT_DFR(ADS_A2A1A0_d_y, 0, 0))

#define DEBOUNCE_MAX 10
#define DEBOUNCE_TOL  3


TSC2046::TSC2046(PinName mosi, PinName miso, PinName sck, PinName cs) :
_spi(mosi, miso, sck), _cs(cs)
{
    _cs = 1; // active low

    _spi.format(8, 3);
    
    // We are limiting the clock rate to 500000 since
    // we have experienced a lot of noise when running with
    // higher rate. It has not been examined why there is a
    // lot of noise with higher rate.
    _spi.frequency(500000);
    _calibrated = false;
    _initialized = false;

    _calibPoint = TSC2046_NUM_CALIB_POINTS+1;

    _insetPx = 15;
}

bool TSC2046::init(uint16_t width, uint16_t height) {
    _width = width;
    _height = height;

    _cs = 0;

    _spi.write(REF_ON);
    _spi.write((READ_12BIT_SER(ADS_A2A1A0_vaux) | ADS_PD10_ALL_ON));
    _spi.write(PWRDOWN);

    _cs = 1;


    _initialized = true;

    return true;
}


bool TSC2046::read(touchCoordinate_t &coord) {

    touchCoordinate_t tmpCoord;
    calibPoint_t displayPoint;
    calibPoint_t screenSample;

    if (!_initialized) return false;


    readAndFilter(tmpCoord);

    _cs = 0;
    _spi.write(PWRDOWN);
    _cs = 1;

    coord.z = tmpCoord.z;

    if (_calibrated) {
        screenSample.x = tmpCoord.x;
        screenSample.y = tmpCoord.y;

        getDisplayPoint(&displayPoint, &screenSample, &_calibMatrix);

        coord.x = displayPoint.x;
        coord.y = displayPoint.y;
    }
    else {
        coord.x = tmpCoord.x;
        coord.y = tmpCoord.y;
    }

    return true;

}

bool TSC2046::calibrateStart() {
    if (!_initialized) return false;

    _calibPoint = 0;

    return true;
}

bool TSC2046::getNextCalibratePoint(uint16_t* x, uint16_t* y) {
    touchCoordinate_t coord;

    if (!_initialized) return false;

    if (x == NULL || y == NULL) return false;

    if (_calibPoint >= TSC2046_NUM_CALIB_POINTS) return false;

    getCalibratePoint(_calibPoint, &coord.x, &coord.y);

    *x = (uint16_t)coord.x;
    *y = (uint16_t)coord.y;
    _calibrateValues[_calibPoint][0] = coord;

    return true;
}

bool TSC2046::waitForCalibratePoint(bool* morePoints, uint32_t timeout) {
    int result = 0;
    bool ret = false;
    int32_t x = 0;
    int32_t y = 0;
    touchCoordinate_t coord;

    if (!_initialized) return false;

    do {
        if (morePoints == NULL || _calibPoint >= TSC2046_NUM_CALIB_POINTS) {
            break;
        }

        result = waitForTouch(&x, &y, timeout);
        if (result != 0) {
            debug("wait for touch response failed (%d)\n", result);
            break;
        }

        coord.x = x;
        coord.y = y;
        _calibrateValues[_calibPoint][1] = coord;

        _calibPoint++;
        *morePoints = (_calibPoint < TSC2046_NUM_CALIB_POINTS);

        if (!(*morePoints)) {

            calibrate(
                    _calibrateValues[0][0],
                    _calibrateValues[1][0],
                    _calibrateValues[2][0],
                    _calibrateValues[0][1],
                    _calibrateValues[1][1],
                    _calibrateValues[2][1]);
        }


        ret = true;

    } while (0);



    if (!ret) {
        // calibration must restart if an error occurred
        _calibPoint = TSC2046_NUM_CALIB_POINTS+1;
    }



    return ret;

}


bool TSC2046::calibrate(touchCoordinate_t* values, int numValues) {
    if (values == NULL || numValues < TSC2046_NUM_CALIB_POINTS) return false;

    touchCoordinate_t ref[TSC2046_NUM_CALIB_POINTS];
    touchCoordinate_t scr[TSC2046_NUM_CALIB_POINTS];

    for (int i = 0; i < TSC2046_NUM_CALIB_POINTS; i++) {
        getCalibratePoint(i, &(ref[i].x), &(ref[i].y));
        scr[i] = values[i];
    }

    calibrate(ref[0], ref[1], ref[2], scr[0], scr[1], scr[2]);

    return true;
}


bool TSC2046::getCalibrationValues(touchCoordinate_t* values, int numValues) {
    if (values == NULL || numValues < TSC2046_NUM_CALIB_POINTS) return false;
    if (!_calibrated) return false;

    for (int i = 0; i < TSC2046_NUM_CALIB_POINTS; i++) {
        values[i] = _calibrateValues[i][1];
    }

    return true;
}


void TSC2046::readAndFilter(touchCoordinate_t &coord)
{
    int32_t ix, iy, iz1, iz2 = 0;
    int32_t lastx, lasty, lastz1, lastz2 = 0;
    int i = 0;

    coord.x = 0;
    coord.y = 0;
    coord.z = 0;

    lasty = getFilteredValue(READ_Y(0));
    lasty >>= 3;
    if (lasty >= 4095) {
        lasty = 0;
    }

    lastx = getFilteredValue(READ_X(0));
    lastx >>= 3;
    if (lastx >= 4095) {
        lastx = 0;
    }

    lastz1 = getFilteredValue(READ_Z1(0));
    lastz1 >>= 3;

    lastz2 = getFilteredValue(READ_Z2(0));
    lastz2 >>= 3;


    if (lastx && lastz1) {
        coord.z = (lastx * ABS(lastz2 - lastz1)) / lastz1;
    }
    else {
        coord.z = 0;
    }

    if (coord.z > 20000) {
        coord.z = 0;
    }

    if (coord.z == 0) {
        return;
    }

    for (i = 0; i < DEBOUNCE_MAX; i++) {
        iy = getFilteredValue(READ_Y(0));
        iy >>= 3;

        if (ABS (lasty - iy) <= DEBOUNCE_TOL) {
            break;
        }

        lasty = iy;
    }

    for (i = 0; i < DEBOUNCE_MAX; i++) {
        ix = getFilteredValue(READ_X(0));
        ix >>= 3;
        if (ix > 4095) {
            ix = 0;
        }

        if (ABS (lastx - ix) <= DEBOUNCE_TOL) {
            break;
        }

        lastx = ix;
    }

    for (i = 0; i < DEBOUNCE_MAX; i++) {
        iz1 = getFilteredValue(READ_Z1(0));
        iz1 >>= 3;

        if (ABS (lastz1 - iz1) <= DEBOUNCE_TOL) {
            break;
        }

        lastz1 = iz1;
    }

    for (i = 0; i < DEBOUNCE_MAX; i++) {
        iz2 = getFilteredValue(READ_Z2(0));
        iz2 >>= 3;

        if (ABS (lastz2 - iz2) <= DEBOUNCE_TOL) {
            break;
        }

        lastz2 = iz2;
    }

    coord.x = ix;
    coord.y = iy;

    if (ix && iz1) {
        coord.z = (ix * ABS(iz2 - iz1)) / iz1;
    }
    else {
        coord.z = 0;
    }

    if (coord.z > 20000) {
        coord.z = 0;
    }

}

int32_t TSC2046::getFilteredValue(int cmd)
{
    int32_t a[7];
    int32_t tmp = 0;
    int i = 0, j = 0;

    /*
     * Median and averaging filter
     *
     * 1. Get 7 values
     * 2. Sort these values
     * 3. Take average of the 3 values in the middle
     */

    for (i = 0; i < 7; i++) {
        a[i] = spiTransfer(cmd);
    }

    // bubble sort
    for (i = 0; i < 7; i++) {
        for (j = 0; j < (7-(i+1)); j++) {
            if (a[j] > a[j+1]) {
                // swap
                tmp = a[j];
                a[j] = a[j+1];
                a[j+1] = tmp;
            }
        }
    }

    // average of 3 values in the middle
    return ((a[2]+a[3]+a[4])/3);
}

uint16_t TSC2046::spiTransfer(uint8_t cmd)
{
    uint8_t data[3];

    _cs = 0;

    /*data[0] = */_spi.write(cmd);
    data[0] = _spi.write(0xff);
    data[1] = _spi.write(0xff);

    _cs = 1;

    return ((data[0] << 8) | data[1]);
}

void TSC2046::calibrate(touchCoordinate_t &ref1,
        touchCoordinate_t &ref2,
        touchCoordinate_t &ref3,
        touchCoordinate_t &scr1,
        touchCoordinate_t &scr2,
        touchCoordinate_t &scr3) {

    calibPoint_t disp[3];
    calibPoint_t scr[3];

    disp[0].x = ref1.x;
    disp[0].y = ref1.y;
    disp[1].x = ref2.x;
    disp[1].y = ref2.y;
    disp[2].x = ref3.x;
    disp[2].y = ref3.y;

    scr[0].x = scr1.x;
    scr[0].y = scr1.y;
    scr[1].x = scr2.x;
    scr[1].y = scr2.y;
    scr[2].x = scr3.x;
    scr[2].y = scr3.y;

    setCalibrationMatrix(disp, scr, &_calibMatrix);

    _calibrated = true;

}

void TSC2046::getCalibratePoint(int pointNum, int32_t* x, int32_t *y) {
    switch(pointNum) {
    case 0:
        *x = _insetPx;
        *y = _height - _insetPx;
        break;
    case 1:
        *x = _width/2;
        *y = _insetPx;
        break;
    case 2:
        *x = _width - _insetPx;
        *y = _height - _insetPx;
        break;
    }
}

int TSC2046::waitForTouch(int32_t* x, int32_t* y, uint32_t timeout) {
    Timer t;
    touchCoordinate_t coord;
    bool waitForRelease = false;
    int32_t tx = 0;
    int32_t ty = 0;


    t.start();
    while (timeout == 0 || ((uint32_t)t.read_ms() < timeout)) {

        read(coord);

        if (coord.z == 0 && waitForRelease) {
            *x = tx;
            *y = ty;
            break;
        }

        if (coord.z > 0) {
            tx = coord.x;
            ty = coord.y;
            waitForRelease = true;
        }

        wait_ms(10);
    }

    if (timeout > 0 && (uint32_t)t.read_ms() > timeout) {
        return -1;
    }

    return 0;

}



// ############################################################################
// >>>>>>>> Calibrate code >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// ############################################################################


/*
 *
 *   Copyright (c) 2001, Carlos E. Vidales. All rights reserved.
 *
 *   This sample program was written and put in the public domain
 *    by Carlos E. Vidales.  The program is provided "as is"
 *    without warranty of any kind, either expressed or implied.
 *   If you choose to use the program within your own products
 *    you do so at your own risk, and assume the responsibility
 *    for servicing, repairing or correcting the program should
 *    it prove defective in any manner.
 *   You may copy and distribute the program's source code in any
 *    medium, provided that you also include in each copy an
 *    appropriate copyright notice and disclaimer of warranty.
 *   You may also modify this program and distribute copies of
 *    it provided that you include prominent notices stating
 *    that you changed the file(s) and the date of any change,
 *    and that you do not charge any royalties or licenses for
 *    its use.
 *
 *
 *
 *   File Name:  calibrate.c
 *
 *
 *   This file contains functions that implement calculations
 *    necessary to obtain calibration factors for a touch screen
 *    that suffers from multiple distortion effects: namely,
 *    translation, scaling and rotation.
 *
 *   The following set of equations represent a valid display
 *    point given a corresponding set of touch screen points:
 *
 *
 *                                              /-     -\
 *              /-    -\     /-            -\   |       |
 *              |      |     |              |   |   Xs  |
 *              |  Xd  |     | A    B    C  |   |       |
 *              |      |  =  |              | * |   Ys  |
 *              |  Yd  |     | D    E    F  |   |       |
 *              |      |     |              |   |   1   |
 *              \-    -/     \-            -/   |       |
 *                                              \-     -/
 *
 *
 *    where:
 *
 *           (Xd,Yd) represents the desired display point
 *                    coordinates,
 *
 *           (Xs,Ys) represents the available touch screen
 *                    coordinates, and the matrix
 *
 *           /-   -\
 *           |A,B,C|
 *           |D,E,F| represents the factors used to translate
 *           \-   -/  the available touch screen point values
 *                    into the corresponding display
 *                    coordinates.
 *
 *
 *    Note that for practical considerations, the utilitities
 *     within this file do not use the matrix coefficients as
 *     defined above, but instead use the following
 *     equivalents, since floating point math is not used:
 *
 *            A = An/Divider
 *            B = Bn/Divider
 *            C = Cn/Divider
 *            D = Dn/Divider
 *            E = En/Divider
 *            F = Fn/Divider
 *
 *
 *
 *    The functions provided within this file are:
 *
 *          setCalibrationMatrix() - calculates the set of factors
 *                                    in the above equation, given
 *                                    three sets of test points.
 *               getDisplayPoint() - returns the actual display
 *                                    coordinates, given a set of
 *                                    touch screen coordinates.
 * translateRawScreenCoordinates() - helper function to transform
 *                                    raw screen points into values
 *                                    scaled to the desired display
 *                                    resolution.
 *
 *
 */


/**********************************************************************
 *
 *     Function: setCalibrationMatrix()
 *
 *  Description: Calling this function with valid input data
 *                in the display and screen input arguments
 *                causes the calibration factors between the
 *                screen and display points to be calculated,
 *                and the output argument - matrixPtr - to be
 *                populated.
 *
 *               This function needs to be called only when new
 *                calibration factors are desired.
 *
 *
 *  Argument(s): displayPtr (input) - Pointer to an array of three
 *                                     sample, reference points.
 *               screenPtr (input) - Pointer to the array of touch
 *                                    screen points corresponding
 *                                    to the reference display points.
 *               matrixPtr (output) - Pointer to the calibration
 *                                     matrix computed for the set
 *                                     of points being provided.
 *
 *
 *  From the article text, recall that the matrix coefficients are
 *   resolved to be the following:
 *
 *
 *      Divider =  (Xs0 - Xs2)*(Ys1 - Ys2) - (Xs1 - Xs2)*(Ys0 - Ys2)
 *
 *
 *
 *                 (Xd0 - Xd2)*(Ys1 - Ys2) - (Xd1 - Xd2)*(Ys0 - Ys2)
 *            A = ---------------------------------------------------
 *                                   Divider
 *
 *
 *                 (Xs0 - Xs2)*(Xd1 - Xd2) - (Xd0 - Xd2)*(Xs1 - Xs2)
 *            B = ---------------------------------------------------
 *                                   Divider
 *
 *
 *                 Ys0*(Xs2*Xd1 - Xs1*Xd2) +
 *                             Ys1*(Xs0*Xd2 - Xs2*Xd0) +
 *                                           Ys2*(Xs1*Xd0 - Xs0*Xd1)
 *            C = ---------------------------------------------------
 *                                   Divider
 *
 *
 *                 (Yd0 - Yd2)*(Ys1 - Ys2) - (Yd1 - Yd2)*(Ys0 - Ys2)
 *            D = ---------------------------------------------------
 *                                   Divider
 *
 *
 *                 (Xs0 - Xs2)*(Yd1 - Yd2) - (Yd0 - Yd2)*(Xs1 - Xs2)
 *            E = ---------------------------------------------------
 *                                   Divider
 *
 *
 *                 Ys0*(Xs2*Yd1 - Xs1*Yd2) +
 *                             Ys1*(Xs0*Yd2 - Xs2*Yd0) +
 *                                           Ys2*(Xs1*Yd0 - Xs0*Yd1)
 *            F = ---------------------------------------------------
 *                                   Divider
 *
 *
 *       Return: OK - the calibration matrix was correctly
 *                     calculated and its value is in the
 *                     output argument.
 *               NOT_OK - an error was detected and the
 *                         function failed to return a valid
 *                         set of matrix values.
 *                        The only time this sample code returns
 *                        NOT_OK is when Divider == 0
 *
 *
 *
 *                 NOTE!    NOTE!    NOTE!
 *
 *  setCalibrationMatrix() and getDisplayPoint() will do fine
 *  for you as they are, provided that your digitizer
 *  resolution does not exceed 10 bits (1024 values).  Higher
 *  resolutions may cause the integer operations to overflow
 *  and return incorrect values.  If you wish to use these
 *  functions with digitizer resolutions of 12 bits (4096
 *  values) you will either have to a) use 64-bit signed
 *  integer variables and math, or b) judiciously modify the
 *  operations to scale results by a factor of 2 or even 4.
 *
 *
 */
int TSC2046::setCalibrationMatrix( calibPoint_t * displayPtr,
        calibPoint_t * screenPtr,
        calibMatrix_t * matrixPtr)
{
    int  retValue = 0 ;


    matrixPtr->Divider = ((screenPtr[0].x - screenPtr[2].x) * (screenPtr[1].y - screenPtr[2].y)) -
                         ((screenPtr[1].x - screenPtr[2].x) * (screenPtr[0].y - screenPtr[2].y)) ;
    if( matrixPtr->Divider == 0 )
    {
        retValue = 1 ;
    }
    else
    {
        matrixPtr->An = ((displayPtr[0].x - displayPtr[2].x) * (screenPtr[1].y - screenPtr[2].y)) -
                        ((displayPtr[1].x - displayPtr[2].x) * (screenPtr[0].y - screenPtr[2].y)) ;
        matrixPtr->Bn = ((screenPtr[0].x - screenPtr[2].x) * (displayPtr[1].x - displayPtr[2].x)) -
                        ((displayPtr[0].x - displayPtr[2].x) * (screenPtr[1].x - screenPtr[2].x)) ;
        matrixPtr->Cn = (screenPtr[2].x * displayPtr[1].x - screenPtr[1].x * displayPtr[2].x) * screenPtr[0].y +
                        (screenPtr[0].x * displayPtr[2].x - screenPtr[2].x * displayPtr[0].x) * screenPtr[1].y +
                        (screenPtr[1].x * displayPtr[0].x - screenPtr[0].x * displayPtr[1].x) * screenPtr[2].y ;
        matrixPtr->Dn = ((displayPtr[0].y - displayPtr[2].y) * (screenPtr[1].y - screenPtr[2].y)) -
                        ((displayPtr[1].y - displayPtr[2].y) * (screenPtr[0].y - screenPtr[2].y)) ;

        matrixPtr->En = ((screenPtr[0].x - screenPtr[2].x) * (displayPtr[1].y - displayPtr[2].y)) -
                        ((displayPtr[0].y - displayPtr[2].y) * (screenPtr[1].x - screenPtr[2].x)) ;
        matrixPtr->Fn = (screenPtr[2].x * displayPtr[1].y - screenPtr[1].x * displayPtr[2].y) * screenPtr[0].y +
                        (screenPtr[0].x * displayPtr[2].y - screenPtr[2].x * displayPtr[0].y) * screenPtr[1].y +
                        (screenPtr[1].x * displayPtr[0].y - screenPtr[0].x * displayPtr[1].y) * screenPtr[2].y ;
    }

    return( retValue ) ;
}

/**********************************************************************
 *
 *     Function: getDisplayPoint()
 *
 *  Description: Given a valid set of calibration factors and a point
 *                value reported by the touch screen, this function
 *                calculates and returns the true (or closest to true)
 *                display point below the spot where the touch screen
 *                was touched.
 *
 *
 *
 *  Argument(s): displayPtr (output) - Pointer to the calculated
 *                                      (true) display point.
 *               screenPtr (input) - Pointer to the reported touch
 *                                    screen point.
 *               matrixPtr (input) - Pointer to calibration factors
 *                                    matrix previously calculated
 *                                    from a call to
 *                                    setCalibrationMatrix()
 *
 *
 *  The function simply solves for Xd and Yd by implementing the
 *   computations required by the translation matrix.
 *
 *                                              /-     -\
 *              /-    -\     /-            -\   |       |
 *              |      |     |              |   |   Xs  |
 *              |  Xd  |     | A    B    C  |   |       |
 *              |      |  =  |              | * |   Ys  |
 *              |  Yd  |     | D    E    F  |   |       |
 *              |      |     |              |   |   1   |
 *              \-    -/     \-            -/   |       |
 *                                              \-     -/
 *
 *  It must be kept brief to avoid consuming CPU cycles.
 *
 *
 *       Return: OK - the display point was correctly calculated
 *                     and its value is in the output argument.
 *               NOT_OK - an error was detected and the function
 *                         failed to return a valid point.
 *
 *
 *
 *                 NOTE!    NOTE!    NOTE!
 *
 *  setCalibrationMatrix() and getDisplayPoint() will do fine
 *  for you as they are, provided that your digitizer
 *  resolution does not exceed 10 bits (1024 values).  Higher
 *  resolutions may cause the integer operations to overflow
 *  and return incorrect values.  If you wish to use these
 *  functions with digitizer resolutions of 12 bits (4096
 *  values) you will either have to a) use 64-bit signed
 *  integer variables and math, or b) judiciously modify the
 *  operations to scale results by a factor of 2 or even 4.
 *
 *
 */
int TSC2046::getDisplayPoint( calibPoint_t * displayPtr,
        calibPoint_t * screenPtr,
        calibMatrix_t * matrixPtr )
{
    int  retValue = 0 ;

    if( matrixPtr->Divider != 0 )
    {
        /* Operation order is important since we are doing integer */
        /*  math. Make sure you add all terms together before      */
        /*  dividing, so that the remainder is not rounded off     */
        /*  prematurely.                                           */
        displayPtr->x = ( (matrixPtr->An * screenPtr->x) +
                (matrixPtr->Bn * screenPtr->y) +
                matrixPtr->Cn
        ) / matrixPtr->Divider ;
        displayPtr->y = ( (matrixPtr->Dn * screenPtr->x) +
                (matrixPtr->En * screenPtr->y) +
                matrixPtr->Fn
        ) / matrixPtr->Divider ;
    }
    else
    {
        retValue = 1 ;
    }
    return( retValue ) ;
}



// ############################################################################
// <<<<<<<< Calibrate code <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// ############################################################################



