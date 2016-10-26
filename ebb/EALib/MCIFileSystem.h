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

#ifndef MCIFILESYSTEM_H
#define MCIFILESYSTEM_H

#include "mbed.h"
#include "FATFileSystem.h"

/** Access the filesystem on an SD Card using MCI
 *
 * @code
 * #include "mbed.h"
 * #include "MCIFileSystem.h"
 *
 * MCIFileSystem mcifs("mci");
 *
 * int main() {
 *     printf("Please insert a SD/MMC card\n");
 *     while(!mcifs.cardInserted()) {
 *         wait(0.5);
 *     }
 *     
 *     printf("Found SD/MMC card, writing to /mci/myfile.txt ...\n");
 *     
 *     FILE *fp = fopen("/mci/myfile.txt", "w");
 *     if (fp != NULL) {
 *         fprintf(fp, "Hello World!\n");
 *         fclose(fp);
 *         printf("Wrote to /mci/myfile.txt\n");
 *     } else {
 *         printf("Failed to open /mci/myfile.txt\n");
 *     }
 * }
 * @endcode
 */
class MCIFileSystem : public FATFileSystem {
public:

    /** Create the File System for accessing an SD/MMC Card using MCI
     *
     * @param name The name used to access the virtual filesystem
     * @param cd   The pin connected to the CardDetect line
     */
    MCIFileSystem(const char* name, PinName cd = p38);

    virtual ~MCIFileSystem();

    virtual int disk_initialize();
    virtual int disk_status();
    virtual int disk_read(uint8_t * buffer, uint64_t block_number);
    virtual int disk_write(const uint8_t * buffer, uint64_t block_number);
    virtual int disk_sync();
    virtual uint64_t disk_sectors();

    void mci_MCIIRQHandler();
    void mci_DMAIRQHandler();

    /** Tests if a SD/MMC card is inserted or not.
     *
     * @returns
     *   True if a card has been inserted,
     *   False if no card is inserted or if the card detect pin is unavailable
     */
    bool cardInserted() const;

private:

    typedef enum {
      CardStatusNumBytes = 4,
    } Constants;

    typedef enum {
      PowerOff = 0,
      PowerUp = 2,
      PowerOn = 3,
    } power_ctrl_t;

    typedef enum {
      SDMMC_IDLE_ST = 0, /*!< Idle state */
      SDMMC_READY_ST,    /*!< Ready state */
      SDMMC_IDENT_ST,    /*!< Identification State */
      SDMMC_STBY_ST,     /*!< standby state */
      SDMMC_TRAN_ST,     /*!< transfer state */
      SDMMC_DATA_ST,     /*!< Sending-data State */
      SDMMC_RCV_ST,      /*!< Receive-data State */
      SDMMC_PRG_ST,      /*!< Programming State */
      SDMMC_DIS_ST       /*!< Disconnect State */
    } CardState;


    typedef struct {
      uint8_t CmdIndex;
      uint32_t Data[CardStatusNumBytes];
    } response_t;

    /**
     * @brief SDC Clock Control Options
     */
    typedef enum {
      SDC_CLOCK_ENABLE         =  8, /*!< Enable SD Card Bus Clock */
      SDC_CLOCK_POWER_SAVE     =  9, /*!< Disable SD_CLK output when bus is idle */
      SDC_CLOCK_DIVIDER_BYPASS = 10, /*!< Enable bypass of clock divide logic */
      SDC_CLOCK_WIDEBUS_MODE   = 11, /*!< Enable wide bus mode (SD_DAT[3:0] is used instead of SD_DAT[0]) */
    } ClockControl;

    /**
     * @brief SD/MMC Card specific setup data structure
     */
    typedef struct {
      uint32_t response[4];        /*!< Most recent response */
      uint32_t cid[4];             /*!< CID of acquired card  */
      uint32_t csd[4];             /*!< CSD of acquired card */
      uint32_t ext_csd[512 / 4];   /*!< Ext CSD */
      uint32_t card_type;          /*!< Card Type */
      uint16_t rca;                /*!< Relative address assigned to card */
      uint32_t speed;              /*!< Speed */
      uint32_t block_len;          /*!< Card sector size */
      uint32_t device_size;        /*!< Device Size */
      uint32_t blocknr;            /*!< Block Number */
      uint32_t clk_rate;           /*!< Clock rate */
    } SDMMC_CARD_T;

    typedef enum {
      SDC_RET_OK             =  0,
      SDC_RET_CMD_FAILED     = -1,
      SDC_RET_BAD_PARAMETERS = -2,
      SDC_RET_BUS_NOT_IDLE   = -3,
      SDC_RET_TIMEOUT        = -4,
      SDC_RET_ERR_STATE      = -5,
      SDC_RET_NOT_READY      = -6,
      SDC_RET_FAILED         = -7,
    } ReturnCode;

    void initMCI();

    int32_t     mci_Acquire();
    uint32_t    mci_GetCardStatus() const;
    CardState   mci_GetCardState() const;
    ReturnCode  mci_ReadBlocks(void *buffer, int32_t startBlock, int32_t blockNum);
    ReturnCode  mci_WriteBlocks(void *buffer, int32_t startBlock, int32_t blockNum);
    void        mci_SetClock(uint32_t freq) const;
    void        mci_ClockControl(ClockControl ctrlType, bool enable) const;
    void        mci_PowerControl(power_ctrl_t powerMode, uint32_t flag) const;
    ReturnCode  mci_ExecuteCmd(uint32_t Command, uint32_t Arg, response_t* pResp) const;
    ReturnCode  mci_SendIfCond() const;
    ReturnCode  mci_SendOpCond(uint32_t *pOCR) const;
    ReturnCode  mci_SendAppOpCond(uint16_t rca, bool hcs, uint32_t *pOcr, bool *pCCS) const;
    ReturnCode  mci_GetCID(uint32_t *pCID) const;
    ReturnCode  mci_SetAddr(uint16_t addr) const;
    ReturnCode  mci_GetAddr(uint16_t *pRCA) const;
    ReturnCode  mci_GetCSD(uint16_t rca, uint32_t *pCSD) const;
    ReturnCode  mci_SelectCard(uint16_t addr) const;
    ReturnCode  mci_GetStatus(uint16_t rca, uint32_t *pStatus) const;
    void        mci_ProcessCSD();
    ReturnCode  mci_SetBusWidth(uint16_t rca, uint8_t width) const;
    ReturnCode  mci_SetTranState(uint16_t rca) const;
    ReturnCode  mci_SetBlockLength(uint32_t rca, uint32_t block_len) const;
    ReturnCode  mci_SetCardParams() const;
    ReturnCode  mci_StopTransmission(uint32_t rca) const;
    bool        mci_CheckR1Response(uint32_t resp, ReturnCode* pCheckResult) const;
    void        mci_WriteDelay() const;
    ReturnCode  mci_SendCmd(uint32_t Command, uint32_t Arg, uint32_t timeout) const;
    ReturnCode  mci_SendAppCmd(uint16_t rca) const;
    void        mci_SetDataTransfer(uint16_t BlockNum, bool DirFromCard, uint32_t Timeout) const;
    void        mci_GetResp(response_t* pResp) const;
    uint32_t    mci_GetBits(int32_t start, int32_t end, uint32_t *data) const;
    void        mci_SetCommand(uint32_t Cmd, uint32_t Arg) const;
    void        mci_ResetCommand() const;
    int32_t     mci_IRQHandler(uint8_t *txBuf, uint32_t *txCnt, uint8_t *rxBuf, uint32_t *rxCnt);
    int32_t     mci_FIFOIRQHandler(uint8_t *txBuf, uint32_t *txCnt, uint8_t *rxBuf, uint32_t *rxCnt);
    void        mci_ReadFIFO(uint32_t *pDst, bool bFirstHalf) const;
    void        mci_WriteFIFO(uint32_t *pSrc, bool bFirstHalf) const;
                
    void        mci_SetupEventWakeup(uint8_t dmaChannel);
    uint32_t    mci_WaitForEvent() const;

    ReturnCode _readBlocks(uint32_t card_type, uint32_t startBlock, uint32_t blockNum) const;
    ReturnCode _writeBlocks(uint32_t card_type, uint32_t startBlock, uint32_t blockNum) const;


    uint32_t _Stat;
    SDMMC_CARD_T _sdCardInfo;

    DigitalIn* _cardDetect;

    uint8_t _eventDmaChannel;    /*!< DMA Channel used for transfer data */
    volatile bool _eventReceived;
    volatile bool _eventSuccess;
};

#endif

