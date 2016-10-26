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

#ifndef SPIFI_H
#define SPIFI_H

#include "mbed.h"
#include "spifi_rom_api.h"

/**
 * SPIFI Example
 *
 * @code
 * #include "mbed.h"
 * #include "SPIFI.h"
 *
 * int main(void) {
 *    SPIFI::SpifiError err;
 *
 *    err = SPIFI::instance().init();
 *    if (err != SPIFI::Ok) {
 *        printf("Failed to initialize SPIFI, error %d\n", err);
 *    }
 *    
 *    // Write "Hello World!" into the first bytes of the SPIFI
 *    char buff[20] = "Hello World!";
 *    err = SPIFI::instance().program(0, strlen(buff)+1, buff, SPIFI::EraseAsRequired);
 *    if (err != SPIFI::Ok) {
 *        printf("Failed to write to SPIFI, error %d\n", err);
 *    }
 *
 *    // Now verify that it can be read
 *    if (memcmp((char*)SPIFI::SpifiMemBase, buff, strlen(buff)+1) == 0) {
 *        printf("Readback from memory OK: '%s'\n", SPIFI::SpifiMemBase);
 *    } else {
 *        printf("Spifi does not contain the correct data!\n");
 *    }
 * }
 * @endcode
 */
class SPIFI {
public:
  
    enum Constants {
        SpifiMemBase = 0x28000000
    };

    enum Options {
        ForceErase      = (0<<2),
        EraseAsRequired = (1<<2),
        CallerErase     = (1<<3)
    };

    enum SpifiError {
        Ok            =       0,
        Uninitialized =       1,
        Verification  =       2,
        SameAddress   =       3,
        UnknownError  =       4,
        InternalError = 0x20002,
        Timeout       = 0x20003,
        OperandError  = 0x20004,
        Status        = 0x20005,
        ExtDeviceId   = 0x20006,
        DeviceId      = 0x20007,
        DeviceType    = 0x20008,
        Manufacturer  = 0x20009,
        InvalidJDECId = 0x2000A,
        EraseConflict = 0x2000B,
    };
    
    enum Device {
        Spansion_S25FL032, /* Manufacturer: 0x01, devType: 0x02, devID: 0x15 */
        Winbond_W25Q64FV,  /* Manufacturer: 0xEF, devType: 0x40, devID: 0x17 */
        UnknownDevice
    };
    
    static SPIFI& instance()
    {
        static SPIFI singleton;
        return singleton;
    }
  

    /** Initializes the SPIFI ROM driver making the content of the external serial flash available
     *
     *  @returns
     *       Ok on success
     *       An error code on failure
     */
    SpifiError init();
  
    /** Returns the detected external serial flash
     *
     *  @returns
     *       The detected device or UnknownDevice
     */
    Device device() { return _device; }
  
    /** Returns the size (in bytes) of the external serial flash
     *
     *  @returns
     *       The size in bytes
     */
    uint32_t memorySize() { return _memorySize; }
  
    /** Returns the size of an erase block (in bytes) on the external serial flash
     *
     *  @returns
     *       The erase block size in bytes
     */
    uint32_t eraseBlockSize() { return _eraseBlockSize; }
  
    /** Returns the address where the verifcation failed. Use only after a Verification error code 
     * has been retured from one of the other functions
     *
     *  @returns
     *       The address to where the program or erase operation failed
     */
    uint32_t getVerificationErrorAddr() { return _verError; }
    
    /** Used by the QSPIFileSystem class to get access to all spifi internal data.
     *
     *  @param initData  The parameter returned by spifi_init
     *  @param romPtr    The location of the SPIFI ROM driver in memory
     */
    void internalData(SPIFIobj** initData, const SPIFI_RTNS** romPtr) { *initData = _romData; *romPtr = _spifi; }
  
    /** Copies len data from src to dest. This function will split len > 256 into writes of 256 bytes each.
     *
     * Programming means that a bit can either be left at 0, or programmed from 1 to 0. Changing bits from 0 to 1 requires an erase operation.
     * While bits can be individually programmed from 1 to 0, erasing bits from 0 to 1 must be done in larger chunks (manufacturer dependant 
     * but typically 4Kbyte to 64KByte at a time).
     *
     * Specify how/when erasing is done with the options parameter:
     *
     *    - ForceErase causes len bytes to be erased starting at dest. If a scratch buffer (must be the size of an erase block) is
     *      provided then only the len bytes will be erased. If no scratch buffer is provided then the erase block surrounding
     *      dest will be erased. 
     * 
     *    - EraseAsRequired tests if the destination can be written to without erasing. If it can then no erasing is done. If it 
     *      can't then behaviour is the same as if ForceErase was specified. 
     * 
     *    - CallerErase tests if the destination can be written to without erasing. If it can then a data is written. If it 
     *      can't then an error code is returned and nothing is written. 
     *
     * Scratch should be NULL or the address of an area of RAM that the SPIFI driver can use 
     * to save data during erase operations. If provided, the scratch area should be as large 
     * as the smallest erase size that is available throughout the serial flash device. If scratch 
     * is NULL (zero) and an erase is necessary, any bytes in the first erase block before dest 
     * are left in erased state (all ones), as are any bytes in the last erase block after dest + len
     *
     *  @param dest      Address to write to, highest byte must be 0x00 or 0x28 as in 0x28000000
     *  @param len       Number of bytes to write
     *  @param src       Data to write
     *  @param options   How to handle content of destination
     *  @param verify    Should all writes be verified or not
     *  @param scratch   Used with ForceErase and EraseAsRequired option to preserve content of flash outside of
     *                   written area. Size of buffer must be at least the size of one erase block.
     *
     *  @returns
     *       Ok on success
     *       An error code on failure
     */
    SpifiError program(uint32_t dest, unsigned len, char* src, Options options, bool verify=false, char* scratch=NULL);
  
    /** Erases the content of the memory at the specified address
     *
     * Scratch should be NULL or the address of an area of RAM that the SPIFI driver can use 
     * to save data during erase operations. If provided, the scratch area should be as large 
     * as the smallest erase size that is available throughout the serial flash device. If scratch 
     * is NULL (zero) and an erase is necessary, any bytes in the first erase block before dest 
     * are left in erased state (all ones), as are any bytes in the last erase block after dest + len
     *
     *  @param dest      Address to start erasing at, highest byte must be 0x00 or 0x28 as in 0x28000000
     *  @param len       Number of bytes to erase
     *  @param verify    Should the erased area be verified to make sure it is all 0xff
     *  @param scratch   Used to preserve content of flash outside of erased area. 
     *                   Size of buffer must be at least the size of one erase block.
     *
     *  @returns
     *       Ok on success
     *       An error code on failure
     */
    SpifiError erase(uint32_t dest, unsigned len, char* src, bool verify=false, char* scratch=NULL);
  

private:

    uint32_t _verError;
    
    bool _initialized;
    
    Device _device;
    uint32_t _memorySize;
    uint32_t _eraseBlockSize;

    SPIFIobj* _romData;
    const SPIFI_RTNS* _spifi;

    char _addrConflictBuff[256];

    explicit SPIFI();
    // hide copy constructor
    SPIFI(const SPIFI&);
    // hide assign operator
    SPIFI& operator=(const SPIFI&);
    ~SPIFI();

    SpifiError translateError(int err, bool verify = false);
};

#endif

