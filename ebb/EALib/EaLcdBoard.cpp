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
#include "EaLcdBoard.h"

/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/

#define PORT_PIN_BUG_IN_MBED_SDK

#define LCDB_MAGIC 0xEA01CDAE


#define LCDB_PCA9532_I2C_ADDR    (0x64 << 1)

/* PCA9532 registers*/
#define LCDB_PCA9532_INPUT0   0x00
#define LCDB_PCA9532_INPUT1   0x01
#define LCDB_PCA9532_PSC0     0x02
#define LCDB_PCA9532_PWM0     0x03
#define LCDB_PCA9532_PSC1     0x04
#define LCDB_PCA9532_PWM1     0x05
#define LCDB_PCA9532_LS0      0x06
#define LCDB_PCA9532_LS1      0x07
#define LCDB_PCA9532_LS2      0x08
#define LCDB_PCA9532_LS3      0x09
#define LCDB_PCA9532_AUTO_INC 0x10

#define LCDB_LS_MODE_ON     0x01
#define LCDB_LS_MODE_BLINK0 0x02
#define LCDB_LS_MODE_BLINK1 0x03

#define LCDB_CTRL_3V3     0x0001
#define LCDB_CTRL_5V      0x0002
#define LCDB_CTRL_DISP_EN 0x0010
#define LCDB_CTRL_BL_EN   0x0080
#define LCDB_CTRL_BL_C    0x0100
#define LCDB_EEPROM_WP    0x8000

#define LCDB_EEPROM_I2C_ADDR  (0x56 << 1)
#define LCDB_EEPROM_PAGE_SIZE     32
#define LCDB_EEPROM_TOTAL_SIZE  8192

/*
 * Set which sequence string version that is supported
 */
#define LCDB_SEQ_VER 2

#ifndef MIN
#define MIN(x, y) (((x)<(y))?(x):(y))
#endif

#define EA_LCD_TMP_BUFFER_SZ 256
static char* eaLcdTmpBuffer[EA_LCD_TMP_BUFFER_SZ];


/* Structure containing the parameters for the LCD panel as stored on LCD Board */

/* LCD display types */
typedef enum {
    TFT = 0,   /* standard TFT */
    ADTFT,     /* advanced TFT */
    HRTFT,     /* highly reflective TFT */
    MONO_4BIT, /* 4-bit mono */
    MONO_8BIT, /* 8-bit mono */
    CSTN       /* color STN */
} nxp_lcd_panel_t;

typedef struct {
    uint8_t         h_back_porch;         /* Horizontal back porch in clocks */
    uint8_t         h_front_porch;        /* Horizontal front porch in clocks */
    uint8_t         h_sync_pulse_width;   /* HSYNC pulse width in clocks */
    uint16_t        pixels_per_line;      /* Pixels per line (horizontal resolution) */
    uint8_t         v_back_porch;         /* Vertical back porch in clocks */
    uint8_t         v_front_porch;        /* Vertical front porch in clocks */
    uint8_t         v_sync_pulse_width;   /* VSYNC pulse width in clocks */
    uint16_t        lines_per_panel;      /* Lines per panel (vertical resolution) */
    uint8_t         invert_output_enable; /* Invert output enable, 1 = invert*/
    uint8_t         invert_panel_clock;   /* Invert panel clock, 1 = invert*/
    uint8_t         invert_hsync;         /* Invert HSYNC, 1 = invert */
    uint8_t         invert_vsync;         /* Invert VSYNC, 1 = invert */
    uint8_t         ac_bias_frequency;    /* AC bias frequency in clocks */
    uint8_t         bits_per_pixel;       /* Maximum bits per pixel the display supports */
    uint32_t        optimal_clock;        /* Optimal clock rate (Hz) */
    nxp_lcd_panel_t lcd_panel_type;       /* LCD panel type */
    uint8_t         dual_panel;           /* Dual panel, 1 = dual panel display */
} nxp_lcd_param_t;

static uint32_t str_to_uint(char* str, uint32_t len);

EaLcdBoard::EaLcdBoard(PinName sda, PinName scl) : _i2c(sda, scl) {
    _blink0Shadow = 0;
    _blink1Shadow = 0;
    _ledStateShadow = 0;
    _lcdPwrOn = false;
}

EaLcdBoard::Result EaLcdBoard::open(LcdController::Config* cfg, char* initSeq) {

    EaLcdBoard::Result result = Ok;

    // load LCD configuration from storage
    if (cfg == NULL) {
        result = getLcdConfig(&_cfg);
        cfg = &_cfg;
    }

    // load init sequence from storage
    if (result == Ok && initSeq == NULL) {
        result = getInitSeq((char*)eaLcdTmpBuffer, EA_LCD_TMP_BUFFER_SZ);
        initSeq = (char*)eaLcdTmpBuffer;
    }

    if (result != Ok) {
        return result;
    }

    return parseInitString(initSeq, cfg);
}

EaLcdBoard::Result EaLcdBoard::close() {
    int r = 0;

    do {
        r = lcdCtrl.setPower(false);
        if (r != 0) break;

        _lcdPwrOn = false;

        r = lcdCtrl.close();
    } while(0);

    if (r != 0) {
        return LcdAccessError;
    }

    return Ok;
}

EaLcdBoard::Result EaLcdBoard::setFrameBuffer(uint32_t address) {
    int r = 0;

    do {

        // begin by powering on the display
        if (!_lcdPwrOn) {
            r = lcdCtrl.setPower(true);
            if (r != 0) break;

            _lcdPwrOn = true;
        }

        // activate specified frame buffer
        r = lcdCtrl.setFrameBuffer(address);
        if (r != 0) break;

    } while(0);

    if (r != 0) {
        return LcdAccessError;
    }


    return Ok;
}

EaLcdBoard::Result EaLcdBoard::getLcdConfig(LcdController::Config* cfg) {
    store_t h;

    nxp_lcd_param_t lcdParam;

    if (cfg == NULL) {
        return InvalidArgument;
    }

    getStore(&h);

    if (h.magic != LCDB_MAGIC) {
        return InvalidStorage;
    }

    eepromRead((uint8_t*)&lcdParam, h.lcdParamOff,
            (h.initOff-h.lcdParamOff));

    cfg->horizontalBackPorch  = lcdParam.h_back_porch;
    cfg->horizontalFrontPorch = lcdParam.h_front_porch;
    cfg->hsync                = lcdParam.h_sync_pulse_width;
    cfg->width                = lcdParam.pixels_per_line;
    cfg->verticalBackPorch    = lcdParam.v_back_porch;
    cfg->verticalFrontPorch   = lcdParam.v_front_porch;
    cfg->vsync                = lcdParam.v_sync_pulse_width;
    cfg->height               = lcdParam.lines_per_panel;
    cfg->invertOutputEnable   = (lcdParam.invert_output_enable == 1);
    cfg->invertPanelClock     = (lcdParam.invert_panel_clock == 1);
    cfg->invertHsync          = (lcdParam.invert_hsync == 1);
    cfg->invertVsync          = (lcdParam.invert_vsync == 1);
    cfg->acBias               = lcdParam.ac_bias_frequency;
    cfg->bpp = LcdController::Bpp_16_565;
    cfg->optimalClock         = lcdParam.optimal_clock;
    cfg->panelType            = (LcdController::LcdPanel)lcdParam.lcd_panel_type;
    cfg->dualPanel            = (lcdParam.dual_panel == 1);

    return Ok;
}

EaLcdBoard::Result EaLcdBoard::getDisplayName(char* buf, int len) {
    store_t h;

    if (buf == NULL) {
        return InvalidArgument;
    }

    getStore(&h);

    if (h.magic != LCDB_MAGIC) {
        return InvalidStorage;
    }

    if (len < NameBufferSize) {
        return BufferTooSmall;
    }

    strncpy(buf, (char*)h.lcd_name, NameBufferSize);

    return Ok;
}

EaLcdBoard::Result EaLcdBoard::getDisplayMfg(char* buf, int len) {
    store_t h;

    if (buf == NULL) {
        return InvalidArgument;
    }

    getStore(&h);

    if (h.magic != LCDB_MAGIC) {
        return InvalidStorage;
    }

    if (len < NameBufferSize) {
        return BufferTooSmall;
    }

    strncpy(buf, (char*)h.lcd_mfg, NameBufferSize);

    return Ok;
}

EaLcdBoard::Result EaLcdBoard::getInitSeq(char* buf, int len) {
    store_t h;

    if (buf == NULL) {
        return InvalidArgument;
    }

    getStore(&h);

    if (h.magic != LCDB_MAGIC) {
        return InvalidStorage;
    }

    if ((h.pdOff-h.initOff) > len) {
        return BufferTooSmall;
    }

    eepromRead((uint8_t*)buf, h.initOff,
            (h.pdOff-h.initOff));

    return Ok;
}

EaLcdBoard::Result EaLcdBoard::getPowerDownSeq(char* buf, int len) {
    store_t h;

    if (buf == NULL) {
        return InvalidArgument;
    }

    getStore(&h);

    if (h.magic != LCDB_MAGIC) {
        return InvalidStorage;
    }

    if ((h.tsOff-h.pdOff) > len) {
        return BufferTooSmall;
    }

    eepromRead((uint8_t*)buf, h.pdOff,
            (h.tsOff-h.pdOff));

    return Ok;
}

EaLcdBoard::Result EaLcdBoard::getTouchParameters(TouchParams_t* params) {
    store_t h;

    if (params == NULL) {
        return InvalidArgument;
    }

    getStore(&h);

    if (h.magic != LCDB_MAGIC) {
        return InvalidStorage;
    }


    if (eepromRead((uint8_t*)params, h.tsOff,
            (h.end-h.tsOff)) == -1) {
        return InvalidStorage;
    }


    if (params->panelId <= TouchPanelInvalidFirst
            || params->panelId >= TouchPanelInvalidLast) {
        params->panelId = TouchPanelUnknown;
    }


    return Ok;
}

EaLcdBoard::Result EaLcdBoard::storeParameters(
  const char* lcdName,
  const char* lcdMfg,
  LcdController::Config* cfg,
  const char* initSeqStr,
  const char* pdSeqStr,
  TouchParams_t* touch,
  bool controlWp)
{
    store_t h;
    nxp_lcd_param_t lcdParam;

    if (lcdName == NULL || lcdMfg == NULL || cfg == NULL
            || initSeqStr == NULL || pdSeqStr == NULL) {
        return InvalidArgument;

    }


    lcdParam.h_back_porch         = cfg->horizontalBackPorch;
    lcdParam.h_front_porch        = cfg->horizontalFrontPorch;
    lcdParam.h_sync_pulse_width   = cfg->hsync;
    lcdParam.pixels_per_line      = cfg->width;
    lcdParam.v_back_porch         = cfg->verticalBackPorch;
    lcdParam.v_front_porch        = cfg->verticalFrontPorch;
    lcdParam.v_sync_pulse_width   = cfg->vsync;
    lcdParam.lines_per_panel      = cfg->height;
    lcdParam.invert_output_enable = (cfg->invertOutputEnable ? 1 : 0);
    lcdParam.invert_panel_clock   = (cfg->invertPanelClock ? 1 : 0);
    lcdParam.invert_hsync         = (cfg->invertHsync ? 1 : 0);
    lcdParam.invert_vsync         = (cfg->invertVsync ? 1 : 0);
    lcdParam.ac_bias_frequency    = cfg->acBias;
    lcdParam.optimal_clock        = cfg->optimalClock;
    lcdParam.lcd_panel_type       = (nxp_lcd_panel_t)cfg->panelType;
    lcdParam.dual_panel           = (cfg->dualPanel ? 1 : 0);


    h.magic = LCDB_MAGIC;
    strncpy((char*)h.lcd_name, lcdName, 30);
    strncpy((char*)h.lcd_mfg, lcdMfg, 30);

    h.lcdParamOff = sizeof(store_t);
    h.initOff     = h.lcdParamOff + sizeof(nxp_lcd_param_t);
    h.pdOff       = h.initOff + strlen(initSeqStr)+1;
    h.tsOff       = h.pdOff + strlen(pdSeqStr)+1;
    h.end         = h.tsOff + sizeof(TouchParams_t);

    if (controlWp) setWriteProtect(false);
    eepromWrite((uint8_t*)&h,         0,             h.lcdParamOff);
    eepromWrite((uint8_t*)&lcdParam,  h.lcdParamOff, (h.initOff-h.lcdParamOff));
    eepromWrite((uint8_t*)initSeqStr, h.initOff,     (h.pdOff-h.initOff));
    eepromWrite((uint8_t*)pdSeqStr,   h.pdOff,       (h.tsOff-h.pdOff));
    eepromWrite((uint8_t*)touch,      h.tsOff,       (h.end-h.tsOff));
    if (controlWp) setWriteProtect(true);

    return Ok;
}

EaLcdBoard::Result EaLcdBoard::getStore(store_t* store) {
    int num = 0;

    if (store == NULL) return InvalidArgument;

    num = eepromRead((uint8_t*)store, 0, sizeof(store_t));
    if (num < (int)sizeof(store_t)) {
        return InvalidStorage;
    }

    return Ok;
}

// ###########################
// An EEPROM is used for persistent storage
// ###########################

int EaLcdBoard::eepromRead(uint8_t* buf, uint16_t offset, uint16_t len) {
    int i = 0;
    char off[2];
    uint16_t retLen = 0;

    if (len > LCDB_EEPROM_TOTAL_SIZE || offset+len > LCDB_EEPROM_TOTAL_SIZE) {
        return -1;
    }

    wait_ms(10);

    off[0] = ((offset >> 8) & 0xff);
    off[1] = (offset & 0xff);

    do {
        if (_i2c.write(LCDB_EEPROM_I2C_ADDR, (char*)off, 2) != 0) break;
        for ( i = 0; i < 0x2000; i++);
        if (_i2c.read(LCDB_EEPROM_I2C_ADDR, (char*)buf, len) != 0) break;

        retLen = len;
    } while(0);

    return retLen;
}

int EaLcdBoard::eepromWrite(uint8_t* buf, uint16_t offset, uint16_t len) {
    int16_t written = 0;
    uint16_t wLen = 0;
    uint16_t off = offset;
    uint8_t tmp[LCDB_EEPROM_PAGE_SIZE+2];

    if (len > LCDB_EEPROM_TOTAL_SIZE || offset+len > LCDB_EEPROM_TOTAL_SIZE) {
        return -1;
    }

    wait_ms(1);

    wLen = LCDB_EEPROM_PAGE_SIZE - (off % LCDB_EEPROM_PAGE_SIZE);
    wLen = MIN(wLen, len);

    while (len) {
        tmp[0] = ((off >> 8) & 0xff);
        tmp[1] = (off & 0xff);
        memcpy(&tmp[2], (void*)&buf[written], wLen);
        if (_i2c.write(LCDB_EEPROM_I2C_ADDR, (char*)tmp, wLen+2) != 0) break;

        // delay to wait for a write cycle
        //eepromDelay();
        wait_ms(10);

        len     -= wLen;
        written += wLen;
        off     += wLen;

        wLen = MIN(LCDB_EEPROM_PAGE_SIZE, len);
    }

    return written;
}

// ###########################
// string parsing (initialization and power down strings)
// ###########################

EaLcdBoard::Result EaLcdBoard::parseInitString(char* str, LcdController::Config* cfg) {
    char* c = NULL;
    uint32_t len = 0;
    Result result = Ok;

    if (str == NULL) {
        return InvalidCommandString;
    }

    while(*str != '\0') {

        // skip whitespaces
        while(*str == ' ') {
            str++;
        }

        c = str;

        // find end of command
        while(*str != ',' && *str != '\0') {
            str++;
        }

        len = (str-c);

        if (*str == ',') {
            str++;
        }

        switch (*c++) {

        case 'v':
            result = checkVersion(c, len-1);
            break;

            // sequence control command (pca9532)
        case 'c':
            execSeqCtrl(c, len-1);
            break;

            // delay
        case 'd':
            execDelay(c, len-1);
            break;

            // open lcd (init LCD controller)
        case 'o':

            if (cfg != NULL) {

                if (lcdCtrl.open(cfg) != 0) {
                    result = LcdAccessError;
                }
            }

            else {
                result = InvalidArgument;
            }

            break;

            // exec pin set
        case 'p':
            execPinSet(c, len-1);
            break;

        }

        if (result != Ok) {
            break;
        }


    }


    return result;
}

EaLcdBoard::Result EaLcdBoard::checkVersion(char* v, uint32_t len) {
    uint32_t ver = str_to_uint(v, len);

    if (ver > LCDB_SEQ_VER) {
        return VersionNotSupported;
    }

    return Ok;
}

EaLcdBoard::Result EaLcdBoard::execDelay(char* del, uint32_t len) {
    wait_ms(str_to_uint(del, len));

    return Ok;
}

EaLcdBoard::Result EaLcdBoard::execSeqCtrl(char* cmd, uint32_t len) {

    switch (*cmd++) {

    // display enable
    case 'd':
        setDisplayEnableSignal(*cmd == '1');
        break;

        // backlight contrast
    case 'c':
        setBacklightContrast(str_to_uint(cmd, len));
        break;

        // 3v3 enable
    case '3':
        set3V3Signal(*cmd == '1');
        break;

        // 5v enable
    case '5':
        set5VSignal(*cmd == '1');
        break;
    }

    return Ok;
}

#ifdef PORT_PIN_BUG_IN_MBED_SDK
static PinName port_pin2(PortName port, int pin_n) {
    return (PinName)(((port << 5) | pin_n));
}
#endif


EaLcdBoard::Result EaLcdBoard::execPinSet(char* cmd, uint32_t len) {

    PortName port;

    do {
        // cmd: 0_02=1 means p0.2 = 1
        if (len < 6) break;
        if (cmd[1] != '_' || cmd[4] != '=') break;

        // port
        int portnum  = cmd[0] - '0';
        if (portnum < 0 || portnum > 5) break;
        port = (PortName)portnum;

        // pin
        int pinnum = (10*(cmd[2]-'0'))+ cmd[3]-'0';
        if (pinnum < 0 || pinnum > 31) break;

        // value
        int value = cmd[5]-'0';
        if (!(value == 0 || value == 1)) break;

#ifdef PORT_PIN_BUG_IN_MBED_SDK
        PinName pin = port_pin2(port, pinnum);
#else
        PinName pin = port_pin(port, pinnum);
#endif

        gpio_t gp;
        gpio_init(&gp, pin);
        gpio_dir(&gp, PIN_OUTPUT);
        gpio_mode(&gp, PullNone);
        gpio_write(&gp, value);

        return Ok;

    } while (false);


    return InvalidCommandString;
}


// ###########################
// PCA9532 is used as a control inteface to the display.
// voltages can be turned on/off and backlight can be controlled.
// ###########################

// Helper function to set LED states
void EaLcdBoard::setLsStates(uint16_t states, uint8_t* ls, uint8_t mode)
{
#define IS_LED_SET(bit, x) ( ( ((x) & (bit)) != 0 ) ? 1 : 0 )

    int i = 0;

    for (i = 0; i < 4; i++) {

        ls[i] |= ( (IS_LED_SET(0x0001, states)*mode << 0)
                | (IS_LED_SET(0x0002, states)*mode << 2)
                | (IS_LED_SET(0x0004, states)*mode << 4)
                | (IS_LED_SET(0x0008, states)*mode << 6) );

        states >>= 4;
    }
}

void EaLcdBoard::setLeds(void)
{
    uint8_t buf[5];
    uint8_t ls[4] = {0,0,0,0};
    uint16_t states = _ledStateShadow;

    // LEDs in On/Off state
    setLsStates(states, ls, LCDB_LS_MODE_ON);

    // set the LEDs that should blink
    setLsStates(_blink0Shadow, ls, LCDB_LS_MODE_BLINK0);
    setLsStates(_blink1Shadow, ls, LCDB_LS_MODE_BLINK1);

    buf[0] = LCDB_PCA9532_LS0 | LCDB_PCA9532_AUTO_INC;
    buf[1] = ls[0];
    buf[2] = ls[1];
    buf[3] = ls[2];
    buf[4] = ls[3];

    _i2c.write(LCDB_PCA9532_I2C_ADDR, (char*)buf, 5);
}

void EaLcdBoard::pca9532_setLeds (uint16_t ledOnMask, uint16_t ledOffMask)
{
    // turn off leds
    _ledStateShadow &= (~(ledOffMask) & 0xffff);

    // ledOnMask has priority over ledOffMask
    _ledStateShadow |= ledOnMask;

    // turn off blinking
    _blink0Shadow &= (~(ledOffMask) & 0xffff);
    _blink1Shadow &= (~(ledOffMask) & 0xffff);

    setLeds();
}

void EaLcdBoard::pca9532_setBlink0Period(uint8_t period)
{
    uint8_t buf[2];

    buf[0] = LCDB_PCA9532_PSC0;
    buf[1] = period;

    _i2c.write(LCDB_PCA9532_I2C_ADDR, (char*)buf, 2);
}

void EaLcdBoard::pca9532_setBlink0Duty(uint8_t duty)
{
    uint8_t buf[2];
    uint32_t tmp = duty;
    if (tmp > 100) {
        tmp = 100;
    }

    tmp = (255 * tmp)/100;

    buf[0] = LCDB_PCA9532_PWM0;
    buf[1] = tmp;

    _i2c.write(LCDB_PCA9532_I2C_ADDR, (char*)buf, 2);
}

void EaLcdBoard::pca9532_setBlink0Leds(uint16_t ledMask)
{
    _blink0Shadow |= ledMask;
    setLeds();
}

void EaLcdBoard::setWriteProtect(bool enable)
{
    if (enable) {
        pca9532_setLeds(0, LCDB_EEPROM_WP);
    } else {
        pca9532_setLeds(LCDB_EEPROM_WP, 0);
    }
}

void EaLcdBoard::set3V3Signal(bool enabled) {
    if (enabled) {
        pca9532_setLeds(LCDB_CTRL_3V3, 0);
    } else {
        pca9532_setLeds(0, LCDB_CTRL_3V3);
    }
}

void EaLcdBoard::set5VSignal(bool enabled) {
    if (enabled) {
        pca9532_setLeds(LCDB_CTRL_5V, 0);
    } else {
        pca9532_setLeds(0, LCDB_CTRL_5V);
    }
}

void EaLcdBoard::setDisplayEnableSignal(bool enabled) {
    if (!enabled) {
        pca9532_setLeds(LCDB_CTRL_DISP_EN, 0);
    } else {
        pca9532_setLeds(0, LCDB_CTRL_DISP_EN);
    }
}

void EaLcdBoard::setBacklightContrast(uint32_t value) {

    if (value > 100) return;

    pca9532_setBlink0Duty(100-value);
    pca9532_setBlink0Period(0);
    pca9532_setBlink0Leds(LCDB_CTRL_BL_C);
}


// convert string to integer
static uint32_t str_to_uint(char* str, uint32_t len)
{
    uint32_t val = 0;

    while(len > 0 && *str <= '9' && *str >= '0') {
        val = (val * 10) + (*str - '0');
        str++;
        len--;
    }

    return val;
}


