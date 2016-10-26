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

#include "gpdma.h"

/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/

#define NUM_GPDMA_CHANNELS  8

#define GPDMACH(__x) ((LPC_GPDMACH_TypeDef*)(LPC_GPDMACH0_BASE + (0x20 * (__x))))

#define CH_MASK(__ch)  (((1UL << (__ch)) & 0xFF))

/**
 * @brief GPDMA request connections
 */
#define GPDMA_CONN_MEMORY           ((0UL))
#define GPDMA_CONN_SDC              ((1UL))      /*!< SD card */

/**
 * @brief Macro defines for DMA channel control registers
 */
#define GPDMA_DMACCxControl_TransferSize(n) (((n & 0xFFF) << 0))  /*!< Transfer size*/
#define GPDMA_DMACCxControl_SBSize(n)       (((n & 0x07) << 12))  /*!< Source burst size*/
#define GPDMA_DMACCxControl_DBSize(n)       (((n & 0x07) << 15))  /*!< Destination burst size*/
#define GPDMA_DMACCxControl_SWidth(n)       (((n & 0x07) << 18))  /*!< Source transfer width*/
#define GPDMA_DMACCxControl_DWidth(n)       (((n & 0x07) << 21))  /*!< Destination transfer width*/
#define GPDMA_DMACCxControl_SI              ((1UL << 26))      /*!< Source increment*/
#define GPDMA_DMACCxControl_DI              ((1UL << 27))      /*!< Destination increment*/
#define GPDMA_DMACCxControl_SrcTransUseAHBMaster1   0
#define GPDMA_DMACCxControl_DestTransUseAHBMaster1  0
#define GPDMA_DMACCxControl_Prot1           ((1UL << 28))      /*!< Indicates that the access is in user mode or privileged mode*/
#define GPDMA_DMACCxControl_Prot2           ((1UL << 29))      /*!< Indicates that the access is bufferable or not bufferable*/
#define GPDMA_DMACCxControl_Prot3           ((1UL << 30))      /*!< Indicates that the access is cacheable or not cacheable*/
#define GPDMA_DMACCxControl_I               ((1UL << 31))      /*!< Terminal count interrupt enable bit */

/**
 * @brief GPDMA Burst size in Source and Destination definitions
 */
#define GPDMA_BSIZE_1   ((0UL))  /*!< Burst size = 1 */
#define GPDMA_BSIZE_4   ((1UL))  /*!< Burst size = 4 */
#define GPDMA_BSIZE_8   ((2UL))  /*!< Burst size = 8 */
#define GPDMA_BSIZE_16  ((3UL))  /*!< Burst size = 16 */
#define GPDMA_BSIZE_32  ((4UL))  /*!< Burst size = 32 */
#define GPDMA_BSIZE_64  ((5UL))  /*!< Burst size = 64 */
#define GPDMA_BSIZE_128 ((6UL))  /*!< Burst size = 128 */
#define GPDMA_BSIZE_256 ((7UL))  /*!< Burst size = 256 */

/**
 * @brief Width in Source transfer width and Destination transfer width definitions
 */
#define GPDMA_WIDTH_BYTE        ((0UL))  /*!< Width = 1 byte */
#define GPDMA_WIDTH_HALFWORD    ((1UL))  /*!< Width = 2 bytes */
#define GPDMA_WIDTH_WORD        ((2UL))  /*!< Width = 4 bytes */

/**
 * @brief Macro defines for DMA Configuration register
 */
#define GPDMA_DMACConfig_E              ((0x01))  /*!< DMA Controller enable*/
#define GPDMA_DMACConfig_M              ((0x02))  /*!< AHB Master endianness configuration*/
#define GPDMA_DMACConfig_BITMASK        ((0x03))

/**
 * @brief Macro defines for DMA Channel Configuration registers
 */
#define GPDMA_DMACCxConfig_E                    ((1UL << 0))      /*!< DMA control enable*/
#define GPDMA_DMACCxConfig_SrcPeripheral(n)     (((n & 0x1F) << 1))    /*!< Source peripheral*/
#define GPDMA_DMACCxConfig_DestPeripheral(n)    (((n & 0x1F) << 6))    /*!< Destination peripheral*/
#define GPDMA_DMACCxConfig_TransferType(n)      (((n & 0x7) << 11))    /*!< This value indicates the type of transfer*/
#define GPDMA_DMACCxConfig_IE                   ((1UL << 14))      /*!< Interrupt error mask*/
#define GPDMA_DMACCxConfig_ITC                  ((1UL << 15))      /*!< Terminal count interrupt mask*/
#define GPDMA_DMACCxConfig_L                    ((1UL << 16))      /*!< Lock*/
#define GPDMA_DMACCxConfig_A                    ((1UL << 17))      /*!< Active*/
#define GPDMA_DMACCxConfig_H                    ((1UL << 18))      /*!< Halt*/

/**
 * @brief GPDMA structure using for DMA configuration
 */
typedef struct {
  uint32_t ChannelNum;  /*!< DMA channel number, should be in
               *  range from 0 to 7.
               *  Note: DMA channel 0 has the highest priority
               *  and DMA channel 7 the lowest priority.
               */
  uint32_t TransferSize;  /*!< Length/Size of transfer */
  uint32_t TransferWidth;  /*!< Transfer width - used for TransferType is GPDMA_TRANSFERTYPE_M2M only */
  uint32_t SrcAddr;    /*!< Physical Source Address, used in case TransferType is chosen as
               *   GPDMA_TRANSFERTYPE_M2M or GPDMA_TRANSFERTYPE_M2P */
  uint32_t DstAddr;    /*!< Physical Destination Address, used in case TransferType is chosen as
               *   GPDMA_TRANSFERTYPE_M2M or GPDMA_TRANSFERTYPE_P2M */
  uint32_t TransferType;  /*!< Transfer Type, should be one of the following:
               * - GPDMA_TRANSFERTYPE_M2M: Memory to memory - DMA control
               * - GPDMA_TRANSFERTYPE_M2P: Memory to peripheral - DMA control
               * - GPDMA_TRANSFERTYPE_P2M: Peripheral to memory - DMA control
               * - GPDMA_TRANSFERTYPE_P2P: Source peripheral to destination peripheral - DMA control
               */
} GPDMA_Channel_CFG_T;

/******************************************************************************
 * External global variables
 *****************************************************************************/

/******************************************************************************
 * Local variables
 *****************************************************************************/

static bool used_channels[NUM_GPDMA_CHANNELS];

/******************************************************************************
 * Local Functions
 *****************************************************************************/

static void gpdma_transfer(GPDMA_Channel_CFG_T* cfg,
                           uint32_t CtrlWord,
                           uint32_t LinkListItem,
                           uint8_t SrcPeripheral,
                           uint8_t DstPeripheral)
{
  /* Get Channel pointer */
  LPC_GPDMACH_TypeDef* pCh = GPDMACH(cfg->ChannelNum);

  /* Reset the Interrupt status */
  LPC_GPDMA->IntTCClear = CH_MASK(cfg->ChannelNum);
  LPC_GPDMA->IntErrClr = CH_MASK(cfg->ChannelNum);

  /* Assign Linker List Item value */
  pCh->CLLI = LinkListItem;

  /* Enable DMA channels, little endian */
  LPC_GPDMA->Config = GPDMA_DMACConfig_E;
  while (!(LPC_GPDMA->Config & GPDMA_DMACConfig_E)) {}

  pCh->CSrcAddr = cfg->SrcAddr;
  pCh->CDestAddr = cfg->DstAddr;

  /* Configure DMA Channel, enable Error Counter and Terminate counter */
  pCh->CConfig = GPDMA_DMACCxConfig_IE
           | GPDMA_DMACCxConfig_ITC
           | GPDMA_DMACCxConfig_TransferType((uint32_t) cfg->TransferType)
           | GPDMA_DMACCxConfig_SrcPeripheral(SrcPeripheral)
           | GPDMA_DMACCxConfig_DestPeripheral(DstPeripheral);

  pCh->CControl = CtrlWord;

  /* Start the Channel */
  pCh->CConfig |= GPDMA_DMACCxConfig_E;
}

/******************************************************************************
 * Public Functions
 *****************************************************************************/

void gpdma_init()
{
  uint8_t i;

  /* Enable GPDMA master clock */
  LPC_SC->PCONP |= (1<<29);

  /* Reset all channel configuration register */
  for (i = 0; i < NUM_GPDMA_CHANNELS; i++) {
    GPDMACH(i)->CConfig = 0;
  }

  /* Clear all DMA interrupt and error flag */
  LPC_GPDMA->IntTCClear = 0xFF;
  LPC_GPDMA->IntErrClr  = 0xFF;

  /* Reset all channels are free */
  for (int i = 0; i < NUM_GPDMA_CHANNELS; i++)
  {
    used_channels[i] = false;
  }
}

void gpdma_deinit()
{
  /* Disable GPDMA master clock */
  LPC_SC->PCONP &= ~(1<<29);
}

void gpdma_stop(uint8_t ChannelNum)
{
  if (ChannelNum >= NUM_GPDMA_CHANNELS) {
    return;
  }

  /* Disable channel */
  GPDMACH(ChannelNum)->CConfig &= ~GPDMA_DMACCxConfig_E;

  /* check terminal count interrupt request status for DMA */
  if (LPC_GPDMA->IntTCStat & CH_MASK(ChannelNum)) {
    /* Clear terminate counter Interrupt pending */
    LPC_GPDMA->IntTCClear = CH_MASK(ChannelNum);
  }

  /* check status of the error interrupt for DMA channels */
  if (LPC_GPDMA->IntErrStat & CH_MASK(ChannelNum)) {
    /* clear the error interrupt request */
    LPC_GPDMA->IntErrClr = CH_MASK(ChannelNum);
  }

  used_channels[ChannelNum] = false;
}

bool gpdma_interrupt(uint8_t ChannelNum)
{
  /* check status of DMA channel interrupts */
  if (LPC_GPDMA->IntStat & CH_MASK(ChannelNum)) {
    /* Check counter terminal status */
    if (LPC_GPDMA->IntTCStat & CH_MASK(ChannelNum)) {
      /* Clear terminate counter Interrupt pending */
      LPC_GPDMA->IntTCClear = CH_MASK(ChannelNum);
      return true;
    }
    /* Check error terminal status */
    if (LPC_GPDMA->IntErrStat & CH_MASK(ChannelNum)) {
      /* Clear error counter Interrupt pending */
      LPC_GPDMA->IntErrClr = CH_MASK(ChannelNum);
      return false;
    }
  }
  return false;
}

bool gpdma_getFreeChannel(uint8_t* pCh)
{
  for (int i = 0; i < NUM_GPDMA_CHANNELS; i++)
  {
    if ((!used_channels[i]) && ((LPC_GPDMA->EnbldChns & CH_MASK(i)) == 0))
    {
      used_channels[i] = true;
      *pCh = i;
      return true;
    }
  }
  return false;
}

bool gpdma_transfer_to_mci(uint8_t ChannelNum,
                           uint32_t src,
                           uint32_t Size)
{
  GPDMA_Channel_CFG_T cfg;
  cfg.ChannelNum = ChannelNum;
  cfg.TransferType = GPDMA_TRANSFERTYPE_M2P_CONTROLLER_PERIPHERAL;
  cfg.TransferSize = Size;
  cfg.TransferWidth = 0;
  cfg.SrcAddr = src;
  cfg.DstAddr = (uint32_t) (&LPC_MCI->FIFO);

  uint32_t ctrl_word =
      GPDMA_DMACCxControl_TransferSize((uint32_t) cfg.TransferSize)
          | GPDMA_DMACCxControl_SBSize(GPDMA_BSIZE_8)
          | GPDMA_DMACCxControl_DBSize(GPDMA_BSIZE_8)
          | GPDMA_DMACCxControl_SWidth(GPDMA_WIDTH_WORD)
          | GPDMA_DMACCxControl_DWidth(GPDMA_WIDTH_WORD)
          | GPDMA_DMACCxControl_DestTransUseAHBMaster1
          | GPDMA_DMACCxControl_SI
          | GPDMA_DMACCxControl_I;

  if (LPC_GPDMA->EnbldChns & CH_MASK(ChannelNum)) {
    /* This channel is enabled, return ERROR, need to release this channel first */
    return false;
  }

  /* Select SD card interface in the DMA MUX*/
  LPC_SC->DMAREQSEL &= ~(1 << 1);

  gpdma_transfer(&cfg, ctrl_word, 0, GPDMA_CONN_MEMORY, GPDMA_CONN_SDC);
  return true;
}

bool gpdma_transfer_from_mci(uint8_t ChannelNum,
                             uint32_t dst,
                             uint32_t Size)
{
  GPDMA_Channel_CFG_T cfg;
  cfg.ChannelNum = ChannelNum;
  cfg.TransferType = GPDMA_TRANSFERTYPE_P2M_CONTROLLER_PERIPHERAL;
  cfg.TransferSize = Size;
  cfg.TransferWidth = 0;
  cfg.SrcAddr = (uint32_t) (&LPC_MCI->FIFO);
  cfg.DstAddr = dst;

  uint32_t ctrl_word =
      GPDMA_DMACCxControl_TransferSize((uint32_t) cfg.TransferSize)
          | GPDMA_DMACCxControl_SBSize(GPDMA_BSIZE_8)
          | GPDMA_DMACCxControl_DBSize(GPDMA_BSIZE_8)
          | GPDMA_DMACCxControl_SWidth(GPDMA_WIDTH_WORD)
          | GPDMA_DMACCxControl_DWidth(GPDMA_WIDTH_WORD)
          | GPDMA_DMACCxControl_SrcTransUseAHBMaster1
          | GPDMA_DMACCxControl_DI
          | GPDMA_DMACCxControl_I;

  if (LPC_GPDMA->EnbldChns & CH_MASK(ChannelNum)) {
    /* This channel is enabled, return ERROR, need to release this channel first */
    return false;
  }

  /* Select SD card interface in the DMA MUX*/
  LPC_SC->DMAREQSEL &= ~(1 << 1);

  gpdma_transfer(&cfg, ctrl_word, 0, GPDMA_CONN_SDC, GPDMA_CONN_MEMORY);
  return true;
}


