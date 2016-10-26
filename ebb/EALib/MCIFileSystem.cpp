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

#include "MCIFileSystem.h"
#include "mbed_debug.h"

#include "diskio.h" //STA_* defines
#include "gpdma.h"

/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/

#define MCI_DBG             0

#define CMD_TIMEOUT                  (0x10000)

#define DATA_TIMER_VALUE_R           (SDC_TRAN_CLOCK_RATE / 4)    // 250ms
#define DATA_TIMER_VALUE_W           (SDC_TRAN_CLOCK_RATE)    // 1000ms
#define ACQUIRE_DELAY       (0.100f)      /*!< inter-command acquire oper condition delay in seconds */

#define SYSCTL_CLOCK_SDC    (1<<28)

/**
 * @brief SDC Clear Register bit definitions
 */
/** Clear all status flag*/
#define SDC_CLEAR_ALL       ((uint32_t) 0x7FF)

/*
 * SDMMC Card bus clock rate definitions
 */
/** Card bus clock in Card Identification Mode */
#define SDC_IDENT_CLOCK_RATE         (400000)  /* 400KHz */
/** Card bus clock in Data Transfer Mode */
#define SDC_TRAN_CLOCK_RATE        (20000000)  /* 20MHz */

/**
 * @brief SDC Power Control Register bit definitions
 */
/** SD_CMD Output Control */
#define SDC_PWR_OPENDRAIN       (((uint32_t) 1) << 6)

/**
 * @brief SDC Command Register bit definitions
 */
/** SDC Command Register Bitmask */
#define SDC_COMMAND_BITMASK     ((uint32_t) 0x7FF)
/** SDC Command Index Bitmask */
#define SDC_COMMAND_INDEX_BITMASK   ((uint32_t) 0x3F)
/** Set SDC Command Index */
#define SDC_COMMAND_INDEX(n)        ((uint32_t) n & 0x3F)
/** No response is expected */
#define SDC_COMMAND_NO_RSP          (((uint32_t) 0 ) << 6)
/** Short response is expected */
#define SDC_COMMAND_SHORT_RSP       (((uint32_t) 1 ) << 6)
/** Long response is expected */
#define SDC_COMMAND_LONG_RSP        (((uint32_t) 3 ) << 6)
/** Response bit mask */
#define SDC_COMMAND_RSP_BITMASK     (((uint32_t) 3 ) << 6)
/** Mark that command timer is disabled and CPSM waits for interrupt request */
#define SDC_COMMAND_INTERRUPT       (((uint32_t) 1 ) << 8)
/** Mark that CPSM waits for CmdPend before starting sending a command*/
#define SDC_COMMAND_PENDING     (((uint32_t) 1 ) << 9)
/** Enable CPSM */
#define SDC_COMMAND_ENABLE          (((uint32_t) 1 ) << 10)

/**
 * @brief SDC Command Response Register bit definitions
 */
/** SDC Command Response value */
#define SDC_RESPCOMMAND_VAL(n)      ((uint32_t) n & 0x3F)

/*
 * SD/MMC Response type definitions
 */
#define CMDRESP_NONE_TYPE       (SDC_COMMAND_NO_RSP)
#define CMDRESP_R1_TYPE         (SDC_COMMAND_SHORT_RSP)
#define CMDRESP_R1b_TYPE        (SDC_COMMAND_SHORT_RSP)
#define CMDRESP_R2_TYPE         (SDC_COMMAND_LONG_RSP)
#define CMDRESP_R3_TYPE         (SDC_COMMAND_SHORT_RSP)
#define CMDRESP_R6_TYPE         (SDC_COMMAND_SHORT_RSP)
#define CMDRESP_R7_TYPE         (SDC_COMMAND_SHORT_RSP)

/*
 * SD command values (Command Index, Response)
 */
#define SD_GO_IDLE_STATE           (SDC_COMMAND_INDEX(MMC_GO_IDLE_STATE) | CMDRESP_NONE_TYPE | SDC_COMMAND_INTERRUPT)  /*!< GO_IDLE_STATE(MMC) or RESET(SD) */
#define SD_CMD1_SEND_OP_COND       (SDC_COMMAND_INDEX(MMC_SEND_OP_COND) | CMDRESP_R3_TYPE | 0)          /*!< SEND_OP_COND(MMC) or ACMD41(SD) */
#define SD_CMD2_ALL_SEND_CID       (SDC_COMMAND_INDEX(MMC_ALL_SEND_CID) | CMDRESP_R2_TYPE | 0)          /*!< ALL_SEND_CID */
#define SD_CMD3_SET_RELATIVE_ADDR  (SDC_COMMAND_INDEX(MMC_SET_RELATIVE_ADDR) | CMDRESP_R1_TYPE | 0)        /*!< SET_RELATE_ADDR */
#define SD_CMD3_SEND_RELATIVE_ADDR (SDC_COMMAND_INDEX(SD_SEND_RELATIVE_ADDR) | CMDRESP_R6_TYPE | 0)        /*!< SEND_RELATE_ADDR */
#define SD_CMD7_SELECT_CARD        (SDC_COMMAND_INDEX(MMC_SELECT_CARD) | CMDRESP_R1b_TYPE | 0)          /*!< SELECT/DESELECT_CARD */
#define SD_CMD8_SEND_IF_COND       (SDC_COMMAND_INDEX(SD_CMD8) | CMDRESP_R7_TYPE | 0)              /*!< SEND_IF_COND */
#define SD_CMD9_SEND_CSD           (SDC_COMMAND_INDEX(MMC_SEND_CSD) | CMDRESP_R2_TYPE | 0)            /*!< SEND_CSD */
#define SD_CMD12_STOP_TRANSMISSION (SDC_COMMAND_INDEX(MMC_STOP_TRANSMISSION) | CMDRESP_R1_TYPE | 0)        /*!< STOP_TRANSMISSION */
#define SD_CMD13_SEND_STATUS       (SDC_COMMAND_INDEX(MMC_SEND_STATUS) | CMDRESP_R1_TYPE | 0)          /*!< SEND_STATUS */

/* Block-Oriented Read Commands (class 2) */
#define SD_CMD16_SET_BLOCKLEN      (SDC_COMMAND_INDEX(MMC_SET_BLOCKLEN) | CMDRESP_R1_TYPE | 0)          /*!< SET_BLOCK_LEN */
#define SD_CMD17_READ_SINGLE_BLOCK (SDC_COMMAND_INDEX(MMC_READ_SINGLE_BLOCK) | CMDRESP_R1_TYPE | 0)        /*!< READ_SINGLE_BLOCK */
#define SD_CMD18_READ_MULTIPLE_BLOCK (SDC_COMMAND_INDEX(MMC_READ_MULTIPLE_BLOCK) | CMDRESP_R1_TYPE | 0)      /*!< READ_MULTIPLE_BLOCK */

/* Block-Oriented Write Commands (class 4) */
#define SD_CMD24_WRITE_BLOCK       (SDC_COMMAND_INDEX(MMC_WRITE_BLOCK) | CMDRESP_R1_TYPE | 0)          /*!< WRITE_BLOCK */
#define SD_CMD25_WRITE_MULTIPLE_BLOCK (SDC_COMMAND_INDEX(MMC_WRITE_MULTIPLE_BLOCK) | CMDRESP_R1_TYPE | 0)    /*!< WRITE_MULTIPLE_BLOCK */

/* Erase Commands (class 5) */
#define SD_CMD32_ERASE_WR_BLK_START (SDC_COMMAND_INDEX(SD_ERASE_WR_BLK_START) | CMDRESP_R1_TYPE | 0)      /*!< ERASE_WR_BLK_START */
#define SD_CMD33_ERASE_WR_BLK_END   (SDC_COMMAND_INDEX(SD_ERASE_WR_BLK_END) | CMDRESP_R1_TYPE | 0)        /*!< ERASE_WR_BLK_END */
#define SD_CMD38_ERASE              (SDC_COMMAND_INDEX(SD_ERASE) | CMDRESP_R1b_TYPE | 0)            /*!< ERASE */

/* Application-Specific Commands (class 8) */
#define SD_CMD55_APP_CMD           (SDC_COMMAND_INDEX(MMC_APP_CMD) | CMDRESP_R1_TYPE | 0)            /*!< APP_CMD */
#define SD_ACMD6_SET_BUS_WIDTH     (SDC_COMMAND_INDEX(SD_APP_SET_BUS_WIDTH) | CMDRESP_R1_TYPE | 0)        /*!< SET_BUS_WIDTH */
#define SD_ACMD13_SEND_SD_STATUS   (SDC_COMMAND_INDEX(MMC_SEND_STATUS) | CMDRESP_R1_TYPE | 0)          /*!< SEND_SD_STATUS */
#define SD_ACMD41_SD_SEND_OP_COND  (SDC_COMMAND_INDEX(SD_APP_OP_COND) | CMDRESP_R3_TYPE | 0)          /*!< SD_SEND_OP_COND */

/**
 * @brief SDC Interrupt Mask Register bit definitions
 */
/** Mask CmdCrcFail flag.*/
#define SDC_MASK0_CMDCRCFAIL     (((uint32_t) 1 ) << 0)
/** Mask DataCrcFail flag. */
#define SDC_MASK0_DATACRCFAIL     (((uint32_t) 1 ) << 1)
/** Mask CmdTimeOut flag. */
#define SDC_MASK0_CMDTIMEOUT     (((uint32_t) 1 ) << 2)
/** Mask DataTimeOut flag. */
#define SDC_MASK0_DATATIMEOUT     (((uint32_t) 1 ) << 3)
/** Mask TxUnderrun flag. */
#define SDC_MASK0_TXUNDERRUN     (((uint32_t) 1 ) << 4)
/** Mask RxOverrun flag. */
#define SDC_MASK0_RXOVERRUN     (((uint32_t) 1 ) << 5)
/** Mask CmdRespEnd flag. */
#define SDC_MASK0_CMDRESPEND     (((uint32_t) 1 ) << 6)
/** Mask CmdSent flag.*/
#define SDC_MASK0_CMDSENT     (((uint32_t) 1 ) << 7)
/** Mask DataEnd flag.*/
#define SDC_MASK0_DATAEND     (((uint32_t) 1 ) << 8)
/** Mask StartBitErr flag.*/
#define SDC_MASK0_STARTBITERR     (((uint32_t) 1 ) << 9)
/** Mask DataBlockEnd flag.*/
#define SDC_MASK0_DATABLOCKEND     (((uint32_t) 1 ) << 10)
/** Mask CmdActive flag.*/
#define SDC_MASK0_CMDACTIVE     (((uint32_t) 1 ) << 11)
/** Mask TxActive flag.*/
#define SDC_MASK0_TXACTIVE     (((uint32_t) 1 ) << 12)
/** Mask RxActive flag.*/
#define SDC_MASK0_RXACTIVE     (((uint32_t) 1 ) << 13)
/** Mask TxFifoHalfEmpty flag.*/
#define SDC_MASK0_TXFIFOHALFEMPTY     (((uint32_t) 1 ) << 14)
/** Mask RxFifoHalfFull flag.*/
#define SDC_MASK0_RXFIFOHALFFULL     (((uint32_t) 1 ) << 15)
/** Mask TxFifoFull flag.*/
#define SDC_MASK0_TXFIFOFULL     (((uint32_t) 1 ) << 16)
/** Mask RxFifoFull flag.*/
#define SDC_MASK0_RXFIFOFULL     (((uint32_t) 1 ) << 17)
/** Mask TxFifoEmpty flag.*/
#define SDC_MASK0_TXFIFOEMPTY     (((uint32_t) 1 ) << 18)
/** Mask RxFifoEmpty flag.*/
#define SDC_MASK0_RXFIFOEMPTY     (((uint32_t) 1 ) << 19)
/** Mask TxDataAvlbl flag.*/
#define SDC_MASK0_TXDATAAVLBL     (((uint32_t) 1 ) << 20)
/** Mask RxDataAvlbl flag.*/
#define SDC_MASK0_RXDATAAVLBL     (((uint32_t) 1 ) << 21)
/** CMD error interrupt mask */
#define SDC_MASK0_CMDERR    (SDC_MASK0_CMDCRCFAIL | SDC_MASK0_CMDTIMEOUT | SDC_MASK0_STARTBITERR)
/** Data Transmit Error interrupt mask */
#define SDC_MASK0_TXDATAERR    (SDC_MASK0_DATACRCFAIL | SDC_MASK0_DATATIMEOUT | SDC_MASK0_TXUNDERRUN | SDC_MASK0_STARTBITERR)
/** Data Receive Error interrupt mask */
#define SDC_MASK0_RXDATAERR    (SDC_MASK0_DATACRCFAIL | SDC_MASK0_DATATIMEOUT | SDC_MASK0_RXOVERRUN | SDC_MASK0_STARTBITERR)
/** Data Transfer interrupt mask*/
#define SDC_MASK0_DATA    (SDC_MASK0_DATAEND | SDC_MASK0_DATABLOCKEND )

/**
 * @brief SDC Clock Control Register bit definitions
 */
/** SDC Clock Control Register Bitmask */
#define SDC_CLOCK_BITMASK       ((uint32_t) 0xFFF)
/** SDC Clock Divider Bitmask */
#define SDC_CLOCK_CLKDIV_BITMASK    (((uint32_t) 0xFF ) << 0)
/** Set SDC Clock Divide value */
#define SDC_CLOCK_CLKDIV(n)     (((uint32_t) (n & 0x0FF)) << 0)

/**
 * @brief SDC Status Register bit definitions
 */
/** Command Response received (CRC check failed) */
#define SDC_STATUS_CMDCRCFAIL     (((uint32_t) 1 ) << 0)
/** Data block sent/received (CRC check failed). */
#define SDC_STATUS_DATACRCFAIL     (((uint32_t) 1 ) << 1)
/** Command response timeout.. */
#define SDC_STATUS_CMDTIMEOUT     (((uint32_t) 1 ) << 2)
/** Data timeout. */
#define SDC_STATUS_DATATIMEOUT     (((uint32_t) 1 ) << 3)
/** Transmit FIFO underrun error. */
#define SDC_STATUS_TXUNDERRUN     (((uint32_t) 1 ) << 4)
/** Receive FIFO overrun error. */
#define SDC_STATUS_RXOVERRUN     (((uint32_t) 1 ) << 5)
/** Command response received (CRC check passed). */
#define SDC_STATUS_CMDRESPEND     (((uint32_t) 1 ) << 6)
/** Command sent (no response required).*/
#define SDC_STATUS_CMDSENT     (((uint32_t) 1 ) << 7)
/** Data end (data counter is zero).*/
#define SDC_STATUS_DATAEND     (((uint32_t) 1 ) << 8)
/** Start bit not detected on all data signals in wide bus mode..*/
#define SDC_STATUS_STARTBITERR     (((uint32_t) 1 ) << 9)
/** Data block sent/received (CRC check passed).*/
#define SDC_STATUS_DATABLOCKEND     (((uint32_t) 1 ) << 10)
/** Command transfer in progress.*/
#define SDC_STATUS_CMDACTIVE     (((uint32_t) 1 ) << 11)
/** Data transmit in progress.*/
#define SDC_STATUS_TXACTIVE     (((uint32_t) 1 ) << 12)
/** Data receive in progress.*/
#define SDC_STATUS_RXACTIVE     (((uint32_t) 1 ) << 13)
/** Transmit FIFO half empty.*/
#define SDC_STATUS_TXFIFOHALFEMPTY     (((uint32_t) 1 ) << 14)
/** Receive FIFO half full.*/
#define SDC_STATUS_RXFIFOHALFFULL     (((uint32_t) 1 ) << 15)
/** Transmit FIFO full.*/
#define SDC_STATUS_TXFIFOFULL     (((uint32_t) 1 ) << 16)
/** Receive FIFO full.*/
#define SDC_STATUS_RXFIFOFULL     (((uint32_t) 1 ) << 17)
/** Transmit FIFO empty.*/
#define SDC_STATUS_TXFIFOEMPTY     (((uint32_t) 1 ) << 18)
/** Receive FIFO empty.*/
#define SDC_STATUS_RXFIFOEMPTY     (((uint32_t) 1 ) << 19)
/** Data available in transmit FIFO.*/
#define SDC_STATUS_TXDATAAVLBL     (((uint32_t) 1 ) << 20)
/** Data available in receive FIFO.*/
#define SDC_STATUS_RXDATAAVLBL     (((uint32_t) 1 ) << 21)
/** Command Error Status */
#define SDC_STATUS_CMDERR    (SDC_STATUS_CMDCRCFAIL | SDC_STATUS_CMDTIMEOUT | SDC_STATUS_STARTBITERR)
/** Data Error Status */
#define SDC_STATUS_DATAERR    (SDC_STATUS_DATACRCFAIL | SDC_STATUS_DATATIMEOUT | SDC_STATUS_TXUNDERRUN \
                 | SDC_STATUS_RXOVERRUN | SDC_STATUS_STARTBITERR)
/** FIFO Status*/
#define SDC_STATUS_FIFO    (SDC_STATUS_TXFIFOHALFEMPTY | SDC_STATUS_RXFIFOHALFFULL \
              | SDC_STATUS_TXFIFOFULL  | SDC_STATUS_RXFIFOFULL \
              | SDC_STATUS_TXFIFOEMPTY | SDC_STATUS_RXFIFOEMPTY \
              | SDC_STATUS_DATABLOCKEND)

/** Data Transfer Status*/
#define SDC_STATUS_DATA    (SDC_STATUS_DATAEND )

/**
 * @brief SDC Data Control Register bit definitions
 */
/** SDC Data Control Register Bitmask */
#define SDC_DATACTRL_BITMASK        ((uint32_t) 0xFF)
/** Enable Data Transfer */
#define SDC_DATACTRL_ENABLE             (((uint32_t) 1 ) << 0)
/** Mark that Data is transfer from card to controller */
#define SDC_DATACTRL_DIR_FROMCARD       (((uint32_t) 1 ) << 1)
/** Mark that Data is transfer from controller to card */
#define SDC_DATACTRL_DIR_TOCARD         ((uint32_t) 0)
/** Mark that the transfer mode is Stream Data Transfer */
#define SDC_DATACTRL_XFER_MODE_STREAM   (((uint32_t) 1 ) << 2)
/** Mark that the transfer mode is Block Data Transfer */
#define SDC_DATACTRL_XFER_MODE_BLOCK    ((uint32_t) 0)
/** Enable DMA */
#define SDC_DATACTRL_DMA_ENABLE         (((uint32_t) 1 ) << 3)
/** Set Data Block size */
#define SDC_DATACTRL_BLOCKSIZE(n)       (((uint32_t) (n & 0x0F) ) << 4)
/** Get Data Block size value */
#define SDC_DATACTRL_BLOCKSIZE_VAL(n)   (((uint32_t) 1) << n)

/**
 * @brief OCR Register definitions
 */
/** Support voltage range 2.7-3.6 */
#define SDC_OCR_27_36               ((uint32_t) 0x00FF8000)
/** Card power up status bit */
#define SDC_OCR_IDLE                (((uint32_t) 1) << 31)
#define SDC_OCR_BUSY                (((uint32_t) 0) << 31)


/* SD/MMC commands - this matrix shows the command, response types, and
   supported card type for that command.
   Command                 Number Resp  SD  MMC
   ----------------------- ------ ----- --- ---
   Reset (go idle)         CMD0   NA    x   x
   Send op condition       CMD1   R3        x
   All send CID            CMD2   R2    x   x
   Send relative address   CMD3   R1        x
   Send relative address   CMD3   R6    x
   Program DSR             CMD4   NA        x
   Select/deselect card    CMD7   R1b       x
   Select/deselect card    CMD7   R1    x
   Send CSD                CMD9   R2    x   x
   Send CID                CMD10  R2    x   x
   Read data until stop    CMD11  R1    x   x
   Stop transmission       CMD12  R1/b  x   x
   Send status             CMD13  R1    x   x
   Go inactive state       CMD15  NA    x   x
   Set block length        CMD16  R1    x   x
   Read single block       CMD17  R1    x   x
   Read multiple blocks    CMD18  R1    x   x
   Write data until stop   CMD20  R1        x
   Setblock count          CMD23  R1        x
   Write single block      CMD24  R1    x   x
   Write multiple blocks   CMD25  R1    x   x
   Program CID             CMD26  R1        x
   Program CSD             CMD27  R1    x   x
   Set write protection    CMD28  R1b   x   x
   Clear write protection  CMD29  R1b   x   x
   Send write protection   CMD30  R1    x   x
   Erase block start       CMD32  R1    x
   Erase block end         CMD33  R1    x
   Erase block start       CMD35  R1        x
   Erase block end         CMD36  R1        x
   Erase blocks            CMD38  R1b       x
   Fast IO                 CMD39  R4        x
   Go IRQ state            CMD40  R5        x
   Lock/unlock             CMD42  R1b       x
   Application command     CMD55  R1        x
   General command         CMD56  R1b       x

 *** SD card application commands - these must be preceded with ***
 *** MMC CMD55 application specific command first               ***
   Set bus width           ACMD6  R1    x
   Send SD status          ACMD13 R1    x
   Send number WR blocks   ACMD22 R1    x
   Set WR block erase cnt  ACMD23 R1    x
   Send op condition       ACMD41 R3    x
   Set clear card detect   ACMD42 R1    x
   Send CSR                ACMD51 R1    x */

/**
 * @brief  Possible SDMMC card state types
 */
typedef enum {
  SDMMC_IDLE_ST = 0,  /*!< Idle state */
  SDMMC_READY_ST,     /*!< Ready state */
  SDMMC_IDENT_ST,     /*!< Identification State */
  SDMMC_STBY_ST,      /*!< standby state */
  SDMMC_TRAN_ST,      /*!< transfer state */
  SDMMC_DATA_ST,      /*!< Sending-data State */
  SDMMC_RCV_ST,       /*!< Receive-data State */
  SDMMC_PRG_ST,       /*!< Programming State */
  SDMMC_DIS_ST        /*!< Disconnect State */
} SDMMC_STATE_T;


/**
 * @brief SD/MMC commands, arguments and responses
 * Standard SD/MMC commands (3.1)       type  argument     response
 */
/* class 1 */
#define MMC_GO_IDLE_STATE         0    /* bc                          */
#define MMC_SEND_OP_COND          1    /* bcr  [31:0]  OCR        R3  */
#define MMC_ALL_SEND_CID          2    /* bcr                     R2  */
#define MMC_SET_RELATIVE_ADDR     3    /* ac   [31:16] RCA        R1  */
#define MMC_SET_DSR               4    /* bc   [31:16] RCA            */
#define MMC_SELECT_CARD           7    /* ac   [31:16] RCA        R1  */
#define MMC_SEND_EXT_CSD          8    /* bc                      R1  */
#define MMC_SEND_CSD              9    /* ac   [31:16] RCA        R2  */
#define MMC_SEND_CID             10    /* ac   [31:16] RCA        R2  */
#define MMC_STOP_TRANSMISSION    12    /* ac                      R1b */
#define MMC_SEND_STATUS          13    /* ac   [31:16] RCA        R1  */
#define MMC_GO_INACTIVE_STATE    15    /* ac   [31:16] RCA            */

/* class 2 */
#define MMC_SET_BLOCKLEN         16    /* ac   [31:0]  block len  R1  */
#define MMC_READ_SINGLE_BLOCK    17    /* adtc [31:0]  data addr  R1  */
#define MMC_READ_MULTIPLE_BLOCK  18    /* adtc [31:0]  data addr  R1  */

/* class 3 */
#define MMC_WRITE_DAT_UNTIL_STOP 20    /* adtc [31:0]  data addr  R1  */

/* class 4 */
#define MMC_SET_BLOCK_COUNT      23    /* adtc [31:0]  data addr  R1  */
#define MMC_WRITE_BLOCK          24    /* adtc [31:0]  data addr  R1  */
#define MMC_WRITE_MULTIPLE_BLOCK 25    /* adtc                    R1  */
#define MMC_PROGRAM_CID          26    /* adtc                    R1  */
#define MMC_PROGRAM_CSD          27    /* adtc                    R1  */

/* class 6 */
#define MMC_SET_WRITE_PROT       28    /* ac   [31:0]  data addr  R1b */
#define MMC_CLR_WRITE_PROT       29    /* ac   [31:0]  data addr  R1b */
#define MMC_SEND_WRITE_PROT      30    /* adtc [31:0]  wpdata addr R1  */

/* class 5 */
#define MMC_ERASE_GROUP_START    35    /* ac   [31:0]  data addr  R1  */
#define MMC_ERASE_GROUP_END      36    /* ac   [31:0]  data addr  R1  */
#define MMC_ERASE                37    /* ac                      R1b */
#define SD_ERASE_WR_BLK_START    32    /* ac   [31:0]  data addr  R1  */
#define SD_ERASE_WR_BLK_END      33    /* ac   [31:0]  data addr  R1  */
#define SD_ERASE                 38    /* ac                      R1b */

/* class 9 */
#define MMC_FAST_IO              39    /* ac   <Complex>          R4  */
#define MMC_GO_IRQ_STATE         40    /* bcr                     R5  */

/* class 7 */
#define MMC_LOCK_UNLOCK          42    /* adtc                    R1b */

/* class 8 */
#define MMC_APP_CMD              55    /* ac   [31:16] RCA        R1  */
#define MMC_GEN_CMD              56    /* adtc [0]     RD/WR      R1b */

/* SD commands                           type  argument     response */
/* class 8 */
/* This is basically the same command as for MMC with some quirks. */
#define SD_SEND_RELATIVE_ADDR     3    /* ac                      R6  */
#define SD_CMD8                   8    /* bcr  [31:0]  OCR        R3  */

/* Application commands */
#define SD_APP_SET_BUS_WIDTH      6    /* ac   [1:0]   bus width  R1   */
#define SD_APP_OP_COND           41    /* bcr  [31:0]  OCR        R1 (R4)  */
#define SD_APP_SEND_SCR          51    /* adtc                    R1   */


/**
 * @brief MMC status in R1<br>
 * Type<br>
 *   e : error bit<br>
 *   s : status bit<br>
 *   r : detected and set for the actual command response<br>
 *   x : detected and set during command execution. the host must poll
 *       the card by sending status command in order to read these bits.
 * Clear condition<br>
 *   a : according to the card state<br>
 *   b : always related to the previous command. Reception of
 *       a valid command will clear it (with a delay of one command)<br>
 *   c : clear by read<br>
 */

#define R1_OUT_OF_RANGE         (1UL << 31)  /* er, c */
#define R1_ADDRESS_ERROR        (1 << 30)  /* erx, c */
#define R1_BLOCK_LEN_ERROR      (1 << 29)  /* er, c */
#define R1_ERASE_SEQ_ERROR      (1 << 28)  /* er, c */
#define R1_ERASE_PARAM          (1 << 27)  /* ex, c */
#define R1_WP_VIOLATION         (1 << 26)  /* erx, c */
#define R1_CARD_IS_LOCKED       (1 << 25)  /* sx, a */
#define R1_LOCK_UNLOCK_FAILED   (1 << 24)  /* erx, c */
#define R1_COM_CRC_ERROR        (1 << 23)  /* er, b */
#define R1_ILLEGAL_COMMAND      (1 << 22)  /* er, b */
#define R1_CARD_ECC_FAILED      (1 << 21)  /* ex, c */
#define R1_CC_ERROR             (1 << 20)  /* erx, c */
#define R1_ERROR                (1 << 19)  /* erx, c */
#define R1_UNDERRUN             (1 << 18)  /* ex, c */
#define R1_OVERRUN              (1 << 17)  /* ex, c */
#define R1_CID_CSD_OVERWRITE    (1 << 16)  /* erx, c, CID/CSD overwrite */
#define R1_WP_ERASE_SKIP        (1 << 15)  /* sx, c */
#define R1_CARD_ECC_DISABLED    (1 << 14)  /* sx, a */
#define R1_ERASE_RESET          (1 << 13)  /* sr, c */
#define R1_STATUS(x)            (x & 0xFFFFE000)
#define R1_CURRENT_STATE(x)     ((x & 0x00001E00) >> 9)  /* sx, b (4 bits) */
#define R1_READY_FOR_DATA       (1 << 8)  /* sx, a */
#define R1_APP_CMD              (1 << 5)  /* sr, c */


/**
 * @brief SD/MMC card OCR register bits
 */
#define OCR_ALL_READY           (1UL << 31)  /* Card Power up status bit */
#define OCR_HC_CCS              (1 << 30)  /* High capacity card */
#define OCR_VOLTAGE_RANGE_MSK   (0x00FF8000)

#define SD_SEND_IF_ARG          0x000001AA
#define SD_SEND_IF_ECHO_MSK     0x000000FF
#define SD_SEND_IF_RESP         0x000000AA

/**
 * @brief R3 response definitions
 */
#define CMDRESP_R3_OCR_VAL(n)           (((uint32_t) n) & 0xFFFFFF)
#define CMDRESP_R3_S18A                 (((uint32_t) 1 ) << 24)
#define CMDRESP_R3_HC_CCS               (((uint32_t) 1 ) << 30)
#define CMDRESP_R3_INIT_COMPLETE        (((uint32_t) 1 ) << 31)

/**
 * @brief R6 response definitions
 */
#define CMDRESP_R6_RCA_VAL(n)           (((uint32_t) (n >> 16)) & 0xFFFF)
#define CMDRESP_R6_CARD_STATUS(n)       (((uint32_t) (n & 0x1FFF)) | \
                     ((n & (1 << 13)) ? (1 << 19) : 0) | \
                     ((n & (1 << 14)) ? (1 << 22) : 0) | \
                     ((n & (1 << 15)) ? (1 << 23) : 0))

/**
 * @brief R7 response definitions
 */
/** Echo-back of check-pattern */
#define CMDRESP_R7_CHECK_PATTERN(n)     (((uint32_t) n ) & 0xFF)
/** Voltage accepted */
#define CMDRESP_R7_VOLTAGE_ACCEPTED     (((uint32_t) 1 ) << 8)

/**
 * @brief CMD3 command definitions
 */
/** Card Address */
#define CMD3_RCA(n)         (((uint32_t) (n & 0xFFFF) ) << 16)

/**
 * @brief CMD7 command definitions
 */
/** Card Address */
#define CMD7_RCA(n)         (((uint32_t) (n & 0xFFFF) ) << 16)

/**
 * @brief CMD8 command definitions
 */
/** Check pattern */
#define CMD8_CHECKPATTERN(n)            (((uint32_t) (n & 0xFF) ) << 0)
/** Recommended pattern */
#define CMD8_DEF_PATTERN                    (0xAA)
/** Voltage supplied.*/
#define CMD8_VOLTAGESUPPLIED_27_36     (((uint32_t) 1 ) << 8)

/**
 * @brief CMD9 command definitions
 */
#define CMD9_RCA(n)         (((uint32_t) (n & 0xFFFF) ) << 16)

/**
 * @brief CMD13 command definitions
 */
#define CMD13_RCA(n)            (((uint32_t) (n & 0xFFFF) ) << 16)

/**
 * @brief APP_CMD command definitions
 */
#define CMD55_RCA(n)            (((uint32_t) (n & 0xFFFF) ) << 16)

/**
 * @brief ACMD41 command definitions
 */
#define ACMD41_OCR(n)                   (((uint32_t) n) & 0xFFFFFF)
#define ACMD41_S18R                     (((uint32_t) 1 ) << 24)
#define ACMD41_XPC                      (((uint32_t) 1 ) << 28)
#define ACMD41_HCS                      (((uint32_t) 1 ) << 30)

/**
 * @brief ACMD6 command definitions
 */
#define ACMD6_BUS_WIDTH(n)              ((uint32_t) n & 0x03)
#define ACMD6_BUS_WIDTH_1               (0)
#define ACMD6_BUS_WIDTH_4               (2)

/** @brief Card type defines
 */
#define CARD_TYPE_SD    (1 << 0)
#define CARD_TYPE_4BIT  (1 << 1)
#define CARD_TYPE_8BIT  (1 << 2)
#define CARD_TYPE_HC    (OCR_HC_CCS)/*!< high capacity card > 2GB */

/**
 * @brief SD/MMC sector size in bytes
 */
#define MMC_SECTOR_SIZE     512

/******************************************************************************
 * External global variables
 *****************************************************************************/

/******************************************************************************
 * Local variables
 *****************************************************************************/

static MCIFileSystem* pUglyForIRQ = NULL;

/******************************************************************************
 * Local Functions
 *****************************************************************************/

static void mymciirq()
{
  pUglyForIRQ->mci_MCIIRQHandler();
}

static void mydmairq()
{
  pUglyForIRQ->mci_DMAIRQHandler();
}

/******************************************************************************
 * Public Functions
 *****************************************************************************/

MCIFileSystem::MCIFileSystem(const char* name, PinName cd) :
    FATFileSystem(name)
{
    pUglyForIRQ = this;

    _Stat = STA_NOINIT;
    memset(&_sdCardInfo, 0, sizeof(SDMMC_CARD_T));
    _eventReceived = false;
    _eventSuccess = false;

    initMCI();

    if (cd == NC)
    {
        _cardDetect = NULL;
    }
    else
    {
      _cardDetect = new DigitalIn(cd);
      _cardDetect->mode(PullUp);
    }
}

MCIFileSystem::~MCIFileSystem()
{
  if (_cardDetect != NULL)
  {
    delete _cardDetect;
  }
}
void MCIFileSystem::initMCI()
{
    // Pinsel for MCI
    LPC_IOCON->P1_2  = 2;          /* SD_CLK @ P1.2 */
    LPC_IOCON->P1_3  = 2;          /* SD_CMD @ P1.3 */
    LPC_IOCON->P1_5  = 2 | (1<<7); /* SD_PWR @ P1.5 - digital mode */
    LPC_IOCON->P1_6  = 2 | (1<<7); /* SD_DAT[0] @ P1.6 - digital mode */
    LPC_IOCON->P1_7  = 2 | (1<<7); /* SD_DAT[1] @ P1.7 - digital mode */
    LPC_IOCON->P1_11 = 2;          /* SD_DAT[2] @ P1.11 */
    LPC_IOCON->P1_12 = 2;          /* SD_DAT[3] @ P1.12 */

    LPC_SC->PCONP |= SYSCTL_CLOCK_SDC;
    LPC_SC->RSTCON0 = (1<<28);
    LPC_SC->RSTCON0 &= ~(1<<28);

    /* Initialize GPDMA controller */
    gpdma_init();

    /* Initialize SDC peripheral */
    LPC_SC->PCONP |= SYSCTL_CLOCK_SDC;

    /* Disable SD_CLK */
    mci_ClockControl(SDC_CLOCK_ENABLE, false);

    /* Power-off */
    mci_PowerControl(PowerOff, 0);
    mci_WriteDelay();

    /* Disable all interrupts */
    LPC_MCI->MASK0 = 0;

    /*Setting for timeout problem */
    LPC_MCI->DATATMR = 0x1FFFFFFF;

    LPC_MCI->COMMAND = 0;
    mci_WriteDelay();

    LPC_MCI->DATACTRL = 0;
    mci_WriteDelay();

    /* clear all pending interrupts */
    LPC_MCI->CLEAR = SDC_CLEAR_ALL;

    /* Power-up SDC Peripheral */
    mci_PowerControl(PowerUp, 0);

    /* delays for the supply output is stable*/
    for (uint32_t i = 0; i < 0x80000; i++ ) {}

    mci_SetClock(SDC_IDENT_CLOCK_RATE);
    mci_ClockControl(SDC_CLOCK_ENABLE, true);

    /* Power-on SDC Interface */
    mci_PowerControl(PowerOn, 0);

    NVIC_SetVector(MCI_IRQn, (uint32_t) mymciirq);
    NVIC_EnableIRQ(MCI_IRQn);

    NVIC_SetVector(DMA_IRQn, (uint32_t) mydmairq);
    NVIC_EnableIRQ(DMA_IRQn);
}

int MCIFileSystem::disk_initialize() {

    debug_if(MCI_DBG, "mcifs:disk_initialize(), _Stat = %#x\n", _Stat);

    if (!cardInserted()) {
      /* No card in the socket */
      _Stat = STA_NODISK | STA_NOINIT;
    }

    if (_Stat != STA_NOINIT) {
        return _Stat;          /* card is already enumerated */
    }

    //rtc_init();

    /* Initialize the Card Data Strucutre */
    memset(&_sdCardInfo, 0, sizeof(SDMMC_CARD_T));

    /* Reset */
    _Stat = STA_NOINIT;

    /* Enumerate the card once detected. Note this function may block for a little while. */
    int ret = mci_Acquire();
    if (ret != 1) {
      debug("Card Acquire failed... got %d, but expected 1\r\n", ret);
      return 1;//Stat;
    }

    _Stat &= ~STA_NOINIT;
    return _Stat;
}

int MCIFileSystem::disk_write(const uint8_t *buffer, uint64_t block_number) {
    debug_if(MCI_DBG, "mcifs:disk_write(%#x, %llu), _Stat = %#x\n", (uint32_t)buffer, block_number, _Stat);
    if (_Stat & STA_NOINIT) {
        // not ready
        return 1;
    }
    if (mci_WriteBlocks((void*)buffer, block_number, 1) == SDC_RET_OK) {
        return 0;
    }

    return 1;
}

int MCIFileSystem::disk_read(uint8_t *buffer, uint64_t block_number) {
    debug_if(MCI_DBG, "mcifs:disk_read(%#x, %llu), _Stat = %#x\n", (uint32_t)buffer, block_number, _Stat);
    if (_Stat & STA_NOINIT) {
        // not ready
        return _Stat;
    }
    if (mci_ReadBlocks(buffer, block_number, 1) == SDC_RET_OK) {
        return 0;
    }

    return 1;
}

int MCIFileSystem::disk_status()
{
  debug_if(MCI_DBG, "mcifs:disk_status(), _Stat = %#x\n", _Stat);
  return _Stat;
}

int MCIFileSystem::disk_sync()
{
  debug_if(MCI_DBG, "mcifs:disk_sync(), _Stat = %#x\n", _Stat);
  uint32_t end = us_ticker_read() + 50*1000; // 50ms
  while (us_ticker_read() < end)
  {
    if (mci_GetCardStatus() & R1_READY_FOR_DATA)
    {
      // card is ready
      return 0;
    }
  }
  // timeout while waiting for card to get ready
  return 1;
}

uint64_t MCIFileSystem::disk_sectors()
{
    debug_if(MCI_DBG, "mcifs:disk_sectors(), _Stat = %#x, returning %llu\n", _Stat, _sdCardInfo.blocknr);
    return _sdCardInfo.blocknr;
}

void MCIFileSystem::mci_MCIIRQHandler()
{
  int32_t Ret;

  Ret = mci_IRQHandler(NULL, 0, NULL, 0);
  if(Ret < 0) {
    _eventSuccess = false;
    _eventReceived = true;
  }
}

void MCIFileSystem::mci_DMAIRQHandler()
{
  _eventSuccess = gpdma_interrupt(_eventDmaChannel);
  _eventReceived = true;
  NVIC_DisableIRQ(DMA_IRQn);
}

/******************************************************************************
 * Private Functions
 *****************************************************************************/

bool MCIFileSystem::cardInserted() const
{
    // If no card detect pin is given, then assume that a card is inserted.
    // If a pin is specified then use that to determing the presence of a card.
    return ((_cardDetect == NULL) || (_cardDetect->read() == 0));
}



int32_t MCIFileSystem::mci_Acquire()
{
  int32_t Ret;

  /* Initialize card info */
  _sdCardInfo.speed = SDC_TRAN_CLOCK_RATE;
  _sdCardInfo.card_type = 0;

  /* During identification phase, the clock should be less than
     400Khz. Once we pass this phase, the normal clock can be set up
     to 25Mhz on SD card and 20Mhz on MMC card. */
  mci_SetClock(SDC_IDENT_CLOCK_RATE);

  /* Clear Open Drain output control for SD */
  mci_PowerControl(PowerOn, 0);
  
  /* Card Reset */
  Ret = mci_ExecuteCmd(SD_GO_IDLE_STATE, 0, NULL);
  if (Ret != 0) {
    return Ret;
  }

  wait(ACQUIRE_DELAY);

  /* Send interface operation condiftion */
  Ret = mci_SendIfCond();
  if (Ret == SDC_RET_BAD_PARAMETERS) {
    return Ret;    /* Non-compatible voltage range or check pattern is not correct */

  }
  /* Get Card Type */
  if (Ret == SDC_RET_OK) {/* Ver2.00 or later SD Memory Card*/
    bool CCS;
    uint32_t OCR = SDC_OCR_27_36;
    _sdCardInfo.card_type |= CARD_TYPE_SD;
    Ret = mci_SendAppOpCond(0, true, &OCR, &CCS);
    if (CCS) {  /* High Capacity or Extended Capacity SD Memory Card */
      _sdCardInfo.card_type |= CARD_TYPE_HC;
    }
  }
  else {  /*Ver2.00 or later SD Memory Card(voltage mismatch) or Ver1.X SD Memory Card
         or not SD Memory Card*/
    bool CCS;
    uint32_t OCR = SDC_OCR_27_36;
    Ret = mci_SendAppOpCond(0, false, &OCR, &CCS);
    if (Ret == SDC_RET_OK) {
      _sdCardInfo.card_type |= CARD_TYPE_SD;
    }
    else if (Ret == SDC_RET_BAD_PARAMETERS) {
      return Ret;
    }
    else {  /* MMC Card setup */
      uint32_t OCR;
      /* Enter to Open Drain mode */
      mci_PowerControl(PowerOn, SDC_PWR_OPENDRAIN);
      wait(ACQUIRE_DELAY);
      Ret = mci_SendOpCond(&OCR);
      if (Ret != SDC_RET_OK) {
        return Ret;
      }

    }
  }

  /* Read CID */
  mci_GetCID(_sdCardInfo.cid);

  /* RCA send, for SD get RCA */
  if (_sdCardInfo.card_type & CARD_TYPE_SD) {
    mci_GetAddr(&_sdCardInfo.rca);
  }
  else {
    _sdCardInfo.rca = 1;
    mci_SetAddr(_sdCardInfo.rca);
    mci_PowerControl(PowerOn, 0);  /* enter to push-pull mode */
  }

  /* Get CSD */
  mci_GetCSD(_sdCardInfo.rca, _sdCardInfo.csd);

  /* Compute card size, block size and no. of blocks  based on CSD response recived. */
  if (_sdCardInfo.cid[0]) {
    mci_ProcessCSD();

    if (mci_SetTranState(_sdCardInfo.rca) != SDC_RET_OK) {
      return 0;
    }

    if (mci_GetCardState() != SDMMC_TRAN_ST) {
      return 0;
    }

    if (mci_SetCardParams() != 0) {
      return 0;
    }
  }

  return (_sdCardInfo.cid[0]) ? 1 : 0;
}

uint32_t MCIFileSystem::mci_GetCardStatus() const
{
  uint32_t Status;
  mci_GetStatus(_sdCardInfo.rca, &Status);
  return Status;
}

MCIFileSystem::CardState MCIFileSystem::mci_GetCardState() const
{
  uint32_t Status;
  volatile int32_t Ret;

  /* get current state of the card */
  Ret = mci_GetStatus(_sdCardInfo.rca, &Status);

  /* check card state in response */
  return (CardState) R1_CURRENT_STATE(Status);
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_StopTransmission(uint32_t rca) const
{
  uint32_t Status;
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint32_t RetryCnt =  20;

  Ret = mci_GetStatus(rca, &Status);
  if (Ret != SDC_RET_OK) {
    return SDC_RET_ERR_STATE;
  }

  if (R1_CURRENT_STATE(Status) == SDMMC_TRAN_ST) {
    return SDC_RET_OK;
  }

  if ((R1_CURRENT_STATE(Status) != SDMMC_DATA_ST) &&
    (R1_CURRENT_STATE(Status) != SDMMC_RCV_ST)) {
    return SDC_RET_ERR_STATE;
  }

  while (RetryCnt > 0) {
    Ret = mci_ExecuteCmd(SD_CMD12_STOP_TRANSMISSION, 0, &Response);
    if (Ret == SDC_RET_OK) {
      if (mci_CheckR1Response(Response.Data[0], &Ret)) {
        if (Ret != SDC_RET_OK) {
          return Ret;
        }
        Ret = mci_GetStatus(rca, &Status);
        if ((R1_CURRENT_STATE(Status) == SDMMC_TRAN_ST) || (R1_CURRENT_STATE(Status) == SDMMC_PRG_ST)) {
          return SDC_RET_OK;
        }
        return SDC_RET_ERR_STATE;
      }
    }
    RetryCnt--;
  }
  return Ret;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_ReadBlocks(void *buffer, int32_t startBlock, int32_t blockNum)
{
  ReturnCode Ret = SDC_RET_FAILED;
  uint8_t dmaChannel;
  int32_t ByteNum = blockNum *  MMC_SECTOR_SIZE;

  do
  {
    /* if card is not acquired return immediately */
    if (( startBlock < 0) || ( (startBlock + blockNum) > _sdCardInfo.blocknr) ) {
      Ret = SDC_RET_NOT_READY;
      break;
    }

    /* Put to tran state */
    Ret = mci_SetTranState(_sdCardInfo.rca);
    if (Ret != SDC_RET_OK) {
      break;
    }

    LPC_MCI->MASK0 = SDC_MASK0_DATA | SDC_MASK0_RXDATAERR;

    /* DMA Setup */
    gpdma_getFreeChannel(&dmaChannel);
    gpdma_transfer_from_mci(dmaChannel, (uint32_t)buffer, ByteNum);
    mci_SetupEventWakeup(dmaChannel);

    /* set transfer information */
    mci_SetDataTransfer(blockNum, true, DATA_TIMER_VALUE_R);

    Ret = _readBlocks(_sdCardInfo.card_type, startBlock, blockNum);
    if (Ret == SDC_RET_OK) {
      /* Wait for transfer Finish */
      if (mci_WaitForEvent() != 0) {
        Ret = SDC_RET_FAILED;
      }
    } else {
      Ret = SDC_RET_FAILED;
    }

    gpdma_stop(dmaChannel);

    if ((blockNum > 1) || (mci_GetCardState() == SDMMC_DATA_ST)) {
      /* Send Stop transmission command */
      mci_StopTransmission(_sdCardInfo.rca);
    }

    /* Wait for card to enter tran state */
    while (mci_GetCardState() != SDMMC_TRAN_ST) {}
      
  } while(false);

  return Ret;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_WriteBlocks(void *buffer, int32_t startBlock, int32_t blockNum)
{
  ReturnCode Ret = SDC_RET_FAILED;
  uint8_t dmaChannel;
  
  do
  {
    /* if card is not acquired return immediately */
    if (( startBlock < 0) || ( (startBlock + blockNum) > _sdCardInfo.blocknr) ) {
      Ret = SDC_RET_NOT_READY;
      break;
    }

    /* Put to tran state */
    Ret = mci_SetTranState(_sdCardInfo.rca);
    if (Ret != SDC_RET_OK) {
      break;
    }

    Ret = _writeBlocks(_sdCardInfo.card_type, startBlock, blockNum);
    if (Ret != SDC_RET_OK) {
      break;
    }

    /*Wait for card enter to rcv state*/
    while (mci_GetCardState() != SDMMC_RCV_ST) {}

    LPC_MCI->MASK0 = SDC_MASK0_DATA | SDC_MASK0_TXDATAERR;

    /* DMA Setup */
    gpdma_getFreeChannel(&dmaChannel);
    gpdma_transfer_to_mci(dmaChannel, (uint32_t)buffer, blockNum*MMC_SECTOR_SIZE);
    mci_SetupEventWakeup(dmaChannel);

    /* set transfer information */
    mci_SetDataTransfer(blockNum, false, DATA_TIMER_VALUE_W);

    /* Wait for transfer done */
    if (mci_WaitForEvent() != 0) {
      Ret = SDC_RET_FAILED;
    }
    gpdma_stop(dmaChannel);

    if ((blockNum > 1) || (mci_GetCardState() == SDMMC_RCV_ST)) {
      /* Send Stop transmission command */
      mci_StopTransmission(_sdCardInfo.rca);
    }

    /* Wait for card to enter tran state */
    while (mci_GetCardState() != SDMMC_TRAN_ST) {}
      
  } while (false);

  return Ret;
}

void MCIFileSystem::mci_SetClock(uint32_t freq) const
{
  uint32_t PClk;
  uint32_t ClkValue = 0;

  PClk = PeripheralClock;

  ClkValue = (PClk + 2 * freq - 1) / (2 * freq);
  if (ClkValue > 0) {
    ClkValue -= 1;
  }
  uint32_t temp;
  temp = (LPC_MCI->CLOCK & (~SDC_CLOCK_CLKDIV_BITMASK));
  LPC_MCI->CLOCK = temp | (SDC_CLOCK_CLKDIV(ClkValue));
  mci_WriteDelay();
}

void MCIFileSystem::mci_ClockControl(ClockControl ctrlType, bool enable) const
{
  if (enable) {
    LPC_MCI->CLOCK |= (1 << ctrlType);
  }
  else {
    LPC_MCI->CLOCK &= (~(1 << ctrlType));
  }
  mci_WriteDelay();
}

void MCIFileSystem::mci_PowerControl(power_ctrl_t powerMode, uint32_t flag) const
{
  LPC_MCI->POWER = (powerMode & 0x3) | flag;
  mci_WriteDelay();
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_ExecuteCmd(uint32_t Command, uint32_t Arg, response_t* pResp) const
{
  ReturnCode Ret = SDC_RET_FAILED;

  /* Send Command to card */
  Ret = mci_SendCmd(Command, Arg, CMD_TIMEOUT);
  if (Ret != SDC_RET_OK) {
    return Ret;
  }

  /* Get response (if any) */
  if ((Command & SDC_COMMAND_RSP_BITMASK) != SDC_COMMAND_NO_RSP) {

    mci_GetResp(pResp);

    /* If the response is not R1, in the response field, the Expected Cmd data
            won't be the same as the CMD data in SendCmd(). Below four cmds have
            R2 or R3 response. We don't need to check if MCI_RESP_CMD is the same
            as the Expected or not. */
    if ((SDC_COMMAND_INDEX(Command) != MMC_SEND_OP_COND) &&
      (SDC_COMMAND_INDEX(Command) != SD_APP_OP_COND) &&
      (SDC_COMMAND_INDEX(Command) != MMC_ALL_SEND_CID) &&
      (SDC_COMMAND_INDEX(Command) != MMC_SEND_CSD) &&
      (pResp->CmdIndex != SDC_COMMAND_INDEX(Command))) {
      return SDC_RET_CMD_FAILED;
    }
  }

  return SDC_RET_OK;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_SendIfCond() const
{
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint32_t RetryCnt =  20;

  while (RetryCnt > 0) {
    Ret = mci_ExecuteCmd(SD_CMD8_SEND_IF_COND, (CMD8_VOLTAGESUPPLIED_27_36 | CMD8_CHECKPATTERN(
                              CMD8_DEF_PATTERN)), &Response);
    if (Ret == SDC_RET_OK) {
      if ((Response.Data[0] & CMDRESP_R7_VOLTAGE_ACCEPTED) &&
        (CMDRESP_R7_CHECK_PATTERN(Response.Data[0]) == CMD8_DEF_PATTERN)) {
        return SDC_RET_OK;
      }
      return SDC_RET_BAD_PARAMETERS;
    }
    RetryCnt--;
  }
  return Ret;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_SendOpCond(uint32_t *pOCR) const
{
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint32_t RetryCnt =  0x200;

  while (RetryCnt > 0) {
    Ret = mci_ExecuteCmd(SD_CMD1_SEND_OP_COND, SDC_OCR_27_36, &Response);
    if (Ret == SDC_RET_OK) {
      *pOCR = Response.Data[0];
      if (*pOCR & SDC_OCR_IDLE) {
        if ((Response.Data[0] & SDC_OCR_27_36) != SDC_OCR_27_36) {
          return SDC_RET_BAD_PARAMETERS;
        }
        return SDC_RET_OK;
      }
    }
    RetryCnt--;
  }
  return SDC_RET_FAILED;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_SendAppOpCond(uint16_t rca, bool hcs, uint32_t *pOcr, bool *pCCS) const
{
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint32_t Argument;
  uint32_t RetryCnt =  0x2000;  /* The host repeatedly issues ACMD41 for at least 1 second or
                         until the busy bit are set to 1 */

  Argument = ACMD41_OCR(*pOcr);
  if (hcs) {
    Argument |= ACMD41_HCS;
  }

  while (RetryCnt > 0) {
    Ret = mci_SendAppCmd(rca);
    if (Ret == SDC_RET_OK) {
      Ret = mci_ExecuteCmd(SD_ACMD41_SD_SEND_OP_COND, Argument, &Response);
      if (Ret == SDC_RET_OK) {
        if (Response.Data[0] & CMDRESP_R3_INIT_COMPLETE) {
          if (*pOcr == 0) {
            *pOcr = CMDRESP_R3_OCR_VAL(Response.Data[0]);
            return SDC_RET_OK;
          }
          if ((CMDRESP_R3_OCR_VAL(Response.Data[0]) & *pOcr) != *pOcr) {
            return SDC_RET_BAD_PARAMETERS;
          }
          *pCCS = (Response.Data[0] & CMDRESP_R3_HC_CCS) ? true : false;
          return SDC_RET_OK;
        }
      }
    }
    else {
      //If we abort here then some cards will go undetected, better to keep retrying
      //return Ret;
    }
    RetryCnt--;
  }
  return SDC_RET_FAILED;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_GetCID(uint32_t *pCID) const
{
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint32_t RetryCnt =  20;

  while (RetryCnt > 0) {
    Ret = mci_ExecuteCmd(SD_CMD2_ALL_SEND_CID, 0, &Response);
    if (Ret == SDC_RET_OK) {
      pCID[3] = Response.Data[0];
      pCID[2] = Response.Data[1];
      pCID[1] = Response.Data[2];
      pCID[0] = Response.Data[3];
      return SDC_RET_OK;
    }
    RetryCnt--;
  }
  return Ret;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_SetAddr(uint16_t addr) const
{
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint32_t RetryCnt =  20;

  while (RetryCnt > 0) {
    Ret = mci_ExecuteCmd(SD_CMD3_SET_RELATIVE_ADDR, CMD3_RCA(addr), &Response);
    if (Ret == SDC_RET_OK) {
      if (mci_CheckR1Response(Response.Data[0], &Ret)) {
        return Ret;
      }
    }
    RetryCnt--;
  }
  return Ret;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_GetAddr(uint16_t *pRCA) const
{
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint32_t RetryCnt =  20;

  *pRCA = 0;
  while (RetryCnt > 0) {
    Ret = mci_ExecuteCmd(SD_CMD3_SEND_RELATIVE_ADDR, 0, &Response);
    if (Ret == SDC_RET_OK) {
      if (!(CMDRESP_R6_CARD_STATUS(Response.Data[0]) & R1_READY_FOR_DATA)) {
        Ret = SDC_RET_NOT_READY;
      }
      else if (R1_CURRENT_STATE(CMDRESP_R6_CARD_STATUS(Response.Data[0])) != SDMMC_STBY_ST) {
        Ret = SDC_RET_ERR_STATE;
      }
      else {
        *pRCA = CMDRESP_R6_RCA_VAL(Response.Data[0]);
        return SDC_RET_OK;
      }
    }
    RetryCnt--;
  }
  return Ret;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_GetCSD(uint16_t rca, uint32_t *pCSD) const
{
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint32_t RetryCnt =  20;

  while (RetryCnt > 0) {
    Ret = mci_ExecuteCmd(SD_CMD9_SEND_CSD, CMD9_RCA(rca), &Response);
    if (Ret == SDC_RET_OK) {
      pCSD[3] = Response.Data[0];
      pCSD[2] = Response.Data[1];
      pCSD[1] = Response.Data[2];
      pCSD[0] = Response.Data[3];
      return Ret;
    }
    RetryCnt--;
  }
  return Ret;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_SelectCard(uint16_t addr) const
{
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint32_t RetryCnt =  20;

  while (RetryCnt > 0) {
    Ret = mci_ExecuteCmd(SD_CMD7_SELECT_CARD, CMD7_RCA(addr), &Response);
    if (Ret == SDC_RET_OK) {
      if (mci_CheckR1Response(Response.Data[0], &Ret)) {
        return Ret;
      }
    }
    RetryCnt--;
  }
  return Ret;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_GetStatus(uint16_t rca, uint32_t *pStatus) const
{
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint32_t RetryCnt =  20;

  *pStatus = (uint32_t) -1;
  while (RetryCnt > 0) {
    Ret = mci_ExecuteCmd(SD_CMD13_SEND_STATUS, CMD13_RCA(rca), &Response);
    if (Ret == SDC_RET_OK) {
      mci_CheckR1Response(Response.Data[0], &Ret);
      *pStatus = Response.Data[0];
      return Ret;
    }
    RetryCnt--;
  }
  return Ret;
}

void MCIFileSystem::mci_ProcessCSD()
{
  int32_t CSize = 0;
  int32_t CSizeMult = 0;
  int32_t Mult = 0;

  /* compute block length based on CSD response */
  _sdCardInfo.block_len = 1 << mci_GetBits(80, 83, _sdCardInfo.csd);

  if ((_sdCardInfo.card_type & CARD_TYPE_HC) && (_sdCardInfo.card_type & CARD_TYPE_SD)) {
    /* See section 5.3.3 CSD Register (CSD Version 2.0) of SD2.0 spec  an explanation for the calculation of these values */
    CSize = mci_GetBits(48, 63, (uint32_t *) _sdCardInfo.csd) + 1;
    _sdCardInfo.blocknr = CSize << 10;  /* 512 byte blocks */
  }
  else {
    /* See section 5.3 of the 4.1 revision of the MMC specs for  an explanation for the calculation of these values */
    CSize = mci_GetBits(62, 73, (uint32_t *) _sdCardInfo.csd);
    CSizeMult = mci_GetBits(47, 49, (uint32_t *) _sdCardInfo.csd);
    Mult = 1 << (CSizeMult + 2);
    _sdCardInfo.blocknr = (CSize + 1) * Mult;

    /* adjust blocknr to 512/block */
    if (_sdCardInfo.block_len > MMC_SECTOR_SIZE) {
      _sdCardInfo.blocknr = _sdCardInfo.blocknr * (_sdCardInfo.block_len >> 9);
    }
  }

  _sdCardInfo.device_size = _sdCardInfo.blocknr << 9;  /* blocknr * 512 */
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_SetBusWidth(uint16_t rca, uint8_t width) const
{
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint8_t RetryCnt =  0x20;

  while (RetryCnt > 0) {
    Ret = mci_SendAppCmd(rca);
    if (Ret == SDC_RET_OK) {
      Ret = mci_ExecuteCmd(SD_ACMD6_SET_BUS_WIDTH, ACMD6_BUS_WIDTH(width), &Response);
      if (Ret == SDC_RET_OK) {
        if (mci_CheckR1Response(Response.Data[0], &Ret)) {
          return Ret;
        }
      }
    }
    RetryCnt--;
  }
  return SDC_RET_FAILED;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_SetTranState(uint16_t rca) const
{
  ReturnCode Ret = SDC_RET_OK;
  uint32_t status = 0;
  SDMMC_STATE_T state;

  /* get current state of the card */
  Ret = mci_GetStatus(rca, &status);
  if (Ret != SDC_RET_OK) {
    /* unable to get the card state. So return immediatly. */
    return Ret;
  }

  /* check card state in response */
  state = (SDMMC_STATE_T) R1_CURRENT_STATE(status);
  switch (state) {
  case SDMMC_STBY_ST:
    /* put card in 'Trans' state */
    Ret = mci_SelectCard(rca);
    if (Ret != SDC_RET_OK) {
      /* unable to put the card in Trans state. So return immediatly. */
      return Ret;
    }
    mci_GetStatus(rca, &status);
    if (((SDMMC_STATE_T) R1_CURRENT_STATE(status)) != SDMMC_TRAN_ST) {
      return SDC_RET_ERR_STATE;
    }
    break;

  case SDMMC_TRAN_ST:
    /*do nothing */
    break;

  default:
    /* card shouldn't be in other states so return */
    return SDC_RET_ERR_STATE;
  }

  return SDC_RET_OK;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_SetBlockLength(uint32_t rca, uint32_t block_len) const
{
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint8_t RetryCnt =  0x20;

  while (RetryCnt > 0) {
    Ret = mci_ExecuteCmd(SD_CMD16_SET_BLOCKLEN, block_len, &Response);
    if (Ret == SDC_RET_OK) {
      if (mci_CheckR1Response(Response.Data[0], &Ret)) {
        return Ret;
      }
    }
    RetryCnt--;
  }
  return SDC_RET_FAILED;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_SetCardParams() const
{
  ReturnCode Ret;

  mci_SetClock(SDC_TRAN_CLOCK_RATE);
  if (_sdCardInfo.card_type & CARD_TYPE_SD) {
    mci_ClockControl(SDC_CLOCK_WIDEBUS_MODE, true);
    Ret = mci_SetBusWidth(_sdCardInfo.rca, ACMD6_BUS_WIDTH_4);
    if (Ret != SDC_RET_OK) {
      return Ret;
    }
  }
  else {
    mci_ClockControl(SDC_CLOCK_WIDEBUS_MODE, false);
  }

  /* set block length */
  Ret = mci_SetBlockLength(_sdCardInfo.rca, MMC_SECTOR_SIZE);
  return Ret;
}

bool MCIFileSystem::mci_CheckR1Response(uint32_t resp, ReturnCode* pCheckResult) const
{
  bool Ret = true;

  if (!(resp & R1_READY_FOR_DATA)) {
    *pCheckResult = SDC_RET_NOT_READY;
    Ret = false;
  }
  else if (R1_STATUS(resp)) {
    *pCheckResult =  SDC_RET_FAILED;
  }
  else {
    *pCheckResult =  SDC_RET_OK;
  }
  return Ret;
}

void MCIFileSystem::mci_WriteDelay() const
{
//  volatile uint8_t i;
//  for ( i = 0; i < 0x10; i++ ) {  /* delay 3MCLK + 2PCLK  */
//  }
  wait(0.00001f); /* delay 10 us */
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_SendCmd(uint32_t Command, uint32_t Arg, uint32_t timeout) const
{
  ReturnCode ret = SDC_RET_TIMEOUT;
  uint32_t Status;

  /* Set Command Info */
  mci_SetCommand(Command, Arg);

  while (timeout) {

    Status = LPC_MCI->STATUS;

    /* check if command was sent */
    if (((Command & SDC_COMMAND_RSP_BITMASK) == SDC_COMMAND_NO_RSP) && (Status & SDC_STATUS_CMDSENT)) {
      ret =  SDC_RET_OK;
      break;
    }
    /* check if response was received */
    if (Status & SDC_STATUS_CMDRESPEND) {
      ret = SDC_RET_OK;
      break;
    }

    /* check command sending status */
    if (Status & SDC_STATUS_CMDERR) {
      if (Status & SDC_STATUS_CMDCRCFAIL) {
        if ((SDC_COMMAND_INDEX(Command) == MMC_SEND_OP_COND) ||
            (SDC_COMMAND_INDEX(Command) == SD_APP_OP_COND) ||
            (SDC_COMMAND_INDEX(Command) == MMC_STOP_TRANSMISSION)) {
          ret = SDC_RET_OK;  /* ignore CRC error if it's a resp for SEND_OP_COND  or STOP_TRANSMISSION. */
          break;
        }
      }
      ret = SDC_RET_CMD_FAILED;
      break;
    }

    timeout--;
  }

  mci_ResetCommand();

  return ret;
}

MCIFileSystem::ReturnCode MCIFileSystem::mci_SendAppCmd(uint16_t rca) const
{
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint32_t RetryCnt =  20;

  while (RetryCnt > 0) {
    Ret = mci_ExecuteCmd(SD_CMD55_APP_CMD, CMD55_RCA(rca), &Response);
    if (Ret == SDC_RET_OK) {
      if (mci_CheckR1Response(Response.Data[0], &Ret)) {
        if (Ret != SDC_RET_OK) {
          return Ret;
        }
        if (Response.Data[0] & R1_APP_CMD) {
          return SDC_RET_OK;
        }
        else {
          Ret = SDC_RET_FAILED;
        }
      }
    }
    RetryCnt--;
  }
  return SDC_RET_FAILED;
}

void MCIFileSystem::mci_SetDataTransfer(uint16_t BlockNum, bool DirFromCard, uint32_t Timeout) const
{
  uint32_t DataCtrl = 0;
  LPC_MCI->DATATMR = Timeout;
  LPC_MCI->DATALEN = BlockNum * 512;

  DataCtrl = SDC_DATACTRL_ENABLE;
  // DataCtrl mode=block, block size=512byte
  DataCtrl |= (0x9 << 4);
  if (DirFromCard) {
    DataCtrl |= (0x1 << 1);
  }
  DataCtrl |= SDC_DATACTRL_DMA_ENABLE;
  LPC_MCI->DATACTRL = DataCtrl;
  mci_WriteDelay();
}

void MCIFileSystem::mci_GetResp(response_t* pResp) const
{
  pResp->CmdIndex = SDC_RESPCOMMAND_VAL(LPC_MCI->RESP_CMD);
  pResp->Data[0] = LPC_MCI->RESP0;
  if (CardStatusNumBytes == 4) {
    pResp->Data[1] = LPC_MCI->RESP1;
    pResp->Data[2] = LPC_MCI->RESP2;
    pResp->Data[3] = LPC_MCI->RESP3;
  }
}

uint32_t MCIFileSystem::mci_GetBits(int32_t start, int32_t end, uint32_t *data) const
{
  uint32_t v;
  uint32_t i = end >> 5;
  uint32_t j = start & 0x1f;

  if (i == (start >> 5)) {
    v = (data[i] >> j);
  }
  else {
    v = ((data[i] << (32 - j)) | (data[start >> 5] >> j));
  }

  return v & ((1 << (end - start + 1)) - 1);
}

void MCIFileSystem::mci_SetCommand(uint32_t Cmd, uint32_t Arg) const
{
  /* Clear status register */
  LPC_MCI->CLEAR = SDC_CLEAR_ALL;

  /* Set the argument first, finally command */
  LPC_MCI->ARGUMENT = Arg;

  /* Write command value, enable the command */
  LPC_MCI->COMMAND = Cmd | SDC_COMMAND_ENABLE;

  mci_WriteDelay();
}

void MCIFileSystem::mci_ResetCommand() const
{
  LPC_MCI->CLEAR = SDC_CLEAR_ALL;

  LPC_MCI->ARGUMENT = 0xFFFFFFFF;

  LPC_MCI->COMMAND = 0;

  mci_WriteDelay();
}

int32_t MCIFileSystem::mci_IRQHandler(uint8_t *txBuf, uint32_t *txCnt, uint8_t *rxBuf, uint32_t *rxCnt)
{
  uint32_t Status;

  Status = LPC_MCI->STATUS;

  if ( Status & SDC_STATUS_DATAERR) {
    LPC_MCI->CLEAR = SDC_STATUS_DATAERR;
    return -1;  /* Data transfer error */
  }

  if ( Status & SDC_STATUS_DATAEND) {
    LPC_MCI->CLEAR = SDC_STATUS_DATAEND;
    LPC_MCI->MASK0 = 0;
    return 0;
  }

  if ( Status & SDC_STATUS_DATABLOCKEND) {
    LPC_MCI->CLEAR = SDC_STATUS_DATABLOCKEND;
    return 1;
  }

  if (Status & SDC_STATUS_FIFO) {
    return mci_FIFOIRQHandler(txBuf, txCnt, rxBuf, rxCnt);
  }

  return 1;
}

int32_t MCIFileSystem::mci_FIFOIRQHandler(uint8_t *txBuf, uint32_t *txCnt, uint8_t *rxBuf, uint32_t *rxCnt)
{
  uint32_t Status;
  Status = LPC_MCI->STATUS;

  if (txBuf) {
    if (Status & SDC_STATUS_TXFIFOHALFEMPTY) {
      if (*txCnt % 64) {
        mci_WriteFIFO((uint32_t *) &txBuf[*txCnt], false);
      }
      else {
        mci_WriteFIFO((uint32_t *) &txBuf[*txCnt], true);
      }
      *txCnt += 32;
    }
  }

  if (rxBuf) {
    if (Status & SDC_STATUS_RXFIFOHALFFULL) {
      if (*rxCnt % 64) {
        mci_ReadFIFO((uint32_t *) &rxBuf[*rxCnt], false);
      }
      else {
        mci_ReadFIFO((uint32_t *) &rxBuf[*rxCnt], true);
      }
      *rxCnt += 32;
    }
  }

  LPC_MCI->CLEAR = SDC_STATUS_FIFO;

  return 1;
}

void MCIFileSystem::mci_ReadFIFO(uint32_t *pDst, bool bFirstHalf) const
{
  uint8_t start = 0, end = 7;

  if (!bFirstHalf) {
    start += 8;
    end += 8;
  }
  for (; start <= end; start++) {
    *pDst = LPC_MCI->FIFO[start];
    pDst++;
  }
}

void MCIFileSystem::mci_WriteFIFO(uint32_t *pSrc, bool bFirstHalf) const
{
  uint8_t start = 0, end = 7;
  if (!bFirstHalf) {
    start += 8;
    end += 8;
  }
  for (; start <= end; start++) {
    LPC_MCI->FIFO[start] = *pSrc;
    pSrc++;
  }
}

void MCIFileSystem::mci_SetupEventWakeup(uint8_t dmaChannel)
{
  /* Wait for IRQ - for an RTOS, you would pend on an event here with a IRQ based wakeup. */
  NVIC_ClearPendingIRQ(DMA_IRQn);

  _eventDmaChannel = dmaChannel;
  _eventReceived = false;
  _eventSuccess = false;

  NVIC_EnableIRQ(DMA_IRQn);
}

uint32_t MCIFileSystem::mci_WaitForEvent() const
{
  /* Wait for the event (DMA or MCI interrupt) for a maximum of 2 seconds */
  uint32_t end = us_ticker_read() + 2*1000*1000;
  while ((us_ticker_read() < end) && (!_eventReceived))
  {
    // If the driver is having problems reading the card, adding a delay here
    // might help.
    //wait(0.01);
  }
  
  if (_eventReceived && _eventSuccess) {
    return 0;
  }

  return 1;
}

MCIFileSystem::ReturnCode MCIFileSystem::_readBlocks(uint32_t card_type, uint32_t startBlock, uint32_t blockNum) const
{
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint32_t Command, Argument;
  uint8_t RetryCnt =  0x20;

  if (blockNum == 1) {
    Command = SD_CMD17_READ_SINGLE_BLOCK;
  }
  else {
    Command = SD_CMD18_READ_MULTIPLE_BLOCK;
  }

  /* Select single or multiple read based on number of blocks */
  /* if high capacity card use block indexing */
  if (card_type & CARD_TYPE_HC) {
    Argument = startBlock;
  }
  else {  /*fix at 512 bytes*/
    Argument = startBlock << 9;
  }

  while (RetryCnt > 0) {
    Ret = mci_ExecuteCmd(Command, Argument, &Response);
    if (Ret == SDC_RET_OK) {
      if (mci_CheckR1Response(Response.Data[0], &Ret)) {
        return Ret;
      }
    }
    RetryCnt--;
  }
  return Ret;
}

MCIFileSystem::ReturnCode MCIFileSystem::_writeBlocks(uint32_t card_type, uint32_t startBlock, uint32_t blockNum) const
{
  ReturnCode Ret = SDC_RET_FAILED;
  response_t Response;
  uint32_t Command, Argument;
  uint8_t RetryCnt =  0x20;

  if (blockNum == 1) {
    Command = SD_CMD24_WRITE_BLOCK;
  }
  else {
    Command = SD_CMD25_WRITE_MULTIPLE_BLOCK;
  }

  /* if high capacity card use block indexing */
  if (card_type & CARD_TYPE_HC) {
    Argument = startBlock;
  }
  else {  /*fix at 512 bytes*/
    Argument = startBlock << 9;

  }

  while (RetryCnt > 0) {
    Ret = mci_ExecuteCmd(Command, Argument, &Response);
    if (Ret == SDC_RET_OK) {
      if (mci_CheckR1Response(Response.Data[0], &Ret)) {
        return Ret;
      }
    }
    RetryCnt--;
  }
  return Ret;
}

