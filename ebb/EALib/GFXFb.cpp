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
#include "GFXFb.h"


GFXFb::GFXFb(uint16_t w, uint16_t h, uint16_t* fb) : Adafruit_GFX(w, h) {
    _fb = fb;
}


void GFXFb::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (_fb == 0) return;

    if (x < 0 || x >= width() || y < 0 || y >= height()) return;

    *(_fb + x + y*_width ) = color;
}

void GFXFb::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    int16_t y2 = y + h - 1;

    if (y < 0) y = 0;
    if (y2 >= _height) y2 = _height-1;

    if (_fb == 0) return;
    if (x < 0 || x >= _width || y >= _height || y2 < y) return;

    uint16_t* f = (_fb + x + y*_width);
    while(y <= y2) {

        *f = color;
        f += _width;
        y++;
    }

}

void GFXFb::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    int16_t x2 = x + w - 1;

    if (x < 0) x = 0;
    if (x2 >= _width) x2 = _width-1;

    if (_fb == 0) return;
    if (x >= _width || x2 < x || y < 0 || y >= _height) return;

    uint16_t* f = (_fb + x + y*_width);
    while(x <= x2) {

        *f++ = color;
        x++;
    }

}


void GFXFb::fillScreen(uint16_t color) {

    if (_fb == 0) return;

    int len = _width*_height;
    for (int i = 0; i < len; i++) {
        *(_fb+i) = color;
    }
}

void GFXFb::writeString(const char* s) {
    if (s == NULL) return;

    while(*s != 0) {
        write(*s);
        s++;
    }
}

int16_t GFXFb::getStringWidth(const char* s) {
    // the default font in GFX is 6 pixels in width
    int chWidth = 6*textsize;
    int sz = 0;

    while(*s != 0) {
        sz += chWidth;
        s++;
    }

    return sz;
}

int16_t GFXFb::getStringHeight(const char* s) {
    (void)s;
    // the default font in GFX is 8 pixels in height
    return 8;
}


