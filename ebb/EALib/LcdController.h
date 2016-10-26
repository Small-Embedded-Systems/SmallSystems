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

#ifndef LCDCONTROLLER_H
#define LCDCONTROLLER_H

/**
 * A LCD controller interface
 *
 * @code
 * #include "mbed.h"
 * #include "LcdController.h"
 *
 * LcdController::Config innolux(
 *        45,
 *        17,
 *        2,
 *        800,
 *        22,
 *        22,
 *        2,
 *        480,
 *        false,
 *        false,
 *        true,
 *        true,
 *        true,
 *        LcdController::Bpp_16_565,
 *        36000000,
 *        LcdController::Tft,
 *        false);
 *
 * int main(void) {
 *    LcdController lcd;
 *
 *    lcd.open(&innolux);
 *    lcd.setFrameBuffer(frameBuffer);
 *    lcd.setPower(true);
 *
 *    // draw on the frame buffer
 *    ...
 * }
 * @endcode
 */
class LcdController {
public:

    enum LcdPanel {
        Tft = 0,    // standard TFT
        AdTft,      // advanced TFT
        HrTft,      // highly reflective TFT
        Mono_4Bit,  // 4-bit mono
        Mono_8Bit,  // 8-bit mono
        ColorStn    // Color STN
    };

    enum BitsPerPixel {
        Bpp_1 = 0,
        Bpp_2,
        Bpp_4,
        Bpp_8,
        Bpp_16,
        Bpp_24,
        Bpp_16_565,
        Bpp_12_444
    };

    /**
     * LCD configuration
     */
    class Config {
    public:

        /** Create an empty LCD configuration object
         *
         */
        Config() {
        }

        /** Create a LCD configuration object with specified values
         *
         *  @param horizontalBackPorch Horizontal back porch in clocks
         *  @param horizontalFrontPorch Horizontal front porch in clocks
         *  @param hsync HSYNC pulse width in clocks
         *  @param width width of display in pixels
         *  @param verticalBackPorch vertical back porch in clocks
         *  @param verticalFrontPorch vertical front porch in clocks
         *  @param vsync VSYNC pulse width in clocks
         *  @param height height of display in pixels
         *  @param invertOutputEnable true to invert output enable
         *  @param invertPanelClock true to invert panel clock
         *  @param invertHsync true to invert HSYNC
         *  @param invertVsync true to invert VSYNC
         *  @param acBias AC bias frequency in clocks
         *  @param bpp bits per pixel
         *  @param optimalClock optimal clock rate (Hz)
         *  @param panelType LCD panel type
         *  @param dualPanel true if it is a dual panel display
         */
        Config(int horizontalBackPorch,
                int horizontalFrontPorch,
                int hsync,
                int width,
                int verticalBackPorch,
                int verticalFrontPorch,
                int vsync,
                int height,
                bool invertOutputEnable,
                bool invertPanelClock,
                bool invertHsync,
                bool invertVsync,
                int acBias,
                BitsPerPixel bpp,
                int optimalClock,
                LcdPanel panelType,
                bool dualPanel) {

            this->horizontalBackPorch = horizontalBackPorch;
            this->horizontalFrontPorch = horizontalFrontPorch;
            this->hsync = hsync;
            this->width = width;
            this->verticalBackPorch = verticalBackPorch;
            this->verticalFrontPorch = verticalFrontPorch;
            this->vsync = vsync;
            this->height = height;
            this->invertOutputEnable = invertOutputEnable;
            this->invertPanelClock = invertPanelClock;
            this->invertHsync = invertHsync;
            this->invertVsync = invertVsync;
            this->acBias = acBias;
            this->bpp = bpp;
            this->optimalClock = optimalClock;
            this->panelType = panelType;
            this->dualPanel = dualPanel;
        }

        int horizontalBackPorch;
        int horizontalFrontPorch;
        int hsync;
        int width;
        int verticalBackPorch;
        int verticalFrontPorch;
        int vsync;
        int height;
        bool invertOutputEnable;
        bool invertPanelClock;
        bool invertHsync;
        bool invertVsync;
        int acBias;
        BitsPerPixel bpp;
        int optimalClock;
        LcdPanel panelType;
        bool dualPanel;
    };

    /** Create a LCD controller interface
     *
     */
    LcdController();

    /** Open and initialize the LCD controller with the given LCD configuration
     *
     *  @param cfg LCD configuration
     *
     *  @returns
     *       0 on success
     *       1 on failure
     */
    int open(LcdController::Config* cfg);

    /** Close the LCD controller
     *
     *  @returns
     *       0 on success
     *       1 on failure
     */
    int close();

    /** Set and activate the address of the frame buffer to use.
     *
     *  It is the content of the frame buffer that is shown on the
     *  display. All the drawing on the frame buffer can be done
     *  'offline' and whenever it should be shown this function
     *  can be called with the address of the offline frame buffer.
     *
     *  @param address Memory address of the frame buffer
     *
     *  @returns
     *       0 on success
     *       1 on failure
     */
    int setFrameBuffer(uint32_t address);

    /** Turn on/off the power to the display.
     *
     *  @param on true to turn on, false to turn off
     *
     *  @returns
     *       0 on success
     *       1 on failure
     */
    int setPower(bool on);


private:

    bool _opened;
    static bool _lcdControllerUsed;

    void init(LcdController::Config* cfg);
    void pinConfig();
    uint32_t getClockDivisor(int clock);
};

#endif


