#include "display.h"

#define LCD_CONFIGURATION_43 \
        40,                         /* horizontalBackPorch */ \
        5,                          /* horizontalFrontPorch */ \
        2,                          /* hsync */ \
        480,                        /* width */ \
        8,                          /* verticalBackPorch */ \
        8,                          /* verticalFrontPorch */ \
        2,                          /* vsync */ \
        272,                        /* height */ \
        false,                      /* invertOutputEnable */ \
        false,                      /* invertPanelClock */ \
        true,                       /* invertHsync */ \
        true,                       /* invertVsync */ \
        1,                          /* acBias */ \
        LcdController::Bpp_16_565,  /* bpp */ \
        9000000,                    /* optimalClock */ \
        LcdController::Tft,         /* panelType */ \
        false                       /* dualPanel */

#define LCD_INIT_STRING_43  (char*)"v1,cd0,c50,cc0,c30,d100,c31,d100,cd1,d10,o,c51,cc100"


Display::Display() : 
  GFXFb(480, 272),
  initStr(NULL),
  lcdCfg(NULL),
  lcdBoard(P0_27, P0_28),
  frameBuffer(0) {

		initStr = LCD_INIT_STRING_43;
		lcdCfg = new LcdController::Config(LCD_CONFIGURATION_43);
    result = lcdBoard.open(lcdCfg, initStr);	
    if (result == EaLcdBoard::Ok) {
      if (sdram_init() == 0) {
        frameBuffer = (uint32_t) malloc(lcdCfg->width * lcdCfg->height * 2 * 3); // 2 is for 16 bit color, 3 is the number of buffers
        memset((uint8_t*)frameBuffer, 0, lcdCfg->width * lcdCfg->height * 2 * 3);
        result = lcdBoard.setFrameBuffer(frameBuffer);
  		  GFXFb::setFb((uint16_t *)frameBuffer);
	    }
			else {
				result = EaLcdBoard::BufferTooSmall;
			}
		}
}

Display::~Display() {
  if (frameBuffer != 0) {
    free((void*)frameBuffer);
    frameBuffer = 0;
	}
}

Display* Display::theDisplay() {
	static Display* theDisplay = new Display();
	return theDisplay;
}

bool Display::initOk() {
	return (result == EaLcdBoard::Ok);
}

int Display::_putc(int value) {
	GFXFb::write(value);
	return value;
}

int Display::_getc() {
	return -1;
}
