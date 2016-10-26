#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <mbed.h>
#include <Graphics.h>
#include <LcdController.h>
#include <EaLcdBoardGPIO.h>
#include <AR1021I2C.h>
#include <sdram.h>
#include <wchar.h>
#include <GFXFb.h>

class Display : public Stream, public GFXFb {
	public:
	  virtual ~Display();
	  bool initOk();
		static Display* theDisplay();
	private:
	  Display();
	  char *initStr;
	  LcdController::Config *lcdCfg;
		EaLcdBoardGPIO lcdBoard;
    uint32_t frameBuffer;
	  EaLcdBoard::Result result;
	protected:
		virtual int _putc(int value);
	  virtual int _getc();
};
#endif
