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

#ifndef TOUCHPANEL_H
#define TOUCHPANEL_H


/**
 * An abstract class that represents touch panels.
 */
class TouchPanel {
public:

    typedef struct {
        int32_t x;
        int32_t y;
        int32_t z;
    } touchCoordinate_t;


    /**
     * Initialize the touch controller. This method must be called before
     * calibrating or reading data from the controller
     *
     * @param width the width of the touch panel. This is usually the same as
     * the width of the display
     * @param height the height of the touch panel. This is usually the same
     * as the height of the display.
     *
     * @return true if the request was successful; otherwise false
     */
    virtual bool init(uint16_t width, uint16_t height) = 0;

    /**
     * Read coordinates from the touch panel.
     *
     * @param coord pointer to coordinate object. The read coordinates will be
     * written to this object.
     */
    virtual bool read(touchCoordinate_t &coord) = 0;


    /**
     * Start to calibrate the display
     *
     * @return true if the request was successful; otherwise false
     */
    virtual bool calibrateStart() = 0;

    /**
     * Get the next calibration point. Draw an indicator on the screen
     * at the coordinates and ask the user to press/click on the indicator.
     * Please note that waitForCalibratePoint() must be called after this
     * method.
     *
     * @param x the x coordinate is written to this argument
     * @param y the y coordinate is written to this argument
     *
     * @return true if the request was successful; otherwise false
     */
    virtual bool getNextCalibratePoint(uint16_t* x, uint16_t* y) = 0;

    /**
     * Wait for a calibration point to have been pressed and recored.
     * This method must be called just after getNextCalibratePoint().
     *
     * @param morePoints true is written to this argument if there
     * are more calibrations points available; otherwise it will be false
     * @param timeout maximum number of milliseconds to wait for
     * a calibration point. Set this argument to 0 to wait indefinite.
     *
     * @return true if the request was successful; otherwise false
     */
    virtual bool waitForCalibratePoint(bool* morePoints, uint32_t timeout) = 0;


};

#endif
