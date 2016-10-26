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

#include "SPIFI.h"
#include "mbed_debug.h"


/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/

#define SPIFI_DBG             0

/* 
 * The SPIFI_ROM_PTR (0x1FFF1FF8) points to an area where the pointers to
 * different drivers in ROM are stored.
 */
typedef struct {
   /*const*/ unsigned p_usbd;     // USBROMD 
   /*const*/ unsigned p_clib;
   /*const*/ unsigned p_cand;
   /*const*/ unsigned p_pwrd;     // PWRROMD
   /*const*/ unsigned p_promd;    // DIVROMD
   /*const*/ SPIFI_RTNS *pSPIFID; // SPIFIROMD
   /*const*/ unsigned p_dev3;
   /*const*/ unsigned p_dev4; 
} ROM;

#define ROM_DRIVERS_PTR ((ROM *)(*((unsigned int *)SPIFI_ROM_PTR)))
#define IS_ADDR_IN_SPIFI(__addr)  ( (((uint32_t)(__addr)) & 0xff000000) == SPIFI_MEM_BASE )

#define SPIFI_MIN(__a, __b) (((__a) < (__b)) ? (__a) : (__b))

/******************************************************************************
 * Local variables
 *****************************************************************************/

/******************************************************************************
 * Private Functions
 *****************************************************************************/

SPIFI::SpifiError SPIFI::translateError(int err, bool verify)
{  
  SpifiError res;
  
  _verError = 0;
  
  if (err == 0)
  {
    res = Ok;
  }
  else if ((err >= Uninitialized) && (err <= UnknownError))
  {
    // This is a known error code
    res = (SpifiError)err;
  }
  else if ((err >= InternalError) && (err <= EraseConflict))
  {
    // This is a known error code
    res = (SpifiError)err;
  }
  else if (verify)
  {
    // As verification was selected and err is not in the list of known
    // codes this falls into this category in the User's Manual:
    //
    // "Other non-zero values can occur if options selects verification.
    //  They will be the address in the SPIFI memory area at which the
    //  first discrepancy was found."
    _verError = err;
    res = Verification;
  }
  else
  {
    // Should never happen :-) as all listed error codes are covered but
    // to be on the safe side and not interpret this as a success, a generic
    // error is set.
    res = UnknownError;
  }
  return res;
}

/******************************************************************************
 * Public Functions
 *****************************************************************************/

SPIFI::SPIFI()
{
  _verError       = 0;
  _initialized    = false;
  _device         = UnknownDevice;
  _memorySize     = 0;
  _eraseBlockSize = 0;
  
  _romData = (SPIFIobj*)malloc(sizeof(SPIFIobj));
  if (_romData == NULL) {
    debug("SPIFI: Failed to allocate memory for ROM data\n");
  }
}

SPIFI::~SPIFI()
{
  if (_romData != NULL) {
    free(_romData);
  }
}

SPIFI::SpifiError SPIFI::init()
{
  if (!_initialized) {
    
    // Turn on SPIFI block as it is disabled on reset
    LPC_SC->PCONP |= 0x00010000;

    // pinsel for SPIFI
    LPC_IOCON->P2_7 = 5; /* SPIFI_CSN @ P2.7 */
    LPC_IOCON->P0_22 = 5; /* SPIFI_CLK @ P0.22 */
    LPC_IOCON->P0_15 = 5; /* SPIFI_IO2 @ P0.15 */
    LPC_IOCON->P0_16 = 5; /* SPIFI_IO3 @ P0.16 */
    LPC_IOCON->P0_17 = 5; /* SPIFI_IO1 @ P0.17 */
    LPC_IOCON->P0_18 = 5; /* SPIFI_IO0 @ P0.18 */
    
    uint32_t spifi_clk_div = (*((volatile uint32_t*)0x400FC1B4)) & 0x1f;
    uint32_t spifi_clk_mhz = (SystemCoreClock / spifi_clk_div) / 1000000;

    _spifi = ROM_DRIVERS_PTR->pSPIFID;

    /* Typical time tCS is 20 ns min, we give 200 ns to be on safer side */
    int rc = _spifi->spifi_init (_romData, spifi_clk_mhz/5, S_FULLCLK+S_RCVCLK, spifi_clk_mhz);
    if (rc) {
      _spifi = NULL;
      return translateError(rc);
    }

    /* Make sure it is a tested flash module */
    if ((_romData->mfger == 1) && (_romData->devType == 0x2) && (_romData->devID == 0x15) && (_romData->memSize > 0x100000)) 
    {
      _device = Spansion_S25FL032;
      _memorySize = _romData->memSize;
      _eraseBlockSize = 64*1024;
    } 
    else if ((_romData->mfger == 0xef) && (_romData->devType == 0x40) && (_romData->devID == 0x17) && (_romData->memSize > 0x100000))
    {
      _device = Winbond_W25Q64FV;
      _memorySize = _romData->memSize;
      _eraseBlockSize = 4*1024;
    } 
    else 
    {
      debug("SPIFI::init(): Memory is unknown and may not work as expected\n");
      
      // Asuming it has 64Kb erase blocks (i.e. same setup as the Spansion S25FL032
      _device = UnknownDevice;
      _memorySize = _romData->memSize;
      _eraseBlockSize = 64*1024;

      /*
       * If this happens, check the manufacturer and device information
       * and compare with the data sheet for your chip. Also make sure
       * that the sector sizes are the same (i.e. 64KB) for your chip.
       * If everything is the same then add an exception for your chip.
       */
    }
    
    _initialized = true;
  }
  return Ok;
}

SPIFI::SpifiError SPIFI::program(uint32_t dest, unsigned len, char* src, Options options, bool verify, char* scratch)
{
  unsigned written = 0;
  SPIFIopers opers;
  opers.dest = (char *)dest;
  opers.length = SPIFI_MIN(len, PROG_SIZE);
  opers.scratch = scratch;
  opers.protect = 0;
  opers.options = options;
  if (verify) {
    opers.options |= S_VERIFY_PROG;
  }
  
  if (IS_ADDR_IN_SPIFI(src))
  {
    // The SPIFI ROM driver cannot write data from SPIFI into
    // SPIFI (i.e. cannot read and write at the same time).
    // The workaround is to copy the source data into a buffer
    // in local memory and use that as source for the write
    // instead.
    while (written < len) {
      memcpy(_addrConflictBuff, src + written, opers.length);
      int rc = _spifi->spifi_program(_romData, _addrConflictBuff, &opers);
      if (rc) 
      {
        // got an error
        return translateError(rc, verify);
      }
      written += opers.length;
      opers.dest += opers.length;
      opers.length = SPIFI_MIN(len - written, PROG_SIZE);
    }
  }
  else 
  {
    while (written < len) {
      int rc = _spifi->spifi_program(_romData, src + written, &opers);
      if (rc) 
      {
        // got an error
        return translateError(rc, verify);
      }
      written += opers.length;
      opers.dest += opers.length;
      opers.length = SPIFI_MIN(len - written, PROG_SIZE);
    }
  }
  
  return Ok;
}

SPIFI::SpifiError SPIFI::erase(uint32_t dest, unsigned len, char* src, bool verify, char* scratch)
{
  SPIFIopers opers;
  opers.dest = (char *)dest;
  opers.length = len;
  opers.scratch = scratch;
  opers.protect = 0;
  if (verify) {
    opers.options = S_VERIFY_ERASE;
  } else {
    opers.options = S_NO_VERIFY;
  }
  int rc = _spifi->spifi_erase(_romData, &opers);
  return translateError(rc);
}


