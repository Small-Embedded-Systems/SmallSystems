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

#ifndef EALCDBOARDGPIO_H
#define EALCDBOARDGPIO_H

#include "EaLcdBoard.h"

/** An interface to Embedded Artists LCD Boards
 *
 */
class EaLcdBoardGPIO : public EaLcdBoard {
public:

    EaLcdBoardGPIO(PinName sda, PinName scl);

    void setBC(uint32_t val) { setBacklightContrast(val); };

protected:
    virtual void setWriteProtect(bool enable);
    virtual void set3V3Signal(bool enabled);
    virtual void set5VSignal(bool enabled);
    virtual void setDisplayEnableSignal(bool enabled);
    virtual void setBacklightContrast(uint32_t value);

private:
    //DigitalOut pinWP;
    DigitalOut pin3v3;
    DigitalOut pin5v;
    DigitalOut pinDE;
    //DigitalOut pinContrast;
    PwmOut pinContrast;
};

#endif


