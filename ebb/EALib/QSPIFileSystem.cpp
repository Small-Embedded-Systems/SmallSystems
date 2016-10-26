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

#include "QSPIFileSystem.h"
#include "mbed_debug.h"

#include "SPIFI.h"

/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/

#define QSPI_DBG             0

#define IS_ADDR_IN_SPIFI(__addr)  ( (((uint32_t)(__addr)) & 0xff000000) == SPIFI_MEM_BASE )

#define MEM_SIZE    (memInfo.memSize)
#define ERASE_SIZE  (memInfo.eraseBlockSize)
#define NUM_BLOCKS  (memInfo.numEraseBlocks)

typedef uint32_t toc_entry_t;

#define TOC_BLOCK_ADDR   (memInfo.tocBlockAddr)   //(SPIFI_MEM_BASE + (NUM_BLOCKS - 1)*ERASE_SIZE)
#define TOC_SIZE         (memInfo.tocSizeInBytes) //(sizeof(toc_entry_t) * NUM_BLOCKS)
#define NUM_TOCS         (memInfo.numTocs)        //((int)(ERASE_SIZE/TOC_SIZE))
#define NUM_TOC_BLOCKS   ((NUM_TOCS * TOC_SIZE) / ERASE_SIZE)
#define NUM_TOC_ENTRIES  ((int)(TOC_SIZE/sizeof(toc_entry_t)))

#define TOC_UNUSED          (0xffffffff)
#define TOC_MAX             (NUM_BLOCKS - 1)
#define TOC_VALID_MASK      (1UL<<31)
#define TOC_RESERVED_MASK   (1UL<<30)
#define TOC_USED_MASK       (1UL<<29)
#define TOC_FILE_MASK       (1UL<<28)
#define TOC_FSIZE_MASK      (0x3ffff)
#define TOC_MANDAT_SET_MASK (0x0ffc0000)

#define MANDATORY_BITS_SET(__v)  (((__v)&TOC_MANDAT_SET_MASK) == TOC_MANDAT_SET_MASK)

#define VALID_TOC_ENTRY(__v)  (((__v)&TOC_VALID_MASK) == 0)
#define USED_TOC_ENTRY(__v)   (VALID_TOC_ENTRY(__v) && (((__v)&TOC_USED_MASK) == 0))
#define TOC_IS_FILE(__v)      (USED_TOC_ENTRY(__v) && (((__v)&TOC_FILE_MASK) == 0))
#define TOC_IS_RESERVED(__v)  (VALID_TOC_ENTRY(__v) && (((__v)&TOC_RESERVED_MASK) == 0))
#define FILESIZE(__v)         ((__v) & 0x3ffff)

#define FS_MIN(__a, __b)  (((__a) < (__b)) ? (__a) : (__b))

// Mask to compare the different access modes. In LPCXpresso this was defined
// but not in uVision
#ifndef O_ACCMODE
#define O_ACCMODE  (O_RDONLY | O_WRONLY | O_RDWR)
#endif


/*
 * The file header currently only consists of the filename (including path)
 * and the string terminating character, but by separating the file name
 * length from the size of the header in the code it allows future additions
 * to the header without too much code modification.
 */
#define HEADER_DNAME_MAXLEN  (250)
#define HEADER_FNAME_STRLEN  (HEADER_DNAME_MAXLEN + 5)
#define HEADER_FNAME_LEN     (HEADER_FNAME_STRLEN + 1)
#define HEADER_LEN           (HEADER_FNAME_LEN) // only filename in header for now

typedef enum
{
  FS_OK,
  FS_ERR_NOT_FORMATTED,
  FS_ERR_NO_FILE,
  FS_ERR_FILE_EXIST,
  FS_ERR_INVALID_PARAM,
  FS_ERR_DISK_FULL,
  FS_ERR_SPIFI,
  FS_ERR_MALLOC, 

  // FS_ERR_SPIFI_* return codes are listed in the User's Manual
  // as possible return values from spifi_init(), spifi_program()
  // and spifi_erase() calls.
  FS_ERR_SPIFI_INTERNAL_ERROR  = 0x20002,  // 0x20002, Internal error in API code
  FS_ERR_SPIFI_TIMEOUT         = 0x20003,  // 0x20003, Time-out waiting for program or erase to begin: protection could not be removed.
  FS_ERR_SPIFI_OPERAND         = 0x20004,  // 0x20004, Operand error (i.e. invalid params)
  FS_ERR_SPIFI_STATUS          = 0x20005,  // 0x20005, Device status error
  FS_ERR_SPIFI_EXT_DEVICE_ID   = 0x20006,  // 0x20006, Unknown extended device ID value
  FS_ERR_SPIFI_DEVICE_ID       = 0x20007,  // 0x20007, Unknown device ID code
  FS_ERR_SPIFI_DEVICE_TYPE     = 0x20008,  // 0x20008, Unknown device type code
  FS_ERR_SPIFI_MANUFACTURER    = 0x20009,  // 0x20009, Unknown manufacturer code
  FS_ERR_SPIFI_INVALID_JDEC_ID = 0x2000A,  // 0x2000A, No operative serial flash (JEDEC ID all zeroes or all ones)
  FS_ERR_SPIFI_ERASE_CONFLICT  = 0x2000B,  // 0x2000B, S_CALLER_ERASE is included in options, and erasure is required.
  FS_ERR_SPIFI_VERIFICATION,               // other,   Other non-zero values can occur if options selects verification.
                                           //          They will be the address in the SPIFI memory area at which the first discrepancy was found.
} fresult;

// The number of times to re-attempt a spifi_program() or spifi_erase()
// if the last one reported a verification error.
#define NUM_VERIFICATION_ATTEMPTS  (1)

typedef struct
{
  uint32_t memSize;
  uint32_t eraseBlockSize;
  uint32_t numEraseBlocks;
  uint32_t tocBlockAddr;
  uint32_t numTocs;
  uint32_t tocSizeInBytes;
  char memName[30];
} meminfo_t;

typedef struct
{
  int      tocIdx;
  uint32_t size;
  uint16_t lastBlock;
} fileHandle_t;

/******************************************************************************
 * Local variables
 *****************************************************************************/

static toc_entry_t* TOC = NULL;
static int activeTOC = -1;

static const SPIFI_RTNS *spifi = NULL;
static SPIFIobj* obj;
static SPIFIopers opers;

static char addr_conflict_buff[PROG_SIZE];

static meminfo_t memInfo = {0,0,0,0,0,0,{0}};

/******************************************************************************
 * Forward Declarations of Local Functions
 *****************************************************************************/
static fresult qspifs_init();
static fresult qspifs_translateSpifiError(int rc);
static fresult qspifs_readTOC(void);
static fresult qspifs_saveTOC(void);
static fresult qspifs_findFile(const char* filename, fileHandle_t* fh);
static fresult qspifs_fileSize(int tocIdx, uint32_t* pSize);
static fresult qspifs_eraseBlock(int block);
static fresult qspifs_allocateFile(const char* filename, int neededBlocks, int* pTocIdx);
static void qspifs_deleteFile(fileHandle_t* fh);
static fresult qspifs_allocateSpace(fileHandle_t* fh, uint32_t size);
static fresult qspifs_format(unsigned int minReservedBytes);
static fresult qspifs_write(fileHandle_t* fh, const uint8_t * const pData, uint32_t size);
static bool qspifs_startsWith(const char* prefix, const char* str);

/******************************************************************************
 * Local Functions
 *****************************************************************************/

/******************************************************************************
 *
 * Description:
 *    Initializes spifi, identifies the chip and reads the file system's
 *    table of content.
 *
 * Params:
 *    None
 *
 * Returns:
 *    FS_OK or one of the FS_ERR_* error codes
 *
 *****************************************************************************/
static fresult qspifs_init()
{
  if (spifi == NULL) {
    SPIFI::SpifiError err;
    err = SPIFI::instance().init();
    if (err != SPIFI::Ok) {
      spifi = NULL;
      return FS_ERR_SPIFI;
    }
    
    SPIFI::instance().internalData(&obj, &spifi);

    /* Make sure it is a tested flash module */
    switch (SPIFI::instance().device()) {
      case SPIFI::Spansion_S25FL032:
        /* For the Spansion memory the TOC occupies 256bytes and the TOC block will
           hold 256 TOCs. */
        strcpy(memInfo.memName, "Spansion S25FL032");
        memInfo.memSize        = obj->memSize;
        memInfo.eraseBlockSize = 64*1024;
        memInfo.numEraseBlocks = memInfo.memSize / memInfo.eraseBlockSize;
        memInfo.tocSizeInBytes = sizeof(toc_entry_t) * memInfo.numEraseBlocks;
        memInfo.numTocs        = memInfo.eraseBlockSize / memInfo.tocSizeInBytes;
        memInfo.tocBlockAddr   = SPIFI_MEM_BASE + (NUM_BLOCKS * ERASE_SIZE) - (memInfo.numTocs * memInfo.tocSizeInBytes);
        break;
      
      case SPIFI::Winbond_W25Q64FV:
        /* For the Winbond memory the TOC occupies 8192 bytes and that is bigger than 
           one erase block (which is 4096 bytes). It is possible to either keep only
           one TOC or to create a couple to reduce wear on the memory. In this case 
           the multiple TOCs option is used. */
        strcpy(memInfo.memName, "Winbond W25Q64FV");
        memInfo.memSize        = obj->memSize;
        memInfo.eraseBlockSize = 4*1024;
        memInfo.numEraseBlocks = memInfo.memSize / memInfo.eraseBlockSize;
        memInfo.tocSizeInBytes = sizeof(toc_entry_t) * memInfo.numEraseBlocks;
        memInfo.numTocs        = 8;
        memInfo.tocBlockAddr   = SPIFI_MEM_BASE + (NUM_BLOCKS * ERASE_SIZE) - (memInfo.numTocs * memInfo.tocSizeInBytes);      
        break;
      
      case SPIFI::UnknownDevice:
      default:
        debug("INIT: Memory is unknown and may not work as expected\n");
        
        // Asuming it has 64Kb erase blocks (i.e. same setup as the Spansion S25FL032
        strcpy(memInfo.memName, "Unknown - check ID");
        memInfo.memSize        = obj->memSize;
        memInfo.eraseBlockSize = 64*1024;
        memInfo.numEraseBlocks = memInfo.memSize / memInfo.eraseBlockSize;      
        memInfo.tocSizeInBytes = sizeof(toc_entry_t) * memInfo.numEraseBlocks;
        memInfo.numTocs        = memInfo.eraseBlockSize / memInfo.tocSizeInBytes;
        memInfo.tocBlockAddr   = SPIFI_MEM_BASE + (NUM_BLOCKS * ERASE_SIZE) - (memInfo.numTocs * memInfo.tocSizeInBytes);      

        /*
         * If this happens, check the manufacturer and device information
         * and compare with the data sheet for your chip. Also make sure
         * that the sector sizes are the same (i.e. 64KB) for your chip.
         * If everything is the same then add an exception for your chip.
         */
        break;
    }
      
    debug_if(QSPI_DBG, "INIT: Found %dMB %s\n", memInfo.memSize/0x100000, memInfo.memName);
    
    if (TOC != NULL) {
      delete TOC;
    }
    TOC = (toc_entry_t*)malloc(TOC_SIZE);
    if (TOC == NULL) {
      debug_if(QSPI_DBG, "INIT: Failed to allocate memory for TOC\n");
      spifi = NULL;
      return FS_ERR_MALLOC;
    }
  }
  if (activeTOC == -1)
  {
    return qspifs_readTOC();
  }
  return FS_OK;
}

/******************************************************************************
 *
 * Description:
 *    Converts the return value from one of the spifi_init(), spifi_program()
 *    or spifi_erase() calls into a FS_* error code to simplify it for the
 *    fs_qspi API user.
 *    This function also attempts to detect the verification failure error.
 *    When a verification error occurs the spifi_* functions returns the
 *    conflicting address and not an error code. As this can be any address
 *    it is difficult to test but this function converts it into the
 *    FS_ERR_SPIFI_VERIFICATION error code which can be tested against.
 *
 * Params:
 *    [in] rc - The return code from any of the spifi_* functions
 *
 * Returns:
 *    FS_OK or one of the FS_ERR_* error codes
 *
 *****************************************************************************/
static fresult qspifs_translateSpifiError(int rc)
{
  fresult res;
  if (rc == 0)
  {
    res = FS_OK;
  }
  else if ((rc >= FS_ERR_SPIFI_INTERNAL_ERROR) && (rc <= FS_ERR_SPIFI_ERASE_CONFLICT))
  {
    // This is a known error code
    res = (fresult)rc;
  }
  else if (opers.options & (S_VERIFY_PROG | S_VERIFY_ERASE))
  {
    // As verification was selected and rc is not in the list of known
    // codes this falls into this category in the User's Manual:
    //
    // "Other non-zero values can occur if options selects verification.
    //  They will be the address in the SPIFI memory area at which the
    //  first discrepancy was found."
    res = FS_ERR_SPIFI_VERIFICATION;
  }
  else
  {
    // Should never happen :-) as all listed error codes are covered but
    // to be on the safe side and not interpret this as a success, a generic
    // error is set.
    res = FS_ERR_SPIFI;
  }
  return res;
}

/******************************************************************************
 *
 * Description:
 *    Reads the table of contents (TOC). The TOC is stored in the last erase
 *    block on the QSPI flash. As the QSPI flash is not exactly RW (might
 *    require erasing before writing) the TOC is relocated inside the erase
 *    block everytime it is saved (see saveTOC()). The currently valid TOC 
 *    is allways the last one stored.
 *
 * Params:
 *    None
 *
 * Returns:
 *    FS_OK or one of the FS_ERR_* error codes
 *
 *****************************************************************************/
static fresult qspifs_readTOC(void)
{
  int i, j;
  toc_entry_t* p;
  uint8_t invalid = 0;
  int lastValid = -1;

  // Search for the first unused TOC, keeping track of the valid
  // ones as we go.
  for (i = 0; (i < NUM_TOCS) && !invalid; i++)
  {
    p = (toc_entry_t*)(TOC_BLOCK_ADDR + i*TOC_SIZE);
    for (j = 0; j < NUM_BLOCKS; j++)
    {
      if (!VALID_TOC_ENTRY(*p) || !MANDATORY_BITS_SET(*p))
      {
        // invalid TOC entry, stop looking
        invalid = 1;
        break;
      }
      p++;
    }

    if (!invalid)
    {
      // this TOC was ok, but perhaps there is a newer one?
      lastValid = i;
    }
  }

  if (lastValid == -1)
  {
    // no valid TOCs on the flash
    return FS_ERR_NOT_FORMATTED;
  }
  else
  {
    // previous entry was ok so use that
    activeTOC = lastValid;
    p = (toc_entry_t*)(TOC_BLOCK_ADDR + activeTOC*TOC_SIZE);
    memcpy(TOC, p, TOC_SIZE);
    return FS_OK;
  }
}

/******************************************************************************
 *
 * Description:
 *    Saves the table of contents (TOC). The TOC is stored in the last erase
 *    block on the QSPI flash. As the QSPI flash is not exactly RW (might
 *    require erasing before writing) the TOC is first compared with what is
 *    stored in the QSPI flash and if there are no changes or all changes
 *    only require bit changes 1->0 then the current TOC can be overwritten.
 *    If bit value changes 0->1 are required then the current stored TOC
 *    cannot be overwritten and the new TOC is instead stored in the next
 *    available space. If the entire last block is filled then it is erased
 *    and the new TOC is placed at the start of it.
 *
 * Params:
 *    None
 *
 * Returns:
 *    FS_OK or one of the FS_ERR_* error codes
 *
 *****************************************************************************/
static fresult qspifs_saveTOC(void)
{
  int i, rc = 0;
  uint32_t* pSrc;
  uint32_t* pDest;
  uint32_t tmp;
  uint8_t identical = 1;

  // active TOC same as the one we want to save?
  pSrc = (uint32_t*)TOC;
  pDest = (uint32_t*)(TOC_BLOCK_ADDR + activeTOC*TOC_SIZE);
  for (i = 0; i < NUM_TOC_ENTRIES; i++)
  {
    if (*pSrc != *pDest)
    {
      identical = 0;
      tmp = ((*pDest) ^ (*pSrc)) & (*pSrc);
      if (tmp > 0)
      {
        // found a change that contains 0->1 bit modification which
        // requires erasing or a new location
        activeTOC = (activeTOC + 1)%NUM_TOCS;
        if (activeTOC == 0)
        {
          // no more free TOCs so an erase is needed          
#if 0          
          opers.options &= ~S_CALLER_ERASE;
          opers.options |= S_FORCE_ERASE;
#else
          opers.dest = (char *) TOC_BLOCK_ADDR;
          opers.length = TOC_SIZE * NUM_TOCS;
          opers.scratch = NULL;
          opers.protect = 0;
          opers.options = S_NO_VERIFY;
          rc = spifi->spifi_erase(obj, &opers);
          if (rc) {
            return qspifs_translateSpifiError(rc);
          }
#endif          
        }
        break;
      }
    }
    pSrc++;
    pDest++;
  }

  if (!identical)
  {
    opers.length = FS_MIN(TOC_SIZE, PROG_SIZE);
    opers.scratch = NULL;
    opers.protect = 0;
    opers.options = S_VERIFY_PROG | S_CALLER_ERASE;
    for (int i = 0; i < (TOC_SIZE / PROG_SIZE); i++) 
    {
      opers.dest = (char *)(TOC_BLOCK_ADDR + activeTOC*TOC_SIZE + i*PROG_SIZE);
      rc = spifi->spifi_program(obj, ((char*)TOC)+i*PROG_SIZE, &opers);
      if (rc) 
      {
        break;
      }
    }
    return qspifs_translateSpifiError(rc);
  }
  return FS_OK;
}

/******************************************************************************
 *
 * Description:
 *    Searches the file system for a file with the specified name and
 *    (if found) returns the file's position in the TOC.
 *
 *    Note that the content of fh is only valid if FS_OK is returned.
 *
 * Params:
 *    [in] filename - The name of the file to find
 *    [out] fh      - The handle with the file information
 *
 * Returns:
 *    FS_OK or one of the FS_ERR_* error codes
 *
 *****************************************************************************/
static fresult qspifs_findFile(const char* filename, fileHandle_t* fh)
{
  int i;

  if (activeTOC == -1)
  {
    return FS_ERR_NOT_FORMATTED;
  }

  // Look at all blocks except for the reserved ones
  for (i = 0; i < NUM_BLOCKS; i++)
  {
    if (TOC_IS_FILE(TOC[i]) && !TOC_IS_RESERVED(TOC[i]))
    {
      // found a file, see if name matches
      char* p = (char*)(SPIFI_MEM_BASE + i*ERASE_SIZE);
      if (strncmp(filename, p, HEADER_FNAME_LEN) == 0)
      {
        // found a matching name
        fh->tocIdx = i;
        fresult res = qspifs_fileSize(fh->tocIdx, &fh->size);
        if (res == FS_OK) {
            fh->lastBlock = fh->tocIdx + ((fh->size + HEADER_LEN)/ ERASE_SIZE);
        }
        return FS_OK;
      }
    }
  }
  return FS_ERR_NO_FILE;
}

/******************************************************************************
 *
 * Description:
 *    Calculates and returns the file's size.
 *
 *    Note that the content of pSize is only valid if FS_OK is returned.
 *
 * Params:
 *    [in] tocIdx - The file's position in the TOC
 *    [out] pSize - The file's size
 *
 * Returns:
 *    FS_OK or one of the FS_ERR_* error codes
 *
 *****************************************************************************/
static fresult qspifs_fileSize(int tocIdx, uint32_t* pSize)
{
  int i;

  if (tocIdx < 0 || tocIdx > NUM_BLOCKS || !TOC_IS_FILE(TOC[tocIdx]))
  {
    return FS_ERR_NO_FILE;
  }

  *pSize = 0;

  // A file is always stored in sequential blocks so start with the files
  // first block and as long as it is full continue sum up the occupied
  // block sizes. As soon as a non-full block is found that must be the
  // file's last block.
  for (i = tocIdx; i < NUM_BLOCKS; i++)
  {
    *pSize += FILESIZE(TOC[i]);
    if (FILESIZE(TOC[i]) < ERASE_SIZE)
    {
      // last block in chain
      break;
    }
  }

  // Remove the filename header from the file's size
  *pSize -= HEADER_LEN;

  return FS_OK;
}

/******************************************************************************
 *
 * Description:
 *    Erases everything in one block on the QSPI flash.
 *
 * Params:
 *    [in] block - The block's number
 *
 * Returns:
 *    FS_OK or one of the FS_ERR_* error codes
 *
 *****************************************************************************/
static fresult qspifs_eraseBlock(int block)
{
  opers.dest = (char *)(block * ERASE_SIZE);
  opers.length = ERASE_SIZE;
  opers.scratch = NULL;
  opers.protect = 0;
  opers.options = S_NO_VERIFY;
  return qspifs_translateSpifiError(spifi->spifi_erase (obj, &opers));
}

/******************************************************************************
 *
 * Description:
 *    Creates a new file if there is enough space for it on the file system.
 *    The TOC is searched for a unused sequence of blocks of at least the
 *    needed size. That block is marked as used and the file's name is stored
 *    in the first bytes of the file's first block.
 *
 *    Note: The filename will not be tested for uniqueness.
 *    Note: The value of pTocIdx will only be valid if FS_OK is returned.
 *
 * Params:
 *    [in] filename - The name of the new file
 *    [in] neededBlocks - The number of blocks (in sequence) to allocate
 *    [out] pTocIdx - The new file's position in the TOC
 *
 * Returns:
 *    FS_OK or one of the FS_ERR_* error codes
 *
 *****************************************************************************/
static fresult qspifs_allocateFile(const char* filename, int neededBlocks, int* pTocIdx)
{
  int i, rc;

  if (activeTOC == -1)
  {
    return FS_ERR_NOT_FORMATTED;
  }

  // Look at all blocks except for the reserved ones
  for (i = 0; i < NUM_BLOCKS; i++)
  {
    //TODO: Improve search to use gaps to avoid having to move files
    //      that are written to
    if (!USED_TOC_ENTRY(TOC[i]) && !TOC_IS_RESERVED(TOC[i]))
    {
      int j;
      for (j = 1; j < neededBlocks; j++)
      {
        if (USED_TOC_ENTRY(TOC[i+j]) || TOC_IS_RESERVED(TOC[i+j]))
        {
          // not enough free blocks in sequence, skip past these
          // tested entries and continue searching
          i += j;
          break;
        }
      }

      if (j == neededBlocks)
      {
        const char* pSrc = filename;
        if (IS_ADDR_IN_SPIFI(filename))
        {
          // The SPIFI ROM driver cannot write data from SPIFI into
          // SPIFI (i.e. cannot read and write at the same time).
          // The workaround is to copy the source data into a buffer
          // in local memory and use that as source for the write
          // instead.
          memcpy(addr_conflict_buff, filename, strlen(filename)+1);
          pSrc = addr_conflict_buff;
        }

        // Erase the new file's first block and store the filename at the
        // start of it
        opers.length = strlen(pSrc)+1;
        opers.scratch = NULL;
        opers.protect = 0;
        opers.options = S_VERIFY_PROG | S_FORCE_ERASE;// S_CALLER_ERASE;
        opers.dest = (char *)(i*ERASE_SIZE);
        rc = spifi->spifi_program(obj, (char*)pSrc, &opers);
        if (rc) {
          return qspifs_translateSpifiError(rc);
        }

        TOC[i] &= ~(TOC_VALID_MASK | TOC_USED_MASK | TOC_FILE_MASK | TOC_FSIZE_MASK);
        TOC[i] |= HEADER_LEN;

        *pTocIdx = i;
        return FS_OK;
      }
    }
  }
  return FS_ERR_DISK_FULL;
}

/******************************************************************************
 *
 * Description:
 *    Deletes the specified file by marking all its blocks as unused in
 *    the TOC.
 *
 *    Note: The deleted blocks are not erased here - that is done when they
 *          are allocated the next time.
 *
 * Params:
 *    [in] fh - The file handle with information about what to delete
 *
 * Returns:
 *    None
 *
 *****************************************************************************/
static void qspifs_deleteFile(fileHandle_t* fh)
{
  int i;

  for (i = fh->lastBlock; i >= fh->tocIdx; i--)
  {
    TOC[i] = ~TOC_VALID_MASK;
  }
}

/******************************************************************************
 *
 * Description:
 *    Ensures that the specified file can grow to the wanted size.
 *    If the file size will increase enough to need one or more new blocks
 *    and there isn't enough space then an attempt is made to move the
 *    current file to a large enough space somewhere else.
 *
 *    If there are more free block(s) at the end of the file then it is not
 *    moved and instead those blocks are marked as used.
 *
 *    Note: The filename will not be tested for uniqueness.
 *    Note: The value of pTocIdx will only be valid if FS_OK is returned.
 *
 * Params:
 *    [in/out] fh - The current file handle, might be updated after a move
 *    [in] size   - The wanted new size
 *
 * Returns:
 *    FS_OK or one of the FS_ERR_* error codes
 *
 *****************************************************************************/
static fresult qspifs_allocateSpace(fileHandle_t* fh, uint32_t size)
{
  uint16_t oldNumBlocks = (fh->size + HEADER_LEN) / ERASE_SIZE;
  uint16_t newNumBlocks = (fh->size + HEADER_LEN + size) / ERASE_SIZE;
  uint16_t numNeeded = newNumBlocks - oldNumBlocks;
  fresult res = FS_OK;

  if (numNeeded > 0)
  {
    uint16_t i;
    for (i = 0; i < numNeeded; i++)
    {
      if (USED_TOC_ENTRY(TOC[fh->tocIdx + oldNumBlocks + 1 + i]) || 
          TOC_IS_RESERVED(TOC[fh->tocIdx + oldNumBlocks + 1 + i]))
      {
        fileHandle_t fhNew;

        // have to move the chain
        char* filename = (char*)(SPIFI_MEM_BASE + fh->tocIdx * ERASE_SIZE);
        res = qspifs_allocateFile(filename, newNumBlocks, &(fhNew.tocIdx));
        if (res == FS_OK)
        {
          // copy data
          fhNew.lastBlock = fhNew.tocIdx;
          fhNew.size = 0;
          res = qspifs_write(&fhNew, (uint8_t*)(SPIFI_MEM_BASE + fh->tocIdx * ERASE_SIZE + HEADER_LEN), fh->size);
        }
        if (res == FS_OK)
        {
          // remove old entries
            qspifs_deleteFile(fh);

          // modify old handle to point to new information
          fh->lastBlock = fhNew.lastBlock;
          fh->size = fhNew.size;
          fh->tocIdx = fhNew.tocIdx;
        }
        if (res != FS_OK)
        {
          // not possible to relocate the file => abort
          return res;
        }
        break;
      }
    }

    // have space that is unused, so mark as used
    for (i = 0; i < numNeeded; i++)
    {
      int tocIdx = fh->tocIdx + oldNumBlocks + 1 + i;
      TOC[tocIdx] &= ~TOC_USED_MASK;
      qspifs_eraseBlock(tocIdx);
    }
  }

  return res;
}

/******************************************************************************
 *
 * Description:
 *    Adds a file system to the QSPI flash. The entire flash will be erase 
 *    except for the minReservedBytes first bytes. That reserved area (rounded
 *    up to the closest even multiple of the erase block size) can be used 
 *    for anything and will never be touched by the file system. That area is
 *    typically used for executing programs from when the internal flash is
 *    full.
 *
 *    The file system will have a table of content (TOC) placed at the start
 *    of the last erase block on the flash.
 *
 * Params:
 *    [in] minReservedBytes  - The number of bytes to ignore at the start of
 *                             the flash.
 *
 * Returns:
 *    FS_OK on success or one of the FS_ERR_* on failure
 *
 *****************************************************************************/
static fresult qspifs_format(unsigned int minReservedBytes)
{
  int i, rc;
  int numReserved = 0;
  
  if (minReservedBytes > 0) {
    numReserved = (minReservedBytes + ERASE_SIZE - 1) / ERASE_SIZE;
    if (numReserved >= (NUM_BLOCKS - 2)) {
      // Too many of the erase blocks are reserved - not even room for one file
      return FS_ERR_INVALID_PARAM;
    }
  }

#if 0   // works but is really slow  
  // Erase all non-reserved blocks
  for (i = numReserved; i < NUM_BLOCKS; i++) {
    opers.dest = (char *) (i * ERASE_SIZE);
    opers.length = ERASE_SIZE;
    opers.scratch = NULL;
    opers.protect = 0;
    opers.options = S_NO_VERIFY;
    rc = spifi->spifi_erase(&obj, &opers);
    if (rc) {
      return qspifs_translateSpifiError(rc);
    }
  }
#else
  // Erase all non-reserved blocks
  opers.dest = (char *) (numReserved * ERASE_SIZE);
  opers.length = MEM_SIZE - (numReserved * ERASE_SIZE);
  opers.scratch = NULL;
  opers.protect = 0;
  opers.options = S_NO_VERIFY;
  rc = spifi->spifi_erase(obj, &opers);
  if (rc) {
    return qspifs_translateSpifiError(rc);
  }
#endif  

  // Create the TOC, mark requested blocks as reserved and mark the TOC's
  // block(s) as reserved as well.
  for (i = 0; i < numReserved; i++) {
    TOC[i] = ~(TOC_VALID_MASK | TOC_RESERVED_MASK);
  }
  for (; i < (NUM_BLOCKS - NUM_TOC_BLOCKS); i++) {
    TOC[i] = ~TOC_VALID_MASK;
  }
  for (; i < NUM_BLOCKS; i++) {
    TOC[i] = ~(TOC_VALID_MASK | TOC_RESERVED_MASK);
  }
  
  // Save the TOC in the last block
  activeTOC = 0;
  fresult res = qspifs_saveTOC();
  if (res != FS_OK) {
    activeTOC = -1;
    return res;
  }
//   opers.dest = (char *) TOC_BLOCK_ADDR;
//   opers.length = TOC_SIZE;
//   opers.scratch = NULL;
//   opers.protect = 0;
//   opers.options = S_VERIFY_PROG | S_CALLER_ERASE;
//   rc = spifi->spifi_program(&obj, (char*) TOC, &opers);
//   if (rc) {
//     return qspifs_translateSpifiError(rc);
//   }

  // Read back TOC to be sure it worked
  return qspifs_readTOC();
}

/******************************************************************************
 *
 * Description:
 *    Deletes all files on the file system. This is a "quick format" that
 *    leaves all blocks untouched and only modifies the TOC. Any reserved
 *    blocks are kept reserved.
 *
 *    The purpose of this function is to make it easy to clear the file system
 *    without going through a time consuming complete erase every time.
 *
 * Params:
 *    None
 *
 * Returns:
 *    FS_OK on success or one of the FS_ERR_* on failure
 *
 *****************************************************************************/
// static fresult qspifs_deleteAllFiles(void)
// {
//   for (int i = 0; i < NUM_BLOCKS; i++)
//   {
//     if (!TOC_IS_RESERVED(TOC[i])) {
//       TOC[i] = ~TOC_VALID_MASK;
//     }
//   }
//   
//   return qspifs_saveTOC();
// }

/******************************************************************************
 *
 * Description:
 *    Appends the data to the end of the file.
 *
 * Params:
 *    [in] fh     - The handle to the file as returned from fs_open_append()
 *    [in] pData  - The data to save
 *    [in] size   - Number of bytes to save
 *
 * Returns:
 *    FS_OK on success or one of the FS_ERR_* on failure
 *
 *****************************************************************************/
static fresult qspifs_write(fileHandle_t* fh, const uint8_t * const pData, uint32_t size)
{
    uint32_t left = size;
    const uint8_t* pSrc = pData;
    int rc, i;
    fresult res;
    int failed_attempts = 0;

    do {
        res = qspifs_allocateSpace(fh, size);
        if (res != FS_OK) {
            break;
        }

        opers.dest = (char *) (SPIFI_MEM_BASE + fh->tocIdx * ERASE_SIZE
                + HEADER_LEN + fh->size);
        opers.scratch = NULL;
        opers.protect = 0;
        opers.options = S_VERIFY_PROG; // | S_FORCE_ERASE;

        while ((res == FS_OK) && (left > 0)) {
            if (left >= PROG_SIZE) {
                opers.length = PROG_SIZE;
            } else {
                opers.length = left;
            }
            if (IS_ADDR_IN_SPIFI(pData)) {
                memcpy(addr_conflict_buff, pSrc, opers.length);
                rc = spifi->spifi_program(obj, addr_conflict_buff, &opers);
            } else {
                rc = spifi->spifi_program(obj, (char*) pSrc, &opers);
            }
            res = qspifs_translateSpifiError(rc);
            if ((res == FS_ERR_SPIFI_VERIFICATION)
                    && (++failed_attempts <= NUM_VERIFICATION_ATTEMPTS)) {
                // The verification process failed.
                // In all the observed occasions re-running the exact same
                // spifi_program command again yielded a 0 as a return value
                // the second time.
                // The quick'N'dirty fix is to re-run that program instruction
                // NUM_VERIFICATION_ATTEMPTS more time(s) when this happens.
                res = FS_OK;
                continue;
            }
            if (res != FS_OK) {
                // Got an error but cannot exit this function here as parts of the data
                // (previous loops?) may have been written so the TOC must be updated.
                break;
            }
            pSrc += opers.length;
            opers.dest += opers.length;
            left -= opers.length;
            failed_attempts = 0;
        }

        // update file information
        fh->size = fh->size + size - left;
        fh->lastBlock = fh->tocIdx + ((fh->size + HEADER_LEN)/ ERASE_SIZE);
        left = fh->size + HEADER_LEN;
        for (i = 0; i <= (fh->lastBlock - fh->tocIdx); i++) {
            TOC[fh->tocIdx + i] &= ~TOC_FSIZE_MASK;
            TOC[fh->tocIdx + i] |= FS_MIN(ERASE_SIZE, left);
            left -= FILESIZE(TOC[fh->tocIdx + i]);
        }

        if (res == FS_OK) {
            res = qspifs_saveTOC();
        } else {
            // Want to save the TOC but not overwrite the previous error with
            // a possibly successful TOC saving thus making it seem like there
            // was no error
            qspifs_saveTOC();
        }
    } while (0);

    return res;
}

/******************************************************************************
 *
 * Description:
 *    Tests if str starts with prefix. A prefix of NULL or an empty string
 *    results in a positive result regardless of the content of str.
 *
 * Params:
 *    [in] prefix - The prefix to look for
 *    [in] str    - The string to search for prefix
 *
 * Returns:
 *    True if the specified string starts with prefix
 *
 *****************************************************************************/
static bool qspifs_startsWith(const char* prefix, const char* str)
{
    const char* pA = prefix;
    const char* pB = str;

    if (pA == NULL)
    {
      return true;
    }
    for (; *pA != '\0'; pA++, pB++)
    {
      if (*pB != *pA)
      {
        return false;
      }
    }

    return true;
}

/******************************************************************************
 * Class Declarations
 *****************************************************************************/

class QSPIFileHandle : public FileHandle {

public:
    QSPIFileHandle(fileHandle_t* handle, int flags);

    virtual int close();

    virtual ssize_t write(const void *buffer, size_t length);

    virtual ssize_t read(void *buffer, size_t length);

    virtual int isatty();

    virtual off_t lseek(off_t position, int whence);

    virtual int fsync();

    virtual off_t flen();

protected:

    fileHandle_t fh;
    bool allowReading;
    bool allowWriting;
    uint32_t pos;
};

class QSPIDirHandle : public DirHandle {

public:
    static QSPIDirHandle* openDir(const char* dirname);

    virtual ~QSPIDirHandle();

    virtual int closedir();
    virtual struct dirent *readdir();
    virtual void rewinddir();

private:
    QSPIDirHandle(const char* dirname);    

    int findFileWithPrefix(const char* prefix, int startTOCIdx, int maxTOCIdx) const;

protected:
  
    char* dirname;
    int nextTocIdx;

    bool isRoot;

    struct dirent cur_entry;
};

/******************************************************************************
 * Class Implementations
 *****************************************************************************/

QSPIFileHandle::QSPIFileHandle(fileHandle_t* handle, int flags)
{
    fh = *handle;
    int accmode = (flags & O_ACCMODE);
    allowReading = (accmode == O_RDONLY) || (accmode == O_RDWR);
    allowWriting = (accmode == O_WRONLY) || (accmode == O_RDWR) || (flags & O_APPEND);
    pos = 0;
}

int QSPIFileHandle::close()
{
    delete this;
    return 0;
}

ssize_t QSPIFileHandle::write(const void *buffer, size_t length)
{
    if (!allowWriting) {
        return -1;
    }
    fresult res = qspifs_write(&fh, (const uint8_t*)buffer, length);
    if (res == FS_OK) {
        // A write is always 'append' in this file system so the file
        // position is always end of file after a write
        pos = fh.size;
        return length;
    }
    return -1;
}

ssize_t QSPIFileHandle::read(void *buffer, size_t length)
{
    if (!allowReading) {
        return -1;
    }
    if (pos >= fh.size) {
        return 0;
    }
    uint32_t len = FS_MIN(length, fh.size - pos);
    const char* pData = (const char*)(SPIFI_MEM_BASE + fh.tocIdx*ERASE_SIZE + HEADER_LEN + pos);
    memcpy(buffer, pData, len);
    pos += len;
    return len;
}

int QSPIFileHandle::isatty()
{
    return 0;
}

off_t QSPIFileHandle::lseek(off_t position, int whence)
{
    switch (whence) {
    case SEEK_SET:
        pos = position;
        break;

    case SEEK_CUR:
        pos += position;
        break;

    case SEEK_END:
        pos = fh.size + position;
        break;

    default:
        return -1;
    }
    return pos;
}

int QSPIFileHandle::fsync()
{
    return 0; // always synced
}

off_t QSPIFileHandle::flen()
{
    return fh.size;
}

QSPIDirHandle::QSPIDirHandle(const char* dirname) {
    size_t len = strlen(dirname);
    this->dirname = (char*)malloc(len + 2); // null termination and possible ending '/'
    if (this->dirname != NULL) {
        if (len == 0 || ((len == 1) && (dirname[0] == '/'))) {
            isRoot = true;
            this->dirname[0] = '\0';
        } else {
            isRoot = false;
            memcpy(this->dirname, dirname, len+1);
            if (dirname[len - 1] != '/') {
                this->dirname[len] = '/';
                this->dirname[len+1] = '\0';
            }
        }
        cur_entry.d_name[HEADER_FNAME_STRLEN] = '\0';
        rewinddir();
      }
}

QSPIDirHandle::~QSPIDirHandle()
{
  if (dirname != NULL) {
    delete dirname;
    dirname = NULL;
  }
}

QSPIDirHandle* QSPIDirHandle::openDir(const char* dirname)
{
  QSPIDirHandle* d = new QSPIDirHandle(dirname);
  if (d->dirname == NULL) {
    // failed to allocate memory for the folder name
    delete d;
    d = NULL;
  } else if (!d->isRoot) {
    if (d->findFileWithPrefix(d->dirname, 0, NUM_BLOCKS) == NUM_BLOCKS) {
      // There are no files in this directory, i.e. it does not exist
      delete d;
      d = NULL;
    }
  }
  return d;
}

int QSPIDirHandle::closedir() {
    delete this;
    return 0;
}

int QSPIDirHandle::findFileWithPrefix(const char* prefix, int startTOCIdx, int maxTOCIdx) const
{
  for (int i = startTOCIdx; i < maxTOCIdx; i++) {
    if (TOC_IS_FILE(TOC[i])) {
      const char* filename = (const char*) (SPIFI_MEM_BASE + i * ERASE_SIZE);
      if (qspifs_startsWith(prefix, filename)) {
        return i;
      }
    }
  }
  return NUM_BLOCKS; // no match
}


struct dirent *QSPIDirHandle::readdir() {
  if (nextTocIdx < NUM_BLOCKS) {
    for (int i = nextTocIdx; i < NUM_BLOCKS; i++) {
      int possible = findFileWithPrefix(dirname, i, NUM_BLOCKS);
      if (possible < NUM_BLOCKS) {
        const char* fullfilename = (const char*) (SPIFI_MEM_BASE + possible * ERASE_SIZE);
        const char* filename = fullfilename + strlen(dirname);
        
        if (strchr(filename, '/') == NULL) {
          // file is not in any sub folder so it is truly in the wanted dir
          nextTocIdx = possible + 1;
          strcpy(cur_entry.d_name, filename);
          return &cur_entry;
        }
        
        // this is a file in a subfolder and should not be reported,
        // but the folder name itself should
        strcpy(cur_entry.d_name, fullfilename);
        char* pSlash = strchr(cur_entry.d_name + strlen(dirname), '/');
        pSlash++;
        *pSlash = '\0';
        
        // now that cur_entry.d_name contains the folder's complete 
        // path with a trailing '/', see if it has occurred earlier
        int older = findFileWithPrefix(cur_entry.d_name, 0, i);
        if (older < possible) {
          // already reported, move past this entry
          i = possible;
        } else {
          // found a new subfolder 
          nextTocIdx = possible + 1;
          strcpy(cur_entry.d_name, filename);
          char* pSlash = strchr(cur_entry.d_name, '/');
//          pSlash++; //with ++ the returned dir name is "mydir/" without ++ "mydir" is returned
          *pSlash = '\0';
          return &cur_entry;
        }
      }
    }
  }
  return NULL;
}

void QSPIDirHandle::rewinddir() {
  nextTocIdx = 0;
}


QSPIFileSystem::QSPIFileSystem(const char* name) :
    FileSystemLike(name) {

    activeTOC = -1;
    spifi = NULL;
}

// All modes are supported but:
//
//    1) All writes are treated as appends
//    2) Truncation is only to size 0, i.e. effectively a delete
//    3) File position operations work like this:
//       ReadOnly - dictates where to read from
//       WriteOnly - ignored, writes are always at the end
//       ReadWrite - dictates where to read from, writes ignore it but
//                   sets the position to the end afterwards
//
FileHandle *QSPIFileSystem::open(const char *filename, int flags)
{
    fresult res = qspifs_init();
//     if (res == FS_OK) {
//         if ((flags & O_ACCMODE) == O_RDONLY) {
//             // ok
//         } else if (flags & O_APPEND) {
//             // ok
//         } else {
//             // not supported yet, this includes all combination of flags
//             // allowing writing at specific positions in the file. This file system
//             // only allows appending
//             res = FS_ERR_INVALID_PARAM;
//         }
//     }
    if (res == FS_OK) {
        if (strlen(filename) > HEADER_FNAME_STRLEN) {
            // Filename is too long
            res = FS_ERR_INVALID_PARAM;
        }
    }
    if (res == FS_OK) {
      // Handle truncation by silently deleting the file before
      // attempting to open it
      if (flags & O_TRUNC) {
        remove(filename);
      }
    }
    if (res == FS_OK) {
        fileHandle_t fh = {0,0,0};
        res = qspifs_findFile(filename, &fh);
        if ((res == FS_ERR_NO_FILE) && (flags & O_CREAT)) {
            res = qspifs_allocateFile(filename, 1, &fh.tocIdx);
        }
        if (res == FS_OK) {
            res = qspifs_saveTOC();
        }
        if (res == FS_OK) {
            return new QSPIFileHandle(&fh, flags);
        }
    }
    debug_if(QSPI_DBG, "QSPIFS: Failed to open: %d\n", res);
    return NULL;
}

int QSPIFileSystem::remove(const char *filename)
{
    fileHandle_t fh = {0,0,0};
    fresult res = qspifs_init();
    if (res == FS_OK) {
        res = qspifs_findFile(filename, &fh);
    }
    if (res == FS_OK) {
        qspifs_deleteFile(&fh);
        res = qspifs_saveTOC();
    }
    else if (res == FS_ERR_NO_FILE) {
        // file does not exist so treat it as a successful deletion
        res = FS_OK;
    }
    if (res != FS_OK) {
        debug_if(QSPI_DBG, "QSPIFS: Failed to delete %s: %d\n", filename, res);
        return -1;
    }
    return 0;
}

int QSPIFileSystem::rename(const char *oldname, const char *newname)
{
    fileHandle_t fhOld = {0,0,0};
    fileHandle_t fhNew = {0,0,0};

    fresult res = qspifs_init();
    if (res == FS_OK) {
        res = qspifs_findFile(oldname, &fhOld);
    }
    if (res == FS_OK) {
        // Make sure the destination file doesn't exist
        res = qspifs_findFile(newname, &fhNew);
        if (res == FS_OK) {
            res = FS_ERR_FILE_EXIST;
        } else if (res == FS_ERR_NO_FILE) {
            res = FS_OK;
        }
    }
    if (res == FS_OK) {
        int numNeededBlocks = 1 + ((fhOld.size + HEADER_LEN) / ERASE_SIZE);
        res = qspifs_allocateFile(newname, numNeededBlocks, &fhNew.tocIdx);
        if (res == FS_OK) {
            const uint8_t* pData = (const uint8_t*)(SPIFI_MEM_BASE + fhOld.tocIdx*ERASE_SIZE + HEADER_LEN);
            res = qspifs_write(&fhNew, pData, fhOld.size);
            if (res == FS_OK) {
                qspifs_deleteFile(&fhOld);
            } else {
                qspifs_deleteFile(&fhNew);
            }
        }
        qspifs_saveTOC();
    }
    if (res != FS_OK) {
        debug_if(QSPI_DBG, "QSPIFS: Failed to rename '%s' to '%s': %d\n", oldname, newname, res);
        return -1;
    }
    return 0;
}

DirHandle *QSPIFileSystem::opendir(const char *name)
{
  FileHandle* fh = open(name, O_RDONLY);
  if (fh != NULL) {
    // Attempting to open a file as a dir
    delete fh;
    return NULL;
  }
  
//     printf("opendir: name '%s'\n", name);
  if (strlen(name) <= HEADER_DNAME_MAXLEN) {
    return QSPIDirHandle::openDir(name);
  }
  return NULL;
}

int QSPIFileSystem::mkdir(const char *name, mode_t mode)
{
    // Creating folders is always successful as there are no folders in this filesystem
    return 0;
}

int QSPIFileSystem::format(unsigned int fsSizeInMB)
{
    fresult res = qspifs_init();
    if (res == FS_OK || res == FS_ERR_NOT_FORMATTED) {
        if (((fsSizeInMB<<20) > memInfo.memSize) || (fsSizeInMB < 1)) {
            debug_if(QSPI_DBG, "QSPIFS: Failed to format to size %d MByte: error %d\n", fsSizeInMB, res);
            return -1;
        }
        activeTOC = -1;
        res = qspifs_format(memInfo.memSize - (fsSizeInMB<<20));
    }

    if (res != FS_OK) {
        debug_if(QSPI_DBG, "QSPIFS: Failed to format: %d\n", res);
        return -1;
    }
    return 0;
}

bool QSPIFileSystem::isformatted()
{
    fresult res = qspifs_init();
    if (res == FS_OK) {
        return true;
    } else if (res == FS_ERR_NOT_FORMATTED) {
        return false;
    }
    debug_if(QSPI_DBG, "QSPIFS: Failed to detect status: %d\n", res);
    return false;
}

bool QSPIFileSystem::getMemoryBoundaries(uint32_t* pStartAddr, uint32_t* pEndAddr)
{
  if (isformatted())
  {
    *pEndAddr = 0x28000000 + memInfo.memSize;
    
    // Look at all blocks except for the reserved ones
    for (int i = 0; i < NUM_BLOCKS; i++)
    {
      if (!TOC_IS_RESERVED(TOC[i]))
      {
        // Found first non-reserved erase block, indicating the start of
        // the file system.
        *pStartAddr = SPIFI_MEM_BASE + i*ERASE_SIZE;
        return true;
      }
    }
    
    // The entire file system seems to be reserved which should never happen
    // but just in case, report it as beeing 1MB in size.
    *pStartAddr = *pEndAddr - 1024*1024;
    return true;
  }
  return false;
}
