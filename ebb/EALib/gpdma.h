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

#ifndef GPDMA_H
#define GPDMA_H

#include "platform.h"

#define GPDMA_CONN_SDC              ((1UL))      /*!< SD card */

typedef enum
{
  GPDMA_TRANSFERTYPE_M2M_CONTROLLER_DMA,             /* Memory to memory - DMA control */
  GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA,             /* Memory to peripheral - DMA control */
  GPDMA_TRANSFERTYPE_P2M_CONTROLLER_DMA,             /* Peripheral to memory - DMA control */
  GPDMA_TRANSFERTYPE_P2P_CONTROLLER_DMA,             /* Source peripheral to destination peripheral - DMA control */
  GPDMA_TRANSFERTYPE_P2P_CONTROLLER_DestPERIPHERAL,  /* Source peripheral to destination peripheral - destination peripheral control */
  GPDMA_TRANSFERTYPE_M2P_CONTROLLER_PERIPHERAL,      /* Memory to peripheral - peripheral control */
  GPDMA_TRANSFERTYPE_P2M_CONTROLLER_PERIPHERAL,      /* Peripheral to memory - peripheral control */
  GPDMA_TRANSFERTYPE_P2P_CONTROLLER_SrcPERIPHERAL,  /* Source peripheral to destination peripheral - source peripheral control */
} gpdma_flowControl_t;

/**
 * @brief  Initialize the GPDMA
 * @param  pGPDMA  : The base of GPDMA on the chip
 * @return  Nothing
 */
void gpdma_init();

/**
 * @brief  Shutdown the GPDMA
 * @param  pGPDMA  : The base of GPDMA on the chip
 * @return  Nothing
 */
void gpdma_deinit();

/**
 * @brief  Stop a stream DMA transfer
 * @param  ChannelNum  : Channel Number to be closed
 * @return  Nothing
 */
void gpdma_stop(uint8_t ChannelNum);

/**
 * @brief  The GPDMA stream interrupt status checking
 * @param  ChannelNum  : Channel Number to be checked on interruption
 * @return  Status:
 *              - true  : DMA transfer success
 *              - false  : DMA transfer failed
 */
bool gpdma_interrupt(uint8_t ChannelNum);

/**
 * @brief  Get a free GPDMA channel for one DMA connection
 * @param  pCh  : Assigned channel number (only valid if success)
 * @return  Status:
 *              - true  : Found a free DMA channel
 *              - false  : No free DMA channels, pCh value is undefined
 */
bool gpdma_getFreeChannel(uint8_t* pCh);

/**
 * @brief  Do a DMA transfer M2M, M2P,P2M or P2P
 * @param  ChannelNum  : Channel used for transfer
 * @param  src      : Address of Memory or PeripheralConnection_ID which is the source
 * @param  dst      : Address of Memory or PeripheralConnection_ID which is the destination
 * @param  TransferType: Select the transfer controller and the type of transfer. Should be:
 *                               - GPDMA_TRANSFERTYPE_M2M_CONTROLLER_DMA
 *                               - GPDMA_TRANSFERTYPE_M2P_CONTROLLER_DMA
 *                               - GPDMA_TRANSFERTYPE_P2M_CONTROLLER_DMA
 *                               - GPDMA_TRANSFERTYPE_P2P_CONTROLLER_DMA
 *                               - GPDMA_TRANSFERTYPE_P2P_CONTROLLER_DestPERIPHERAL
 *                               - GPDMA_TRANSFERTYPE_M2P_CONTROLLER_PERIPHERAL
 *                               - GPDMA_TRANSFERTYPE_P2M_CONTROLLER_PERIPHERAL
 *                               - GPDMA_TRANSFERTYPE_P2P_CONTROLLER_SrcPERIPHERAL
 * @param  Size    : The number of DMA transfers
 * @return  False on error, true on success
 */
//bool gpdma_transfer(uint8_t ChannelNum,
//                    uint32_t src,
//                    uint32_t dst,
//                    gpdma_flowControl_t TransferType,
//                    uint32_t Size);

bool gpdma_transfer_to_mci(uint8_t ChannelNum,
                           uint32_t src,
                           uint32_t Size);
bool gpdma_transfer_from_mci(uint8_t ChannelNum,
                             uint32_t dst,
                             uint32_t Size);
#endif


