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

#ifndef EALCDBOARD_H
#define EALCDBOARD_H

#include "LcdController.h"

/** An interface to Embedded Artists LCD Boards
 *
 */
class EaLcdBoard {
public:

    enum Result {
        Ok = 0,
        InvalidCommandString,
        InvalidArgument,
        InvalidStorage,
        BufferTooSmall,
        VersionNotSupported,
        LcdAccessError
    };

    enum Constants {
        NameBufferSize = 30
    };

    enum TouchPanelId {
        // must be first
        TouchPanelInvalidFirst = 0x80,

        // Texas Instruments Touch Screen Controller (TSC2046).
        TouchPanel_TSC2046,
        // Microchip Touch Screen Controller (AR1021).
        TouchPanel_AR1021,

        // Must be after last panel ID
        TouchPanelInvalidLast,
        // unknown touch panel
        TouchPanelUnknown = 0xFF

    };

    typedef struct {
      uint8_t swap;         // set to 1 if x and y should be swapped
      uint32_t xres;        // x resistance
      uint32_t yres;        // y resistance
      TouchPanelId panelId; // identifies touch panel

    } TouchParams_t;

    /** Create an interface to an Embedded Artists LCD Board
     *
     * @param sda I2C data line pin
     * @param scl I2C clock line pin
     */
    EaLcdBoard(PinName sda, PinName scl);

    /** Open the interface and start initialization.
     *
     * @param cfg initialize with a given LCD configuration. If this argument is
     *            NULL the LCD configuration will be retrieved from persistent
     *            storage on the LCD Board.
     * @param initSeq the initialization string. If this argument is NULL the
     *            initialization string will be retrieved from persistent
     *            storage on the LCD Board.
     *
     * @returns the result of the operation
     */
    Result open(LcdController::Config* cfg, char* initSeq);

    /** Close the interface
     *
     * @returns the result of the operation
     */
    Result close();

    /** Set and activate the address of the frame buffer to use.
     *
     *  It is the content of the frame buffer that is shown on the
     *  display. All the drawing on the frame buffer can be done
     *  'offline' and whenever it should be shown this function
     *  can be called with the address of the offline frame buffer.
     *
     *  @param address Memory address of the frame buffer
     *
     * @returns the result of the operation
     */
    Result setFrameBuffer(uint32_t address);

    /** Get the LCD configuration stored in persistent storage on the LCD Board
     *
     *  @param cfg pointer to a configuration object. Parameters are copied to
     *  this object.
     *
     * @returns the result of the operation
     */
    Result getLcdConfig(LcdController::Config* cfg);

    /** Get the display name stored in persistent storage on the LCD Board
     *
     *  @param buf buffer to which the name will be copied
     *  @param len size of the buffer in bytes
     *
     * @returns the result of the operation
     */
    Result getDisplayName(char* buf, int len);

    /** Get the display manufacturer stored in persistent storage on the
     *  LCD Board
     *
     *  @param buf buffer to which the name will be copied
     *  @param len size of the buffer in bytes
     *
     * @returns the result of the operation
     */
    Result getDisplayMfg(char* buf, int len);

    /** Get the initialization sequence stored in persistent storage on the
     *  LCD Board
     *
     *  @param buf buffer to which the string will be copied
     *  @param len size of the buffer in bytes
     *
     * @returns the result of the operation
     */
    Result getInitSeq(char* buf, int len);

    /** Get the power down sequence stored in persistent storage on the
     *  LCD Board
     *
     *  @param buf buffer to which the string will be copied
     *  @param len size of the buffer in bytes
     *
     * @returns the result of the operation
     */
    Result getPowerDownSeq(char* buf, int len);

    /**
     * Get the touch panel parameters stored in persistent storage
     *
     *  @param params pointer to a configuration object. Parameters are copied to
     *  this object.
     *
     * @returns the result of the operation
     */
    Result getTouchParameters(TouchParams_t* params);

    /**
     * Write display parameters to the EEPROM. Please use this function with
     * care since original parameters will be overwritten and cannot be restored.
     *
     * @param lcdName the name of the display
     * @param lcdMfg the display manufacturer
     * @param cfg the display configuration parameters
     * @param initSeqStr the initialization sequence string
     * @param pdSeqStr the power down sequence string
     * @param touch touch panel parameters
     */
    Result storeParameters(
      const char* lcdName,
      const char* lcdMfg,
      LcdController::Config* cfg,
      const char* initSeqStr,
      const char* pdSeqStr,
      TouchParams_t* touch,
      bool controlWp = false);


private:

    typedef struct {
        uint32_t magic;        // magic number
        uint8_t lcd_name[NameBufferSize]; // LCD name
        uint8_t lcd_mfg[NameBufferSize]; // manufacturer name
        uint16_t lcdParamOff;  // offset to LCD parameters
        uint16_t initOff;      // offset to init sequence string
        uint16_t pdOff;        // offset to power down sequence string
        uint16_t tsOff;        // offset to touch parameters
        uint16_t end;          // end offset
    } store_t;

    Result getStore(store_t* store);

    int eepromRead(uint8_t* buf, uint16_t offset, uint16_t len);
    int eepromWrite(uint8_t* buf, uint16_t offset, uint16_t len);

    Result parseInitString(char* str, LcdController::Config* cfg);
    Result checkVersion(char* v, uint32_t len);
    Result execDelay(char* del, uint32_t len);
    Result execSeqCtrl(char* cmd, uint32_t len);
    Result execPinSet(char* cmd, uint32_t len);

    void setLsStates(uint16_t states, uint8_t* ls, uint8_t mode);
    void setLeds(void);
    void pca9532_setLeds (uint16_t ledOnMask, uint16_t ledOffMask);
    void pca9532_setBlink0Period(uint8_t period);
    void pca9532_setBlink0Duty(uint8_t duty);
    void pca9532_setBlink0Leds(uint16_t ledMask);

    virtual void setWriteProtect(bool enable);
    virtual void set3V3Signal(bool enabled);
    virtual void set5VSignal(bool enabled);
    virtual void setDisplayEnableSignal(bool enabled);
    virtual void setBacklightContrast(uint32_t value);

    I2C _i2c;
    LcdController::Config _cfg;
    LcdController lcdCtrl;
    uint16_t _blink0Shadow;
    uint16_t _blink1Shadow;
    uint16_t _ledStateShadow;
    bool _lcdPwrOn;
};

#endif
