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

#ifndef GFXFB_H
#define GFXFB_H

#include "Adafruit_GFX.h"

#define BLACK         0x0000
/* Light gray color, 565 mode */
#define LIGHTGRAY     0X7BEF
/* Dark gray color, 565 mode */
#define DARKGRAY      0x39E7
/* White color, 565 mode */
#define WHITE         0xffff
/* Red color, 565 mode */
#define RED           0xF800
/* Green color, 565 mode */
#define GREEN         0x07E0
/* Blue color, 565 mode */
#define BLUE          0x001F
/* Magenta color, 565 mode */
#define MAGENTA       (RED | BLUE)
/* Cyan color, 565 mode */
#define CYAN          (GREEN | BLUE)
/* Yellow color, 565 mode */
#define YELLOW        (RED | GREEN)
/* Light red color, 565 mode */
#define LIGHTRED      0x7800
/* Light green color, 565 mode */
#define LIGHTGREEN    0x03E0
/* Light blue color, 565 mode */
#define LIGHTBLUE     0x000F
/* Light magenta color, 565 mode */
#define LIGHTMAGENTA  (LIGHTRED | LIGHTBLUE)
/* Light cyan color, 565 mode */
#define LIGHTCYAN     (LIGHTGREEN | LIGHTBLUE)
/* Light yellow color, 565 mode */
#define LIGHTYELLOW   (LIGHTRED | LIGHTGREEN)

/**
 * Graphical library based on Adafruit's GFX using a Frame buffer.
 */
class GFXFb : public Adafruit_GFX {
public:

    /** Create an interface to the GFX graphical library
     *
     * @param w width of the display
     * @param h height of the display
     * @param fb frame buffer that will be used by the graphical library. Can
     * be set by calling setFb instead.
     */
    GFXFb(uint16_t w, uint16_t h, uint16_t* fb = 0);

    virtual void drawPixel(int16_t x, int16_t y, uint16_t color);
    virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
    virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
    virtual void fillScreen(uint16_t color);

    /** Associate a frame buffer with the graphical library.
     *  All drawing requests will be performed on the frame buffer.
     *
     * @param fb frame buffer that will be used by the graphical library
     */
    void setFb(uint16_t* fb) {_fb = fb;}

    /** Get the frame buffer associated with the graphical library.
     *
     * @returns the frame buffer associated with the graphical library.
     */
    uint16_t* getFb() {return _fb;}

    /** Write a null-terminated string
     *
     * @param s the string to write on the display
     */
    void writeString(const char* s);

    /** Get the width in pixels of the given string.
     *
     * @param s width will be calculated on this string
     * @returns the width in pixels of the string
     */
    int16_t getStringWidth(const char* s);

    /** Get the height in pixels of the given string.
     *
     * @param s height will be calculated on this string
     * @returns the height in pixels of the string
     */
    int16_t getStringHeight(const char* s);


private:

    uint16_t* _fb;
};

#endif


