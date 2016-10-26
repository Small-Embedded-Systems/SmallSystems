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
#include "LcdController.h"

/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/

#undef _SBF
#define _SBF(p,v) (((uint32_t)(v)) << (p))

#undef _BITMASK
#define _BITMASK(field_width) (_BIT(field_width) - 1)

#undef _BIT
#define _BIT(p) (((uint32_t)(1)) << (p))

/***********************************************************************
 * Color LCD controller horizontal axis plane control register definitions
 **********************************************************************/

/* LCD controller horizontal axis plane control register pixels per line */
#define CLCDC_LCDTIMING0_PPL_WIDTH 6
#define CLCDC_LCDTIMING0_PPL(n) _SBF(2, (((n) / 16) - 1) & _BITMASK(CLCDC_LCDTIMING0_PPL_WIDTH))
/* LCD controller horizontal axis plane control register HSYNC pulse width */
#define CLCDC_LCDTIMING0_HSW_WIDTH 8
#define CLCDC_LCDTIMING0_HSW(n) _SBF(8, ((n) - 1) & _BITMASK(CLCDC_LCDTIMING0_HSW_WIDTH))
/* LCD controller horizontal axis plane control register horizontal front porch */
#define CLCDC_LCDTIMING0_HFP_WIDTH 8
#define CLCDC_LCDTIMING0_HFP(n) _SBF(16, ((n) - 1) & _BITMASK(CLCDC_LCDTIMING0_HFP_WIDTH))
/* LCD controller horizontal axis plane control register horizontal back porch */
#define CLCDC_LCDTIMING0_HBP_WIDTH 8
#define CLCDC_LCDTIMING0_HBP(n) _SBF(24, ((n) - 1) & _BITMASK(CLCDC_LCDTIMING0_HBP_WIDTH))

/***********************************************************************
 * Color LCD controller vertical axis plane control register definitions
 **********************************************************************/

/* LCD controller vertical axis plane control register lines per panel */
#define CLCDC_LCDTIMING1_LPP_WIDTH 10
#define CLCDC_LCDTIMING1_LPP(n) _SBF(0, ((n) - 1) & _BITMASK(CLCDC_LCDTIMING1_LPP_WIDTH))
/* LCD controller vertical axis plane control register VSYNC pulse width */
#define CLCDC_LCDTIMING1_VSW_WIDTH 6
#define CLCDC_LCDTIMING1_VSW(n) _SBF(10, ((n) - 1) & _BITMASK(CLCDC_LCDTIMING1_VSW_WIDTH))
/* LCD controller vertical axis plane control register vertical front porch */
#define CLCDC_LCDTIMING1_VFP_WIDTH 8
#define CLCDC_LCDTIMING1_VFP(n) _SBF(16, (n) & _BITMASK(CLCDC_LCDTIMING1_VFP_WIDTH))
/* LCD controller vertical axis plane control register vertical back porch */
#define CLCDC_LCDTIMING1_VBP_WIDTH 8
#define CLCDC_LCDTIMING1_VBP(n) _SBF(24, (n) & _BITMASK(CLCDC_LCDTIMING1_VBP_WIDTH))

/***********************************************************************
 * Color LCD controller clock and signal polarity control register definitions
 **********************************************************************/

/* LCD controller clock and signal polarity control register panel clock divisor low*/
#define CLCDC_LCDTIMING2_PCD_LO_WIDTH 5
#define CLCDC_LCDTIMING2_PCD_LO(n) _SBF(0, ((n) - 2) & _BITMASK(CLCDC_LCDTIMING2_PCD_LO_WIDTH))
/* LCD controller clock and signal polarity control register clock select */
#define CLCDC_LCDTIMING2_CLKSEL _BIT(5)
/* LCD controller clock and signal polarity control register AC bias pin frequency */
#define CLCDC_LCDTIMING2_ACB_WIDTH 5
#define CLCDC_LCDTIMING2_ACB(n) _SBF(6, ((n) - 1) & _BITMASK(CLCDC_LCDTIMING2_ACB_WIDTH))
/* LCD controller clock and signal polarity control register invert VSYNC */
#define CLCDC_LCDTIMING2_IVS    _BIT(11)
/* LCD controller clock and signal polarity control register invert HSYNC */
#define CLCDC_LCDTIMING2_IHS    _BIT(12)
/* LCD controller clock and signal polarity control register invert plane clock */
#define CLCDC_LCDTIMING2_IPC    _BIT(13)
/* LCD controller clock and signal polarity control register invert output enable */
#define CLCDC_LCDTIMING2_IOE    _BIT(14)
/* LCD controller clock and signal polarity control register clocks per line */
#define CLCDC_LCDTIMING2_CPL_WIDTH 10
#define CLCDC_LCDTIMING2_CPL(n) _SBF(16, (n) & _BITMASK(CLCDC_LCDTIMING2_CPL_WIDTH))
/* LCD controller clock and signal polarity control register bypass pixel clock divider */
#define CLCDC_LCDTIMING2_BCD    _BIT(26)
/* LCD controller clock and signal polarity control register panel clock divisor high*/
#define CLCDC_LCDTIMING2_PCD_HI_WIDTH 5
#define CLCDC_LCDTIMING2_PCD_HI(n) _SBF((27 - CLCDC_LCDTIMING2_PCD_LO_WIDTH), ((n) - 2) & _SBF(CLCDC_LCDTIMING2_PCD_LO_WIDTH, _BITMASK(CLCDC_LCDTIMING2_PCD_HI_WIDTH)))


/***********************************************************************
 * Color LCD controller control register definitions
 **********************************************************************/

/* LCD control enable bit */
#define CLCDC_LCDCTRL_ENABLE    (1<<0)
/* LCD control 1 bit per pixel bit field */
#define CLCDC_LCDCTRL_BPP1      (0 << 1)
/* LCD control 2 bits per pixel bit field */
#define CLCDC_LCDCTRL_BPP2      (1 << 1)
/* LCD control 4 bits per pixel bit field */
#define CLCDC_LCDCTRL_BPP4      (2 << 1)
/* LCD control 8 bits per pixel bit field */
#define CLCDC_LCDCTRL_BPP8      (3 << 1)
/* LCD control 16 bits per pixel bit field */
#define CLCDC_LCDCTRL_BPP16     (4 << 1)
/* LCD control 24 bits per pixel bit field */
#define CLCDC_LCDCTRL_BPP24     (5 << 1)
/* LCD control 16 bits (5:6:5 mode) per pixel bit field */
#define CLCDC_LCDCTRL_BPP16_565_MODE (6 << 1)
/* LCD control 12 bits (4:4:4 mode) per pixel bit field */
#define CLCDC_LCDCTRL_BPP12_444_MODE (7 << 1)
/* LCD control mono select bit */
#define CLCDC_LCDCTRL_BW_COLOR  (0 << 4)
#define CLCDC_LCDCTRL_BW_MONO   (1 << 4)
/* LCD controler TFT select bit */
#define CLCDC_LCDCTRL_TFT       (1 << 5)
/* LCD control monochrome LCD has 4-bit/8-bit select bit */
#define CLCDC_LCDCTRL_MON8      (1 << 6)
/* LCD control dual panel select bit */
#define CLCDC_LCDCTRL_DUAL      (1 << 7)
/* LCD control RGB or BGR format select bit */
#define CLCDC_LCDCTRL_RGB       (0 << 8)
#define CLCDC_LCDCTRL_BGR       (1 << 8)
/* LCD control big-endian byte order select bit */
#define CLCDC_LCDCTRL_BEBO      (1 << 9)
/* LCD control big-endian pixel order within a byte select bit */
#define CLCDC_LCDCTRL_BEPO      (1 << 10)
/* LCD control power enable bit */
#define CLCDC_LCDCTRL_PWR       (1 << 11)
/* LCD control VCOMP interrupt is start of VSYNC */
#define CLCDC_LCDCTRL_VCOMP_VS  (0 << 12)
/* LCD control VCOMP interrupt is start of back porch */
#define CLCDC_LCDCTRL_VCOMP_BP  (1 << 12)
/* LCD control VCOMP interrupt is start of active video */
#define CLCDC_LCDCTRL_VCOMP_AV  (2 << 12)
/* LCD control VCOMP interrupt is start of front porch */
#define CLCDC_LCDCTRL_VCOMP_FP  (3 << 12)
/* LCD control watermark level is 8 or more words free bit */
#define CLCDC_LCDCTRL_WATERMARK (1 << 16)



bool LcdController::_lcdControllerUsed = false;

LcdController::LcdController() {
  _opened = false;
}

int LcdController::open(LcdController::Config* cfg) {
    if (_lcdControllerUsed) return 1;
    if (cfg == NULL) return 1;

    // enable power for LCD controller
    LPC_SC->PCONP |= 0x00000001;

    pinConfig();
    init(cfg);

    // only one instance at a time is allowed to be used
    _lcdControllerUsed = true;
    _opened = true;

    return 0;
}

int LcdController::close() {

    if (!_opened) return 1;

    if (_lcdControllerUsed) {

        // disable power for LCD controller
        LPC_SC->PCONP &= ~(0x00000001);

        _lcdControllerUsed = false;
        _opened = false;
    }


    return 0;
}

int LcdController::setFrameBuffer(uint32_t address) {
    if (!_opened) return 1;

    LPC_LCD->UPBASE = address;

    return 0;
}

int LcdController::setPower(bool on) {
    if (!_opened) return 1;

    if (on) {
        LPC_LCD->CTRL |= CLCDC_LCDCTRL_ENABLE;
        LPC_LCD->CTRL |= CLCDC_LCDCTRL_PWR;
    }
    else {
        LPC_LCD->CTRL &= ~CLCDC_LCDCTRL_PWR;
        LPC_LCD->CTRL &= ~CLCDC_LCDCTRL_ENABLE;
    }

    return 0;
}

void LcdController::init(LcdController::Config* cfg) {
    uint32_t tmp, i;

    // Disable the display in case it is on
    LPC_LCD->CTRL &= ~CLCDC_LCDCTRL_ENABLE;

    // Generate the horizontal axis plane control word
    tmp = (CLCDC_LCDTIMING0_PPL(cfg->width) |
            CLCDC_LCDTIMING0_HSW(cfg->hsync) |
            CLCDC_LCDTIMING0_HFP(cfg->horizontalFrontPorch) |
            CLCDC_LCDTIMING0_HBP(cfg->horizontalBackPorch));
    LPC_LCD->TIMH = tmp;

    // Generate the vertical axis plane control word
    tmp = (CLCDC_LCDTIMING1_LPP(cfg->height) |
            CLCDC_LCDTIMING1_VSW(cfg->vsync) |
            CLCDC_LCDTIMING1_VFP(cfg->verticalFrontPorch) |
            CLCDC_LCDTIMING1_VBP(cfg->verticalBackPorch));
    LPC_LCD->TIMV = tmp;

    // Generate the clock and signal polarity control word
    if(cfg->acBias != 0)
    {
        /* STN panel has AC bias value */
        tmp = CLCDC_LCDTIMING2_ACB(cfg->acBias);
    }
    else
    {
        tmp = 0;
    }

    if (cfg->invertOutputEnable)
    {
        tmp |= CLCDC_LCDTIMING2_IOE;
    }

    if (cfg->invertPanelClock)
    {
        tmp |= CLCDC_LCDTIMING2_IPC;
    }

    if (cfg->invertHsync)
    {
        tmp |= CLCDC_LCDTIMING2_IHS;
    }

    if (cfg->invertVsync)
    {
        tmp |= CLCDC_LCDTIMING2_IVS;
    }

    // Compute clocks per line based on panel type
    switch (cfg->panelType)
    {
    case Mono_4Bit:
        // Clocks per line is a quarter of pixels per line
        tmp = tmp | CLCDC_LCDTIMING2_CPL((cfg->width / 4) - 1);
        break;

    case Mono_8Bit:
        // Clocks per line is an eighth of pixels per line
        tmp = tmp | CLCDC_LCDTIMING2_CPL((cfg->width / 8) - 1);
        break;

    case ColorStn:
        // CSTN Clocks per line (* 3 / 8)
        tmp = tmp | CLCDC_LCDTIMING2_CPL(((cfg->width * 3) / 8) - 1);
        break;

    case Tft:
    case AdTft:
    case HrTft:
    default:
        // Clocks per line and pixels per line are the same
        tmp = tmp | CLCDC_LCDTIMING2_CPL(cfg->width - 1);
        break;
    }

    // clock
    tmp = tmp | getClockDivisor(cfg->optimalClock);

    LPC_LCD->POL = tmp;

    // Skip line end control word - just set to 0x0
    LPC_LCD->LE = 0x00000000;

    // Default with all interrupts of
    LPC_LCD->INTMSK = 0x00000000;


    switch(cfg->bpp) {
    case Bpp_1:
        tmp = CLCDC_LCDCTRL_BPP1;
        break;
    case Bpp_2:
        tmp = CLCDC_LCDCTRL_BPP2;
        break;
    case Bpp_4:
        tmp = CLCDC_LCDCTRL_BPP4;
        break;
    case Bpp_8:
        tmp = CLCDC_LCDCTRL_BPP8;
        break;
    case Bpp_16:
        tmp = CLCDC_LCDCTRL_BPP16;
        break;
    case Bpp_24:
        tmp = CLCDC_LCDCTRL_BPP24;
        break;
    case Bpp_16_565:
        tmp = CLCDC_LCDCTRL_BPP16_565_MODE;
        break;
    case Bpp_12_444:
        tmp = CLCDC_LCDCTRL_BPP12_444_MODE;
        break;
    default:
        tmp = CLCDC_LCDCTRL_BPP16_565_MODE;
        break;
    }

    // red and blue swapped
    tmp |= CLCDC_LCDCTRL_BGR;

    switch (cfg->panelType)
    {
    case AdTft:
    case HrTft:
    case Tft:
        tmp |= CLCDC_LCDCTRL_TFT;
        break;

    case Mono_4Bit:
        tmp |= CLCDC_LCDCTRL_BW_MONO;
        break;

    case Mono_8Bit:
        tmp |= (CLCDC_LCDCTRL_MON8 | CLCDC_LCDCTRL_BW_MONO);
        break;

    case ColorStn:
        ;
        break;

    default:
        // Unsupported panel type
        break;
    }

    // Dual panel operation
    if (cfg->dualPanel)
    {
        tmp |= CLCDC_LCDCTRL_DUAL;
    }

    LPC_LCD->CTRL = tmp;

    // clear the palette (color is black )
    for (i = 0; i < sizeof(LPC_LCD->PAL)/sizeof(LPC_LCD->PAL[0]); i++)
    {
        LPC_LCD->PAL[i] = 0;
    }

    LPC_SC->LCD_CFG = 0x0;

}

void LcdController::pinConfig() {

    LPC_IOCON->P0_4 |= 7; /* LCD_VD_0 @ P0.4 */
    LPC_IOCON->P0_5 |= 7; /* LCD_VD_1 @ P0.5 */
    LPC_IOCON->P0_6 |= 7; /* LCD_VD_8 @ P0.6 */
    LPC_IOCON->P0_7 |= 7; /* LCD_VD_9 @ P0.7 */
    LPC_IOCON->P0_8 |= 7; /* LCD_VD_16 @ P0.8 */
    LPC_IOCON->P0_9 |= 7; /* LCD_VD_17 @ P0.9 */
    LPC_IOCON->P0_10 |= 7; /* LCD_VD_5 @ P0.10 */  /* LPC4088 */

#ifdef LPC4088_OEM
    LPC_IOCON->P1_20 |= 7; /* LCD_VD_10 @ P1.20 */
    LPC_IOCON->P1_23 |= 7; /* LCD_VD_13 @ P1.23 */
    LPC_IOCON->P1_24 |= 7; /* LCD_VD_14 @ P1.24 */
#else
    LPC_IOCON->P0_11 |= 7; /* LCD_VD_10 @ P0.11 */
    LPC_IOCON->P0_19 |= 7; /* LCD_VD_13 @ P0.19 */
    LPC_IOCON->P0_20 |= 7; /* LCD_VD_14 @ P0.20 */
#endif

    LPC_IOCON->P1_21 |= 7; /* LCD_VD_11 @ P1.21 */
    LPC_IOCON->P1_22 |= 7; /* LCD_VD_12 @ P1.22 */

    LPC_IOCON->P1_25 |= 7; /* LCD_VD_15 @ P1.25 */
    LPC_IOCON->P1_26 |= 7; /* LCD_VD_20 @ P1.26 */
    LPC_IOCON->P1_27 |= 7; /* LCD_VD_21 @ P1.27 */
    LPC_IOCON->P1_28 |= 7; /* LCD_VD_22 @ P1.28 */
    LPC_IOCON->P1_29 |= 7; /* LCD_VD_23 @ P1.29 */

    // pwr always controlled by gpio
//    LPC_IOCON->P2_0 |= 7; /* LCD_PWR @ P2.0 */
//    LPC_IOCON->P2_1 |= 7; /* LCD_LE  @ P2.1 */  /* Never Used */
    LPC_IOCON->P2_2 |= 7; /* LCD_DCLK @ P2.2 */
    LPC_IOCON->P2_3 |= 7; /* LCD_FP @ P2.3 */
    LPC_IOCON->P2_4 |= 7; /* LCD_ENAB_M @ P2.4 */
    LPC_IOCON->P2_5 |= 7; /* LCD_LP @ P2.5 */
    LPC_IOCON->P2_6 |= 7; /* LCD_VD_4 @ P2.6 */
    //LPC_IOCON->P2_7 |= 7; /* LCD_VD_5 @ P2.7 */  /* LPC4088 */
    LPC_IOCON->P2_8 |= 7; /* LCD_VD_6 @ P2.8 */
    LPC_IOCON->P2_9 |= 7; /* LCD_VD_7 @ P2.9 */

    LPC_IOCON->P2_11 |= 7; /* LCD_CLKIN @ P2.11 */
    LPC_IOCON->P2_12 |= 5; /* LCD_VD_3 @ P2.12 Signal marked as LCD_VD_18 on base board, but shall carry the LCD_VD_3 signal */
    LPC_IOCON->P2_13 |= 7; /* LCD_VD_19 @ P2.13 */
}

uint32_t LcdController::getClockDivisor(int clock) {
    uint32_t pixel_div, tmp = 0;
    uint32_t clk;

    clk = SystemCoreClock;

    // Find closest clock divider to get clock rate
    pixel_div = 1;
    while (((clk / pixel_div) > clock) && (pixel_div <= 0x3F))
    {
      pixel_div++;
    }

    if (pixel_div <= 1)
    {
      // Pixel clock divider is 1, skip divider logic
      tmp = CLCDC_LCDTIMING2_BCD;
    }
    else
    {
      // Add in new divider
      pixel_div -= 2;

      tmp |= (((pixel_div >> 0) & 0x1F)
             | (((pixel_div >> 5) & 0x1F) << 27));
    }

    return tmp;
}


