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
#include "mbed_debug.h"

#include "AR1021I2C.h"

/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/

#define AR1021_REG_TOUCH_THRESHOLD        (0x02)
#define AR1021_REG_SENS_FILTER            (0x03)
#define AR1021_REG_SAMPLING_FAST          (0x04)
#define AR1021_REG_SAMPLING_SLOW          (0x05)
#define AR1021_REG_ACC_FILTER_FAST        (0x06)
#define AR1021_REG_ACC_FILTER_SLOW        (0x07)
#define AR1021_REG_SPEED_THRESHOLD        (0x08)
#define AR1021_REG_SLEEP_DELAY            (0x0A)
#define AR1021_REG_PEN_UP_DELAY           (0x0B)
#define AR1021_REG_TOUCH_MODE             (0x0C)
#define AR1021_REG_TOUCH_OPTIONS          (0x0D)
#define AR1021_REG_CALIB_INSETS           (0x0E)
#define AR1021_REG_PEN_STATE_REPORT_DELAY (0x0F)
#define AR1021_REG_TOUCH_REPORT_DELAY     (0x11)


#define AR1021_CMD_GET_VERSION                 (0x10)
#define AR1021_CMD_ENABLE_TOUCH                (0x12)
#define AR1021_CMD_DISABLE_TOUCH               (0x13)
#define AR1021_CMD_CALIBRATE_MODE              (0x14)
#define AR1021_CMD_REGISTER_READ               (0x20)
#define AR1021_CMD_REGISTER_WRITE              (0x21)
#define AR1021_CMD_REGISTER_START_ADDR_REQUEST (0x22)
#define AR1021_CMD_REGISTER_WRITE_TO_EEPROM    (0x23)
#define AR1021_CMD_EEPROM_READ                 (0x28)
#define AR1021_CMD_EEPROM_WRITE                (0x29)
#define AR1021_CMD_EEPROM_WRITE_TO_REGISTERS   (0x2B)

#define AR1021_RESP_STAT_OK           (0x00)
#define AR1021_RESP_STAT_CMD_UNREC    (0x01)
#define AR1021_RESP_STAT_HDR_UNREC    (0x03)
#define AR1021_RESP_STAT_TIMEOUT      (0x04)
#define AR1021_RESP_STAT_CANCEL_CALIB (0xFC)


#define AR1021_ERR_NO_HDR      (-1000)
#define AR1021_ERR_INV_LEN     (-1001)
#define AR1021_ERR_INV_RESP    (-1002)
#define AR1021_ERR_INV_RESPLEN (-1003)
#define AR1021_ERR_TIMEOUT     (-1004)

// bit 7 is always 1 and bit 0 defines pen up or down
#define AR1021_PEN_MASK (0x81)

#define AR1021_NUM_CALIB_POINTS (4)

#define AR1021_ADDR  (0x4D << 1)

#define AR1021_TIMEOUT     1000        //how many ms to wait for responce 
#define AR1021_RETRY          5        //how many times to retry sending command 

#define AR1021_MIN(__a, __b) (((__a)<(__b))?(__a):(__b))


AR1021I2C::AR1021I2C(PinName sda, PinName scl, PinName siq) :
_i2c(sda, scl), _siq(siq), _siqIrq(siq)
{
    _i2c.frequency(200000);

    // default calibration inset is 25 -> (25/2 = 12.5%)
    _inset = 25;

    // make sure _calibPoint has an invalid value to begin with
    // correct value is set in calibrateStart()
    _calibPoint = AR1021_NUM_CALIB_POINTS+1;

    _x = 0;
    _y = 0;
    _pen = 0;

    _initialized = false;
}


bool AR1021I2C::read(touchCoordinate_t &coord) {

    if (!_initialized) return false;

    coord.x = _x * _width/4095;
    coord.y = _y * _height/4095;
    coord.z = _pen;

    return true;
}


bool AR1021I2C::init(uint16_t width, uint16_t height) {
    int result = 0;
    bool ok = false;
    int attempts = 0;

    _width = width;
    _height = height;

    while (1) {

        do {
            // disable touch
            result = cmd(AR1021_CMD_DISABLE_TOUCH, NULL, 0, NULL, 0);
            if (result != 0) {
                debug("disable touch failed (%d)\n", result);
                break;
            }

            char regOffset = 0;
            int regOffLen = 1;
            result = cmd(AR1021_CMD_REGISTER_START_ADDR_REQUEST, NULL, 0,
                    &regOffset, &regOffLen);
            if (result != 0) {
                debug("register offset request failed (%d)\n", result);
                break;
            }

            // enable calibrated coordinates
            //                  high, low address,                        len,  value
            char toptions[4] = {0x00, AR1021_REG_TOUCH_OPTIONS+regOffset, 0x01, 0x01};
            result = cmd(AR1021_CMD_REGISTER_WRITE, toptions, 4, NULL, 0);
            if (result != 0) {
                debug("register write request failed (%d)\n", result);
                break;
            }

            // save registers to eeprom
            result = cmd(AR1021_CMD_REGISTER_WRITE_TO_EEPROM, NULL, 0, NULL, 0);
            if (result != 0) {
                debug("register write to eeprom failed (%d)\n", result);
                break;
            }

            // enable touch
            result = cmd(AR1021_CMD_ENABLE_TOUCH, NULL, 0, NULL, 0);
            if (result != 0) {
                debug("enable touch failed (%d)\n", result);
                break;
            }

            _siqIrq.rise(this, &AR1021I2C::readTouchIrq);

            _initialized = true;
            ok = true;

        } while(0);

        if (ok) break;

        // try to run the initialize sequence at most 2 times
        if(++attempts >= 2) break;
    }


    return ok;
}

bool AR1021I2C::calibrateStart() {
    bool ok = false;
    int result = 0;
    int attempts = 0;

    if (!_initialized) return false;

    _siqIrq.rise(NULL);

    while(1) {

        do {
            // disable touch
            result = cmd(AR1021_CMD_DISABLE_TOUCH, NULL, 0, NULL, 0);
            if (result != 0) {
                debug("disable touch failed (%d)\n", result);
                break;
            }

            char regOffset = 0;
            int regOffLen = 1;
            result = cmd(AR1021_CMD_REGISTER_START_ADDR_REQUEST, NULL, 0,
                    &regOffset, &regOffLen);
            if (result != 0) {
                debug("register offset request failed (%d)\n", result);
                break;
            }

            // set insets
            // enable calibrated coordinates
            //                high, low address,                       len,  value
            char insets[4] = {0x00, AR1021_REG_CALIB_INSETS+regOffset, 0x01, _inset};
            result = cmd(AR1021_CMD_REGISTER_WRITE, insets, 4, NULL, 0);
            if (result != 0) {
                debug("register write request failed (%d)\n", result);
                break;
            }

            // calibration mode
            char calibType = 4;
            result = cmd(AR1021_CMD_CALIBRATE_MODE, &calibType, 1, NULL, 0, false);
            if (result != 0) {
                debug("calibration mode failed (%d)\n", result);
                break;
            }

            _calibPoint = 0;
            ok = true;

        } while(0);

        if (ok) break;

        // try to run the calibrate mode sequence at most 2 times
        if (++attempts >= 2) break;
    }

    return ok;
}

bool AR1021I2C::getNextCalibratePoint(uint16_t* x, uint16_t* y) {

    if (!_initialized) return false;
    if (x == NULL || y == NULL) return false;

    int xInset = (_width * _inset / 100) / 2;
    int yInset = (_height * _inset / 100) / 2;

    switch(_calibPoint) {
    case 0:
        *x = xInset;
        *y = yInset;
        break;
    case 1:
        *x = _width - xInset;
        *y = yInset;
        break;
    case 2:
        *x = _width - xInset;
        *y = _height - yInset;
        break;
    case 3:
        *x = xInset;
        *y = _height - yInset;
        break;
    default:
        return false;
    }

    return true;
}

bool AR1021I2C::waitForCalibratePoint(bool* morePoints, uint32_t timeout) {
    int result = 0;
    bool ret = false;

    if (!_initialized) return false;

    do {
        if (morePoints == NULL || _calibPoint >= AR1021_NUM_CALIB_POINTS) {
            break;
        }

        // wait for response
        result = waitForCalibResponse(timeout);
        if (result != 0) {
            debug("wait for calibration response failed (%d)\n", result);
            break;
        }

        _calibPoint++;
        *morePoints = (_calibPoint < AR1021_NUM_CALIB_POINTS);


        // no more points -> enable touch
        if (!(*morePoints)) {

            // wait for calibration data to be written to eeprom
            // before enabling touch
            result = waitForCalibResponse(timeout);
            if (result != 0) {
                debug("wait for calibration response failed (%d)\n", result);
                break;
            }


            // clear chip-select since calibration is done;
//            _cs = 1;

            result = cmd(AR1021_CMD_ENABLE_TOUCH, NULL, 0, NULL, 0);
            if (result != 0) {
                debug("enable touch failed (%d)\n", result);
                break;
            }

            _siqIrq.rise(this, &AR1021I2C::readTouchIrq);
        }

        ret = true;

    } while (0);



    if (!ret) {
        // make sure to set chip-select off in case of an error
//        _cs = 1;
        // calibration must restart if an error occurred
        _calibPoint = AR1021_NUM_CALIB_POINTS+1;
    }



    return ret;
}

int AR1021I2C::cmd(char cmd, char* data, int len, char* respBuf, int* respLen,
        bool setCsOff) {

    int ret = 0;
    int readLen = (respLen == NULL) ? 0 : *respLen;
    for (int attempt = 1; attempt <= AR1021_RETRY; attempt++) {
        if (attempt > 1) {
            wait_ms(50);
        }

        // command request
        // ---------------
        // 0x00 0x55 len cmd data
        // 0x00 = protocol command byte
        // 0x55 = header
        // len = data length + cmd (1)
        // data = data to send

        _i2c.start(); 
        _i2c.write(AR1021_ADDR);                   //send write address 
        _i2c.write(0x00); 
        _i2c.write(0x55);                          //header 
        _i2c.write(len+1);                         //message length 
        _i2c.write(cmd);
        for (int i = 0; i < len; i++) {
            _i2c.write(data[i]);
        }
        wait_us(60);
        _i2c.stop();
     
        // wait for response (siq goes high when response is available)
        Timer t;
        t.start();
        while(_siq.read() != 1 && t.read_ms() < AR1021_TIMEOUT);
        
        if (t.read_ms() < AR1021_TIMEOUT) {
        
            // command response
            // ---------------
            // 0x55 len status cmd data
            // 0x55 = header
            // len = number of bytes following the len byte (i.e. including the status&cmd)
            // status = status
            // cmd = command ID
            // data = data to receive
            _i2c.start();
            _i2c.write(AR1021_ADDR + 1);        //send read address 
            char header = _i2c.read(1);         //header should always be 0x55
            if (header != 0x55) {
                ret = AR1021_ERR_NO_HDR;
                continue;
            }
            char length = _i2c.read(1);         //data length
            if (length < 2) {
                ret = AR1021_ERR_INV_LEN;       //must have at least status and command bytes
                continue;
            }
            length -= 2;
            if (length > readLen) {
                ret = AR1021_ERR_INV_LEN;       //supplied buffer is not enough
                continue;
            }

            char status = _i2c.read(1);         //command status
            char usedCmd;
            if (readLen <= 0) {
                usedCmd = _i2c.read(0);         //no data => read command byte + NACK
            } else {
                usedCmd = _i2c.read(1);         //which command

                //we need to send a NACK on the last read. 
                int i;
                for (i = 0; i < (length-1); i++) {
                    respBuf[i] = _i2c.read(1);
                }
                respBuf[i] = _i2c.read(0);      //last returned data byte + NACK
            }
            _i2c.stop();
            
            
            if (status != AR1021_RESP_STAT_OK) {
                ret = -status;
                continue;
            }
            if (usedCmd != cmd) {
                ret = AR1021_ERR_INV_RESP;
                continue;
            }
            
            // success
            ret = 0;
            break;
            
        } else {
            ret = AR1021_ERR_TIMEOUT;
            continue;
        }
    }

    return ret;
}

int AR1021I2C::waitForCalibResponse(uint32_t timeout) {
    Timer t;
    int ret = 0;

    t.start();

    // wait for siq
    while (_siq.read() != 1 &&
            (timeout == 0 || (uint32_t)t.read_ms() < (int)timeout));


    do {

        if (timeout >  0 && (uint32_t)t.read_ms() >= timeout) {
            ret = AR1021_ERR_TIMEOUT;
            break;
        }

        // command response
        // ---------------
        // 0x55 len status cmd data
        // 0x55 = header
        // len = number of bytes following the len byte (should be 2)
        // status = status
        // cmd = command ID
        _i2c.start();
        _i2c.write(AR1021_ADDR + 1);        //send read address 
        char header = _i2c.read(1);         //header should always be 0x55  
        char length = _i2c.read(1);         //data length

        if (header != 0x55) {
            ret = AR1021_ERR_NO_HDR;
            break;
        }
        if (length < 2) {
            ret = AR1021_ERR_INV_LEN;
            break;
        }
        char status = _i2c.read(1);         //status
        char cmd    = _i2c.read(0);         //command, should be NACK'ed
        _i2c.stop();
        if (status != AR1021_RESP_STAT_OK) {
            ret = -status;
            break;
        }
        if (cmd != AR1021_CMD_CALIBRATE_MODE) {
            ret = AR1021_ERR_INV_RESP;
            break;
        }
        
        // success
        ret = 0;

    } while (0);

    return ret;
}


void AR1021I2C::readTouchIrq() {
    while(_siq.read() == 1) {

        // touch coordinates are sent in a 5-byte data packet
        _i2c.start();
        _i2c.write(AR1021_ADDR + 1);        //send read address 
        int pen = _i2c.read(1);
        int xlo = _i2c.read(1);
        int xhi = _i2c.read(1);
        int ylo = _i2c.read(1);
        int yhi = _i2c.read(0);
        _i2c.stop();
        
        // pen down
        if ((pen&AR1021_PEN_MASK) == (1<<7|1<<0)) {
            _pen = 1;
        }
        // pen up
        else if ((pen&AR1021_PEN_MASK) == (1<<7)){
            _pen = 0;
        }
        // invalid value
        else {
            continue;
        }

        _x = ((xhi<<7)|xlo);
        _y = ((yhi<<7)|ylo);
    }
}

bool AR1021I2C::info(int* verHigh, int* verLow, int* resBits, int* type)
{
    char buff[3] = {0,0,0};
    int read = 3;
    int res = cmd(AR1021_CMD_GET_VERSION, NULL, 0, buff, &read);
    if (res == 0) {
        *verHigh = buff[0];
        *verLow = buff[1];
        switch(buff[2] & 3) {
            case 0:   
                *resBits = 8;
                break;
            case 1:   
                *resBits = 10;
                break;
            case 2:   
                *resBits = 12;
                break;
            case 3:   
                *resBits = 12;
                break;
            default:
                res = 25;
                printf("Invalid resolution %d\n", buff[2]&3);
                break;
        }
        *type = buff[2]>>2;
    }
    return (res == 0);
}
