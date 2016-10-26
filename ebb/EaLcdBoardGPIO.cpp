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
#include "EaLcdBoardGPIO.h"

/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/


EaLcdBoardGPIO::EaLcdBoardGPIO(PinName sda, PinName scl)
  : EaLcdBoard(sda, scl), /*pinWP(P4_15),*/ pin3v3(P2_0), pin5v(P2_21), pinDE(P2_11), pinContrast(P2_1)
{
  pinContrast.period_ms(10);
  setWriteProtect(true);
  set3V3Signal(false);
  set5VSignal(false);
  setDisplayEnableSignal(false);
  setBacklightContrast(0);
}


void EaLcdBoardGPIO::setWriteProtect(bool enable)
{  
    // Not Applicable
}

void EaLcdBoardGPIO::set3V3Signal(bool enabled) { //P2.0 L=3.3V on
    if (enabled) {
        pin3v3 = 0;
    } else {
        pin3v3 = 1;
    }
}

void EaLcdBoardGPIO::set5VSignal(bool enabled) { //P2.21 H=5V on
    if (enabled) {
        pin5v = 1;
    } else {
        pin5v = 0;
    }
}

void EaLcdBoardGPIO::setDisplayEnableSignal(bool enabled) { //P2.11 H=enabled
    LPC_IOCON->P2_11 &= ~7; /* GPIO2[11]  @ P2.11 */
    if (enabled) {
        pinDE = 1;
    } else {
        pinDE = 0;
    }
}

void EaLcdBoardGPIO::setBacklightContrast(uint32_t value) { //P2.1, set to 4.30 for now
#if 0    
    LPC_IOCON->P2_1 &= ~7; /* GPIO2[1]  @ P2.1 */
  if (value > 50) {
      pinContrast = 1;
  } else {
      pinContrast = 0;
  }
#else
    uint32_t tmp = LPC_IOCON->P2_1;
    tmp &= ~7;
    tmp |= 1;
    LPC_IOCON->P2_1 = tmp; /* PWM2[1]  @ P2.1 */
    float f = value;
    pinContrast = f/100.0f;
#endif
  
//    if (value > 100) return;

//    pca9532_setBlink0Duty(100-value);
//    pca9532_setBlink0Period(0);
//    pca9532_setBlink0Leds(LCDB_CTRL_BL_C);
}



