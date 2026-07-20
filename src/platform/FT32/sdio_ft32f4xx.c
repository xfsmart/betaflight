/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <string.h>

#include "platform.h"

#ifdef USE_SDCARD_SDIO

#include "drivers/sdmmc_sdio.h"

#include "pg/sdio.h"

#include "drivers/io.h"
#include "drivers/io_impl.h"
#include "drivers/nvic.h"
#include "drivers/time.h"
#include "platform/rcc.h"
#include "drivers/dma.h"
#include "drivers/dma_reqmap.h"
#include "drivers/light_led.h"

#include "build/debug.h"

/* Define(s) --------------------------------------------------------------------------------------------------------*/

#define BLOCK_SIZE                      ((uint32_t)(512))

#define SD_SOFTWARE_COMMAND_TIMEOUT     ((uint32_t)0x00020000)

#define SD_OCR_ADDR_OUT_OF_RANGE        ((uint32_t)0x80000000)
#define SD_OCR_ADDR_MISALIGNED          ((uint32_t)0x40000000)
#define SD_OCR_BLOCK_LEN_ERR            ((uint32_t)0x20000000)
#define SD_OCR_ERASE_SEQ_ERR            ((uint32_t)0x10000000)
#define SD_OCR_BAD_ERASE_PARAM          ((uint32_t)0x08000000)
#define SD_OCR_WRITE_PROT_VIOLATION     ((uint32_t)0x04000000)
#define SD_OCR_LOCK_UNLOCK_FAILED       ((uint32_t)0x01000000)
#define SD_OCR_COM_CRC_FAILED           ((uint32_t)0x00800000)
#define SD_OCR_ILLEGAL_CMD              ((uint32_t)0x00400000)
#define SD_OCR_CARD_ECC_FAILED          ((uint32_t)0x00200000)
#define SD_OCR_CC_ERROR                 ((uint32_t)0x00100000)
#define SD_OCR_GENERAL_UNKNOWN_ERROR    ((uint32_t)0x00080000)
#define SD_OCR_STREAM_READ_UNDERRUN     ((uint32_t)0x00040000)
#define SD_OCR_STREAM_WRITE_OVERRUN     ((uint32_t)0x00020000)
#define SD_OCR_CID_CSD_OVERWRITE        ((uint32_t)0x00010000)
#define SD_OCR_WP_ERASE_SKIP            ((uint32_t)0x00008000)
#define SD_OCR_CARD_ECC_DISABLED        ((uint32_t)0x00004000)
#define SD_OCR_ERASE_RESET              ((uint32_t)0x00002000)
#define SD_OCR_AKE_SEQ_ERROR            ((uint32_t)0x00000008)
#define SD_OCR_ERRORBITS                ((uint32_t)0xFDFFE008)

#define SD_R6_GENERAL_UNKNOWN_ERROR     ((uint32_t)0x00002000)
#define SD_R6_ILLEGAL_CMD               ((uint32_t)0x00004000)
#define SD_R6_COM_CRC_FAILED            ((uint32_t)0x00008000)

#define SD_VOLTAGE_WINDOW_SD            ((uint32_t)0x80100000)
#define SD_RESP_HIGH_CAPACITY           ((uint32_t)0x40000000)
#define SD_RESP_STD_CAPACITY            ((uint32_t)0x00000000)
#define SD_CHECK_PATTERN                ((uint32_t)0x000001AA)

#define SD_MAX_VOLT_TRIAL               ((uint32_t)0x0000FFFF)
#define SD_ALLZERO                      ((uint32_t)0x00000000)

#define SD_WIDE_BUS_SUPPORT             ((uint32_t)0x00040000)
#define SD_SINGLE_BUS_SUPPORT           ((uint32_t)0x00010000)
#define SD_CARD_LOCKED                  ((uint32_t)0x02000000)

#define SD_0TO7BITS                     ((uint32_t)0x000000FF)
#define SD_8TO15BITS                    ((uint32_t)0x0000FF00)
#define SD_16TO23BITS                   ((uint32_t)0x00FF0000)
#define SD_24TO31BITS                   ((uint32_t)0xFF000000)
#define SD_MAX_DATA_LENGTH              ((uint32_t)0x01FFFFFF)

#define SD_CCCC_ERASE                   ((uint32_t)0x00000020)

#define SD_SDIO_SEND_IF_COND           ((uint32_t)SDMMC_CMD_HS_SEND_EXT_CSD)

// DesignWare SDIO clock divider values
// CLKDIV = (SDIOCLK / (2 * desired_freq)) - 1
// Init clock: ~400KHz from 105MHz APB2 => CLKDIV = (105000000 / (2*400000)) - 1 = 130
#define SDIO_INIT_CLK_DIV              ((uint8_t)0x82)
// Normal clock: must stay within 25MHz SD spec. DIV=2 => 105M/(2*3) = 17.5MHz
#define SDIO_CLK_DIV                   ((uint8_t)0x02)

// DesignWare CMD register bit fields for command construction
#define SDIO_CMD_IDX_MASK              ((uint32_t)0x3F)
#define SDIO_CMD_RESPONSE_EXPECT_BIT   ((uint32_t)(1 << 6))
#define SDIO_CMD_RESPONSE_LENGTH_BIT   ((uint32_t)(1 << 7))
#define SDIO_CMD_CHECK_CRC_BIT         ((uint32_t)(1 << 8))
#define SDIO_CMD_DATA_EXPECTED_BIT     ((uint32_t)(1 << 9))
#define SDIO_CMD_WRITE_BIT             ((uint32_t)(1 << 10))
#define SDIO_CMD_TRANSFER_MODE_BIT     ((uint32_t)(1 << 11))
#define SDIO_CMD_SEND_AUTO_STOP_BIT    ((uint32_t)(1 << 12))
#define SDIO_CMD_WAIT_PRVDATA_BIT      ((uint32_t)(1 << 13))
#define SDIO_CMD_STOP_ABORT_BIT        ((uint32_t)(1 << 14))
#define SDIO_CMD_UPDATE_CLOCK_BIT      ((uint32_t)(1 << 21))
#define SDIO_CMD_START_BIT             ((uint32_t)(1 << 31))

// Command construction macros for DesignWare CMD register
#define SD_CMD_RESPONSE_SHORT           SDIO_CMD_RESPONSE_EXPECT_BIT
#define SD_CMD_RESPONSE_LONG            (SDIO_CMD_RESPONSE_EXPECT_BIT | SDIO_CMD_RESPONSE_LENGTH_BIT)
#define SD_CMD_CHECK_CRC                SDIO_CMD_CHECK_CRC_BIT
#define SD_CMD_DATA_EXPECTED_BIT        SDIO_CMD_DATA_EXPECTED_BIT
#define SD_CMD_WRITE_BIT                SDIO_CMD_WRITE_BIT
#define SD_CMD_SEND_AUTO_STOP_BIT       SDIO_CMD_SEND_AUTO_STOP_BIT

#define SDIO_DIR_TX 1
#define SDIO_DIR_RX 0

/* Typedef(s) -------------------------------------------------------------------------------------------------------*/

typedef enum
{
    SD_SINGLE_BLOCK    = 0,
    SD_MULTIPLE_BLOCK  = 1,
} SD_Operation_t;

typedef struct
{
    uint32_t          CSD[4];
    uint32_t          CID[4];
    volatile uint32_t TransferComplete;
    volatile uint32_t TransferError;
    volatile uint32_t RXCplt;
    volatile uint32_t TXCplt;
    volatile uint32_t Operation;
} SD_Handle_t;

typedef enum
{
    SD_CARD_READY                  = ((uint32_t)0x00000001),
    SD_CARD_IDENTIFICATION         = ((uint32_t)0x00000002),
    SD_CARD_STANDBY                = ((uint32_t)0x00000003),
    SD_CARD_TRANSFER               = ((uint32_t)0x00000004),
    SD_CARD_SENDING                = ((uint32_t)0x00000005),
    SD_CARD_RECEIVING              = ((uint32_t)0x00000006),
    SD_CARD_PROGRAMMING            = ((uint32_t)0x00000007),
    SD_CARD_DISCONNECTED           = ((uint32_t)0x00000008),
    SD_CARD_ERROR                  = ((uint32_t)0x000000FF)
} SD_CardState_t;

/* Variable(s) ------------------------------------------------------------------------------------------------------*/

static SD_Handle_t                 SD_Handle;
SD_CardInfo_t                      SD_CardInfo;
static uint32_t                    SD_Status;
static uint32_t                    SD_CardRCA;
SD_CardType_t                      SD_CardType;
static volatile uint32_t           TimeOut;
static dmaResource_t              *sdioDmaResource;
static const dmaChannelSpec_t     *sdioDmaSpec;

/* Private function(s) ----------------------------------------------------------------------------------------------*/

static void             SD_DataTransferInit         (uint32_t Size, uint32_t DataBlockSize, bool IsItReadFromCard);
static SD_Error_t       SD_TransmitCommand          (uint32_t Command, uint32_t Argument, int8_t ResponseType);
static SD_Error_t       SD_CmdResponse              (uint8_t SD_CMD, int8_t ResponseType);
static void             SD_GetResponse              (uint32_t* pResponse);
static SD_Error_t       SD_ReadFifo                 (uint32_t *Buffer, uint32_t WordCount);
static SD_Error_t       CheckOCR_Response           (uint32_t Response_R1);
static void             SD_DMA_Complete             (dmaChannelDescriptor_t *descriptor);
static SD_Error_t       SD_InitializeCard           (void);

static SD_Error_t       SD_PowerON                  (void);
static SD_Error_t       SD_WideBusOperationConfig   (uint32_t WideMode);
static SD_Error_t       SD_FindSCR                  (uint32_t *pSCR);

void SDIO_DMA_IRQHandler(dmaChannelDescriptor_t *descriptor);

// DesignWare SDIO requires writing CMD register with start bit, then busy-wait
// Separate command index, response type, CRC, data direction fields in CMD register
// Clock control via CLKDIV+CLKENA+CMD_UPDATE_CLOCK registers instead of single CLKCR

/** -----------------------------------------------------------------------------------------------------------------*/
static void SD_DataTransferInit(uint32_t Size, uint32_t DataBlockSize, bool IsItReadFromCard)
{
    SDIO_TimeOutConfig(SD_DATATIMEOUT);
    SDIO_BlockSizeConfig(DataBlockSize);
    SDIO_ByteCountConfig(Size);
    // direction set via SDIO_TransferDirectionConfig instead of SDIO_DCTRL_DTDIR
    if (IsItReadFromCard) {
        SDIO_TransferDirectionConfig(SDIO_TRANSFER_READ_FROM_CARD);
    } else {
        SDIO_TransferDirectionConfig(SDIO_TRANSFER_WRITE_TO_CARD);
    }
}

/** -----------------------------------------------------------------------------------------------------------------*/
static SD_Error_t SD_TransmitCommand(uint32_t Command, uint32_t Argument, int8_t ResponseType)
{
    SD_Error_t ErrorState;

    // Clear all interrupt flags via RINTSTS write-1-to-clear
    SDIO->RINTSTS = 0xFFFFFFFF;

    // Set command argument
    SDIO_CMDARGConfig(Argument);

    // Build CMD register value from Command parameter
    // Command bits [5:0] = command index, other bits carry response/data flags
    uint32_t cmdReg = (Command & SDIO_CMD_IDX_MASK);

    // Response type bits from Command parameter (passed in upper bits)
    if (Command & SDIO_CMD_RESPONSE_EXPECT_BIT) {
        SDIO_ResponseExpectConfig(SDIO_RESPONSE_EXPECT_ENABLE);
    } else {
        SDIO_ResponseExpectConfig(SDIO_RESPONSE_EXPECT_DISABLE);
    }
    if (Command & SDIO_CMD_RESPONSE_LENGTH_BIT) {
        SDIO_ResponseLengthConfig(SDIO_RESPONSE_LONG);
    } else {
        SDIO_ResponseLengthConfig(SDIO_RESPONSE_SHORT);
    }
    if (Command & SDIO_CMD_CHECK_CRC_BIT) {
        SDIO_CheckResponseCRCConfig(SDIO_CHECK_RESPONSE_CRC_ENABLE);
    } else {
        SDIO_CheckResponseCRCConfig(SDIO_CHECK_RESPONSE_CRC_DISABLE);
    }
    if (Command & SDIO_CMD_DATA_EXPECTED_BIT) {
        SDIO_DataExpectedConfig(SDIO_DATA_EXPECTED_ENABLE);
    } else {
        SDIO_DataExpectedConfig(SDIO_DATA_EXPECTED_DISABLE);
    }
    if (Command & SDIO_CMD_WRITE_BIT) {
        SDIO_TransferDirectionConfig(SDIO_TRANSFER_WRITE_TO_CARD);
    } else {
        SDIO_TransferDirectionConfig(SDIO_TRANSFER_READ_FROM_CARD);
    }
    if (Command & SDIO_CMD_TRANSFER_MODE_BIT) {
        SDIO_TransferModeConfig(SDIO_TRANSFER_MODE_STREAM);
    } else {
        SDIO_TransferModeConfig(SDIO_TRANSFER_MODE_BLOCK);
    }
    // Auto stop and wait-prvdata bits must be cleared first
    // to prevent residual bits from previous commands
    SDIO->CMD &= ~(SDIO_CMD_SEND_AUTO_STOP | SDIO_CMD_WAIT_PRVDATA_COMPLETE);
    if (Command & SDIO_CMD_SEND_AUTO_STOP_BIT) {
        SDIO->CMD |= SDIO_CMD_SEND_AUTO_STOP;
    }
    if (Command & SDIO_CMD_WAIT_PRVDATA_BIT) {
        SDIO->CMD |= SDIO_CMD_WAIT_PRVDATA_COMPLETE;
    }

    if ((Argument == 0) && (ResponseType == 0)) {
        ResponseType = -1;  // Go idle command
    }

    // Send command via standard library (handles start bit and busy-wait)
    SDIO_SendCMD(cmdReg);

    ErrorState = SD_CmdResponse(Command & SDIO_CMD_IDX_MASK, ResponseType);

    // Clear flags again
    SDIO->RINTSTS = 0xFFFFFFFF;

    return ErrorState;
}

/** -----------------------------------------------------------------------------------------------------------------*/
static uint32_t SD_RawITStatus(uint32_t Flag)
{
    return SDIO->RINTSTS & Flag;
}

static SD_Error_t SD_CmdResponse(uint8_t SD_CMD, int8_t ResponseType)
{
    uint32_t Response_R1;
    timeUs_t TimeOut;
    uint32_t Flag;

    if (ResponseType == -1) {
        Flag = SDIO_IT_FLAG_CMDDONE;
    } else {
        Flag = SDIO_IT_FLAG_CMDDONE | SDIO_IT_FLAG_RTO | SDIO_IT_FLAG_RCRC;
    }

    TimeOut = micros() + SD_SOFTWARE_COMMAND_TIMEOUT;
    do {
        if (SD_RawITStatus(Flag)) {
            break;
        }
    } while (cmpTimeUs(micros(), TimeOut) < 0);

    if (ResponseType <= 0) {
        if (cmpTimeUs(micros(), TimeOut) >= 0) {
            return SD_CMD_RSP_TIMEOUT;
        } else {
            return SD_OK;
        }
    }

    // Check RTO (response timeout) instead of SDIO_STA_CTIMEOUT
    if (SD_RawITStatus(SDIO_IT_FLAG_RTO)) {
        SDIO_ClearITFlag(SDIO_IT_CLEAN_RTO);
        return SD_CMD_RSP_TIMEOUT;
    }
    if (ResponseType == 3) {
        if (cmpTimeUs(micros(), TimeOut) >= 0) {
            return SD_CMD_RSP_TIMEOUT;
        } else {
            return SD_OK;
        }
    }

    // Check RCRC (response CRC error) instead of SDIO_STA_CCRCFAIL
    if (SD_RawITStatus(SDIO_IT_FLAG_RCRC)) {
        SDIO_ClearITFlag(SDIO_IT_CLEAN_RCRC);
        return SD_CMD_CRC_FAIL;
    }
    if (ResponseType == 2) {
        return SD_OK;
    }

    // Verify response index from STATUS register instead of RESPCMD
    if ((SDIO->STATUS & SDIO_STATUS_RESPONSE_INDEX) != SD_CMD) {
        return SD_ILLEGAL_CMD;
    }

    // Response data in RESP0 instead of RESP1
    Response_R1 = SDIO->RESP0;

    if (ResponseType == 1) {
        return CheckOCR_Response(Response_R1);
    } else if (ResponseType == 6) {
        if ((Response_R1 & (SD_R6_GENERAL_UNKNOWN_ERROR | SD_R6_ILLEGAL_CMD | SD_R6_COM_CRC_FAILED)) == SD_ALLZERO) {
            SD_CardRCA = Response_R1;
        }
        if ((Response_R1 & SD_R6_GENERAL_UNKNOWN_ERROR) == SD_R6_GENERAL_UNKNOWN_ERROR) {
            return SD_GENERAL_UNKNOWN_ERROR;
        }
        if ((Response_R1 & SD_R6_ILLEGAL_CMD) == SD_R6_ILLEGAL_CMD) {
            return SD_ILLEGAL_CMD;
        }
        if ((Response_R1 & SD_R6_COM_CRC_FAILED) == SD_R6_COM_CRC_FAILED) {
            return SD_COM_CRC_FAILED;
        }
    }

    return SD_OK;
}

/** -----------------------------------------------------------------------------------------------------------------*/
static SD_Error_t CheckOCR_Response(uint32_t Response_R1)
{
    if((Response_R1 & SD_OCR_ERRORBITS)             == SD_ALLZERO)                  return SD_OK;
    if((Response_R1 & SD_OCR_ADDR_OUT_OF_RANGE)     == SD_OCR_ADDR_OUT_OF_RANGE)    return SD_ADDR_OUT_OF_RANGE;
    if((Response_R1 & SD_OCR_ADDR_MISALIGNED)       == SD_OCR_ADDR_MISALIGNED)      return SD_ADDR_MISALIGNED;
    if((Response_R1 & SD_OCR_BLOCK_LEN_ERR)         == SD_OCR_BLOCK_LEN_ERR)        return SD_BLOCK_LEN_ERR;
    if((Response_R1 & SD_OCR_ERASE_SEQ_ERR)         == SD_OCR_ERASE_SEQ_ERR)        return SD_ERASE_SEQ_ERR;
    if((Response_R1 & SD_OCR_BAD_ERASE_PARAM)       == SD_OCR_BAD_ERASE_PARAM)      return SD_BAD_ERASE_PARAM;
    if((Response_R1 & SD_OCR_WRITE_PROT_VIOLATION)  == SD_OCR_WRITE_PROT_VIOLATION) return SD_WRITE_PROT_VIOLATION;
    if((Response_R1 & SD_OCR_LOCK_UNLOCK_FAILED)    == SD_OCR_LOCK_UNLOCK_FAILED)   return SD_LOCK_UNLOCK_FAILED;
    if((Response_R1 & SD_OCR_COM_CRC_FAILED)        == SD_OCR_COM_CRC_FAILED)       return SD_COM_CRC_FAILED;
    if((Response_R1 & SD_OCR_ILLEGAL_CMD)           == SD_OCR_ILLEGAL_CMD)          return SD_ILLEGAL_CMD;
    if((Response_R1 & SD_OCR_CARD_ECC_FAILED)       == SD_OCR_CARD_ECC_FAILED)      return SD_CARD_ECC_FAILED;
    if((Response_R1 & SD_OCR_CC_ERROR)              == SD_OCR_CC_ERROR)             return SD_CC_ERROR;
    if((Response_R1 & SD_OCR_GENERAL_UNKNOWN_ERROR) == SD_OCR_GENERAL_UNKNOWN_ERROR)return SD_GENERAL_UNKNOWN_ERROR;
    if((Response_R1 & SD_OCR_STREAM_READ_UNDERRUN)  == SD_OCR_STREAM_READ_UNDERRUN) return SD_STREAM_READ_UNDERRUN;
    if((Response_R1 & SD_OCR_STREAM_WRITE_OVERRUN)  == SD_OCR_STREAM_WRITE_OVERRUN) return SD_STREAM_WRITE_OVERRUN;
    if((Response_R1 & SD_OCR_CID_CSD_OVERWRITE)     == SD_OCR_CID_CSD_OVERWRITE)    return SD_CID_CSD_OVERWRITE;
    if((Response_R1 & SD_OCR_WP_ERASE_SKIP)         == SD_OCR_WP_ERASE_SKIP)        return SD_WP_ERASE_SKIP;
    if((Response_R1 & SD_OCR_CARD_ECC_DISABLED)     == SD_OCR_CARD_ECC_DISABLED)    return SD_CARD_ECC_DISABLED;
    if((Response_R1 & SD_OCR_ERASE_RESET)           == SD_OCR_ERASE_RESET)          return SD_ERASE_RESET;
    if((Response_R1 & SD_OCR_AKE_SEQ_ERROR)         == SD_OCR_AKE_SEQ_ERROR)        return SD_AKE_SEQ_ERROR;

    return SD_OK;
}

/** -----------------------------------------------------------------------------------------------------------------*/
static void SD_GetResponse(uint32_t* pResponse)
{
    // DesignWare uses RESP0-3 instead of RESP1-4
    pResponse[0] = SDIO->RESP0;
    pResponse[1] = SDIO->RESP1;
    pResponse[2] = SDIO->RESP2;
    pResponse[3] = SDIO->RESP3;
}

/** -----------------------------------------------------------------------------------------------------------------*/
static SD_Error_t SD_ReadFifo(uint32_t *Buffer, uint32_t WordCount)
{
    uint32_t Count = SD_DATATIMEOUT;
    uint32_t Index = 0;

    while ((Index < WordCount) && (Count > 0)) {
        if (!SDIO_GetFifoStatus(SDIO_STAT_FIFO_EMPTY)) {
            Buffer[Index++] = SDIO_DATA->DATA;
            Count = SD_DATATIMEOUT;
        } else if (SD_RawITStatus(SDIO_IT_FLAG_DTO | SDIO_IT_FLAG_DCRC | SDIO_IT_FLAG_DRTO | SDIO_IT_FLAG_FRUN)) {
            break;
        } else {
            Count--;
        }
    }

    if ((Count == 0) || SD_RawITStatus(SDIO_IT_FLAG_DRTO)) return SD_DATA_TIMEOUT;
    if (SD_RawITStatus(SDIO_IT_FLAG_DCRC)) return SD_DATA_CRC_FAIL;
    if (SD_RawITStatus(SDIO_IT_FLAG_FRUN)) return SD_RX_OVERRUN;
    if (Index != WordCount) return SD_DATA_TIMEOUT;
    if (!SDIO_GetFifoStatus(SDIO_STAT_FIFO_EMPTY)) return SD_OUT_OF_BOUND;

    return SD_OK;
}

/** -----------------------------------------------------------------------------------------------------------------*/
static void SD_DMA_Complete(dmaChannelDescriptor_t *descriptor)
{
    UNUSED(descriptor);

    if (SD_Handle.RXCplt) {
        if (SD_Handle.Operation == ((SDIO_DIR_RX << 1) | SD_MULTIPLE_BLOCK)) {
            SD_TransmitCommand((SDMMC_CMD_STOP_TRANSMISSION | SD_CMD_RESPONSE_SHORT), 0, 1);
        }

        // Disable DMA via CTRL register instead of SDIO_DCTRL_DMAEN
        SDIO->CTRL &= ~SDIO_CTRL_DMA_ENABLE;

        SDIO->RINTSTS = 0xFFFFFFFF;
        SD_Handle.RXCplt = 0;

        // Disable DMA channel via xDMA_Cmd
        xDMA_Cmd(sdioDmaResource, DISABLE);
    } else {
        // Enable data end interrupt
        SDIO_ITConfig(SDIO_IT_MASK_DTO);
    }
}

/** -----------------------------------------------------------------------------------------------------------------*/
static SD_Error_t SD_InitializeCard(void)
{
    SD_Error_t ErrorState = SD_OK;

    // Check PWREN register instead of SDIO_POWER_PWRCTRL
    if ((SDIO->PWREN & SDIO_PWREN_POWER_ENABLE_0) != 0)
    {
        if (SD_CardType != SD_SECURE_DIGITAL_IO)
        {
            if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_ALL_SEND_CID | SD_CMD_RESPONSE_LONG), 0, 2)) != SD_OK)
            {
                return ErrorState;
            }
            SD_GetResponse(SD_Handle.CID);
        }

        if ((SD_CardType == SD_STD_CAPACITY_V1_1)    || (SD_CardType == SD_STD_CAPACITY_V2_0) ||
           (SD_CardType == SD_SECURE_DIGITAL_IO_COMBO) || (SD_CardType == SD_HIGH_CAPACITY))
        {
            if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_SET_REL_ADDR | SD_CMD_RESPONSE_SHORT), 0, 6)) != SD_OK)
            {
                return ErrorState;
            }
        }

        if (SD_CardType != SD_SECURE_DIGITAL_IO)
        {
            if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_SEND_CSD | SD_CMD_RESPONSE_LONG), SD_CardRCA, 2)) == SD_OK)
            {
                SD_GetResponse(SD_Handle.CSD);
            }
        }
    }
    else
    {
        ErrorState = SD_REQUEST_NOT_APPLICABLE;
    }

    return ErrorState;
}

/** -----------------------------------------------------------------------------------------------------------------*/
static bool SD_EnableDMAInterrupt(DMA_ARCH_TYPE *dmaChannel)
{
    xDMA_Cmd(dmaChannel, DISABLE);
    if (ft32DmaIsChannelEnabled(dmaChannel)) {
        return false;
    }

    xDMA_ITConfig(dmaChannel, DMA_IT_TFR, ENABLE);
    return true;
}

static void SD_StartBlockTransfer(uint32_t* pBuffer, uint32_t BlockSize, uint32_t NumberOfBlocks, uint8_t dir)
{
    // Complete rewrite for DesignWare DMA architecture
    SD_Handle.TransferComplete = 0;
    SD_Handle.TransferError    = SD_OK;
    SD_Handle.Operation        = (NumberOfBlocks > 1) ? SD_MULTIPLE_BLOCK : SD_SINGLE_BLOCK;
    SD_Handle.Operation       |= dir << 1;

    // Configure interrupts
    SDIO->INTMASK = 0;
    SDIO->CTRL |= SDIO_CTRL_INT_ENABLE;
    if (dir == SDIO_DIR_RX) {
        SDIO_ITConfig(SDIO_IT_MASK_DCRC | SDIO_IT_MASK_DTO | SDIO_IT_MASK_FRUN);
    } else {
        SDIO_ITConfig(SDIO_IT_MASK_DCRC | SDIO_IT_MASK_DTO | SDIO_IT_MASK_FRUN);
    }

    // Configure DMA transfer
    xDMA_Cmd(sdioDmaResource, DISABLE);
    if (ft32DmaIsChannelEnabled((DMA_ARCH_TYPE *)sdioDmaResource)) {
        SD_Handle.TransferError = SD_ERROR;
        return;
    }

    DMA_InitTypeDef DMA_InitStructure;
    DMA_StructInit(&DMA_InitStructure);

    // Use DMA spec from dma_reqmap for hardware interface
    if (dir == SDIO_DIR_RX) {
        DMA_InitStructure.TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_P2M_DMA;
        DMA_InitStructure.SrcAddrMode = DMA_SRC_ADDRMODE_HOLD;
        DMA_InitStructure.DstAddrMode = DMA_DST_ADDRMODE_INC;
    } else {
        DMA_InitStructure.TransferTypeFlowCtl = DMA_TRANSFERTYPE_FLOWCTL_M2P_DMA;
        DMA_InitStructure.SrcAddrMode = DMA_SRC_ADDRMODE_INC;
        DMA_InitStructure.DstAddrMode = DMA_DST_ADDRMODE_HOLD;
    }

    DMA_InitStructure.SrcDstMasterSel     = DMA_SRCMASTER1_DSTMASTER2;
    // SDIO FIFO at SDIO_DATA->DATA (SDIO_BASE + 0x200)
    DMA_InitStructure.SrcAddress          = (dir == SDIO_DIR_RX) ? (uint32_t)&SDIO_DATA->DATA : (uint32_t)pBuffer;
    DMA_InitStructure.DstAddress          = (dir == SDIO_DIR_RX) ? (uint32_t)pBuffer : (uint32_t)&SDIO_DATA->DATA;
    DMA_InitStructure.BlockTransSize      = (BlockSize * NumberOfBlocks) / 4;
    DMA_InitStructure.SrcTransferWidth    = DMA_SRC_TRANSFERWIDTH_32BITS;
    DMA_InitStructure.DstTransferWidth    = DMA_DST_TRANSFERWIDTH_32BITS;
    DMA_InitStructure.Priority            = DMA_CH_PRIORITY_7;

    if (dir == SDIO_DIR_RX) {
        ft32DmaSetSrcRequest(&DMA_InitStructure, sdioDmaResource, sdioDmaSpec->channel);
    } else {
        ft32DmaSetDstRequest(&DMA_InitStructure, sdioDmaResource, sdioDmaSpec->channel);
    }

    xDMA_Init(sdioDmaResource, &DMA_InitStructure);
    if (!SD_EnableDMAInterrupt((DMA_ARCH_TYPE *)sdioDmaResource)) {
        SD_Handle.TransferError = SD_ERROR;
        return;
    }
    xDMA_Cmd(sdioDmaResource, ENABLE);
    SDIO->CTRL |= SDIO_CTRL_DMA_ENABLE;
}

/** -----------------------------------------------------------------------------------------------------------------*/
SD_Error_t SD_ReadBlocks_DMA(uint64_t ReadAddress, uint32_t *buffer, uint32_t BlockSize, uint32_t NumberOfBlocks)
{
    SD_Error_t ErrorState;
    uint32_t   CmdIndex;
    SD_Handle.RXCplt = 1;

    if (SD_CardType != SD_HIGH_CAPACITY)
    {
        ReadAddress *= 512;
    }

    // Set up data transfer before command (DesignWare sequence)
    SD_StartBlockTransfer(buffer, BlockSize, NumberOfBlocks, SDIO_DIR_RX);

    SD_DataTransferInit(BlockSize * NumberOfBlocks, SDIO_DATABLOCK_SIZE_512B, true);

    ErrorState = SD_TransmitCommand((SDMMC_CMD_SET_BLOCKLEN | SD_CMD_RESPONSE_SHORT), BlockSize, 1);

    CmdIndex   = (NumberOfBlocks > 1) ? SDMMC_CMD_READ_MULT_BLOCK : SDMMC_CMD_READ_SINGLE_BLOCK;
    // Use SEND_AUTO_STOP for multi-block instead of manual STOP command
    uint32_t cmdFlags = SD_CMD_RESPONSE_SHORT | SD_CMD_CHECK_CRC | SD_CMD_DATA_EXPECTED_BIT;
    if (NumberOfBlocks > 1) {
        cmdFlags |= SD_CMD_SEND_AUTO_STOP_BIT;
    }

    uint8_t retries = 10;
    do {
        ErrorState = SD_TransmitCommand((CmdIndex | cmdFlags), (uint32_t)ReadAddress, 1);
        if (ErrorState != SD_OK && retries--) {
            SD_TransmitCommand((SDMMC_CMD_APP_CMD | SD_CMD_RESPONSE_SHORT), 0, 1);
        }
    } while (ErrorState != SD_OK && retries);

    if (ErrorState != SD_OK) {
        SD_Handle.RXCplt = 0;
    }

    SD_Handle.TransferError = ErrorState;

    return ErrorState;
}

/** -----------------------------------------------------------------------------------------------------------------*/
SD_Error_t SD_WriteBlocks_DMA(uint64_t WriteAddress, uint32_t *buffer, uint32_t BlockSize, uint32_t NumberOfBlocks)
{
    SD_Error_t ErrorState;
    uint32_t   CmdIndex;
    SD_Handle.TXCplt = 1;

    if (SD_CardType != SD_HIGH_CAPACITY)
    {
        WriteAddress *= 512;
    }

    CmdIndex = (NumberOfBlocks > 1) ? SDMMC_CMD_WRITE_MULT_BLOCK : SDMMC_CMD_WRITE_SINGLE_BLOCK;

    uint32_t cmdFlags = SD_CMD_RESPONSE_SHORT | SD_CMD_CHECK_CRC | SD_CMD_DATA_EXPECTED_BIT | SD_CMD_WRITE_BIT;
    if (NumberOfBlocks > 1) {
        cmdFlags |= SD_CMD_SEND_AUTO_STOP_BIT;
    }

    uint8_t retries = 10;
    do {
        ErrorState = SD_TransmitCommand((CmdIndex | cmdFlags), (uint32_t)WriteAddress, 1);
        if (ErrorState != SD_OK && retries--) {
            SD_TransmitCommand((SDMMC_CMD_APP_CMD | SD_CMD_RESPONSE_SHORT), 0, 1);
        }
    } while (ErrorState != SD_OK && retries);

    if (ErrorState != SD_OK) {
        SD_Handle.TXCplt = 0;
        return ErrorState;
    }

    SD_StartBlockTransfer(buffer, BlockSize, NumberOfBlocks, SDIO_DIR_TX);

    SD_DataTransferInit(BlockSize * NumberOfBlocks, SDIO_DATABLOCK_SIZE_512B, false);

    SD_Handle.TransferError = ErrorState;

    return ErrorState;
}

SD_Error_t SD_CheckWrite(void)
{
    if (SD_Handle.TXCplt != 0) return SD_BUSY;
    return SD_OK;
}

SD_Error_t SD_CheckRead(void)
{
    if (SD_Handle.RXCplt != 0) return SD_BUSY;
    return SD_OK;
}

/** -----------------------------------------------------------------------------------------------------------------*/
SD_Error_t SD_GetCardInfo(void)
{
    SD_Error_t ErrorState = SD_OK;
    uint32_t Temp = 0;

    Temp = (SD_Handle.CSD[0] & 0xFF000000) >> 24;
    SD_CardInfo.SD_csd.CSDStruct      = (uint8_t)((Temp & 0xC0) >> 6);
    SD_CardInfo.SD_csd.SysSpecVersion = (uint8_t)((Temp & 0x3C) >> 2);
    SD_CardInfo.SD_csd.Reserved1      = Temp & 0x03;

    Temp = (SD_Handle.CSD[0] & 0x00FF0000) >> 16;
    SD_CardInfo.SD_csd.TAAC = (uint8_t)Temp;

    Temp = (SD_Handle.CSD[0] & 0x0000FF00) >> 8;
    SD_CardInfo.SD_csd.NSAC = (uint8_t)Temp;

    Temp = SD_Handle.CSD[0] & 0x000000FF;
    SD_CardInfo.SD_csd.MaxBusClkFrec = (uint8_t)Temp;

    Temp = (SD_Handle.CSD[1] & 0xFF000000) >> 24;
    SD_CardInfo.SD_csd.CardComdClasses = (uint16_t)(Temp << 4);

    Temp = (SD_Handle.CSD[1] & 0x00FF0000) >> 16;
    SD_CardInfo.SD_csd.CardComdClasses |= (uint16_t)((Temp & 0xF0) >> 4);
    SD_CardInfo.SD_csd.RdBlockLen       = (uint8_t)(Temp & 0x0F);

    Temp = (SD_Handle.CSD[1] & 0x0000FF00) >> 8;
    SD_CardInfo.SD_csd.PartBlockRead   = (uint8_t)((Temp & 0x80) >> 7);
    SD_CardInfo.SD_csd.WrBlockMisalign = (uint8_t)((Temp & 0x40) >> 6);
    SD_CardInfo.SD_csd.RdBlockMisalign = (uint8_t)((Temp & 0x20) >> 5);
    SD_CardInfo.SD_csd.DSRImpl         = (uint8_t)((Temp & 0x10) >> 4);
    SD_CardInfo.SD_csd.Reserved2       = 0;

    if ((SD_CardType == SD_STD_CAPACITY_V1_1) || (SD_CardType == SD_STD_CAPACITY_V2_0))
    {
        SD_CardInfo.SD_csd.DeviceSize = (Temp & 0x03) << 10;

        Temp = (uint8_t)(SD_Handle.CSD[1] & 0x000000FF);
        SD_CardInfo.SD_csd.DeviceSize |= (Temp) << 2;

        Temp = (uint8_t)((SD_Handle.CSD[2] & 0xFF000000) >> 24);
        SD_CardInfo.SD_csd.DeviceSize |= (Temp & 0xC0) >> 6;

        SD_CardInfo.SD_csd.MaxRdCurrentVDDMin = (Temp & 0x38) >> 3;
        SD_CardInfo.SD_csd.MaxRdCurrentVDDMax = (Temp & 0x07);

        Temp = (uint8_t)((SD_Handle.CSD[2] & 0x00FF0000) >> 16);
        SD_CardInfo.SD_csd.MaxWrCurrentVDDMin = (Temp & 0xE0) >> 5;
        SD_CardInfo.SD_csd.MaxWrCurrentVDDMax = (Temp & 0x1C) >> 2;
        SD_CardInfo.SD_csd.DeviceSizeMul      = (Temp & 0x03) << 1;

        Temp = (uint8_t)((SD_Handle.CSD[2] & 0x0000FF00) >> 8);
        SD_CardInfo.SD_csd.DeviceSizeMul |= (Temp & 0x80) >> 7;

        SD_CardInfo.CardCapacity  = (SD_CardInfo.SD_csd.DeviceSize + 1) ;
        SD_CardInfo.CardCapacity *= (1 << (SD_CardInfo.SD_csd.DeviceSizeMul + 2));
        SD_CardInfo.CardBlockSize = 1 << (SD_CardInfo.SD_csd.RdBlockLen);
        SD_CardInfo.CardCapacity = SD_CardInfo.CardCapacity * SD_CardInfo.CardBlockSize / 512;
    }
    else if (SD_CardType == SD_HIGH_CAPACITY)
    {
        Temp = (uint8_t)(SD_Handle.CSD[1] & 0x000000FF);
        SD_CardInfo.SD_csd.DeviceSize = (Temp & 0x3F) << 16;

        Temp = (uint8_t)((SD_Handle.CSD[2] & 0xFF000000) >> 24);
        SD_CardInfo.SD_csd.DeviceSize |= (Temp << 8);

        Temp = (uint8_t)((SD_Handle.CSD[2] & 0x00FF0000) >> 16);
        SD_CardInfo.SD_csd.DeviceSize |= (Temp);

        Temp = (uint8_t)((SD_Handle.CSD[2] & 0x0000FF00) >> 8);

        SD_CardInfo.CardCapacity  = ((uint64_t)SD_CardInfo.SD_csd.DeviceSize + 1) * 1024;
        SD_CardInfo.CardBlockSize = 512;
    }
    else
    {
        ErrorState = SD_ERROR;
    }

    SD_CardInfo.SD_csd.EraseGrSize = (Temp & 0x40) >> 6;
    SD_CardInfo.SD_csd.EraseGrMul  = (Temp & 0x3F) << 1;

    Temp = (uint8_t)(SD_Handle.CSD[2] & 0x000000FF);
    SD_CardInfo.SD_csd.EraseGrMul     |= (Temp & 0x80) >> 7;
    SD_CardInfo.SD_csd.WrProtectGrSize = (Temp & 0x7F);

    Temp = (uint8_t)((SD_Handle.CSD[3] & 0xFF000000) >> 24);
    SD_CardInfo.SD_csd.WrProtectGrEnable = (Temp & 0x80) >> 7;
    SD_CardInfo.SD_csd.ManDeflECC        = (Temp & 0x60) >> 5;
    SD_CardInfo.SD_csd.WrSpeedFact       = (Temp & 0x1C) >> 2;
    SD_CardInfo.SD_csd.MaxWrBlockLen     = (Temp & 0x03) << 2;

    Temp = (uint8_t)((SD_Handle.CSD[3] & 0x00FF0000) >> 16);
    SD_CardInfo.SD_csd.MaxWrBlockLen      |= (Temp & 0xC0) >> 6;
    SD_CardInfo.SD_csd.WriteBlockPaPartial = (Temp & 0x20) >> 5;
    SD_CardInfo.SD_csd.Reserved3           = 0;
    SD_CardInfo.SD_csd.ContentProtectAppli = (Temp & 0x01);

    Temp = (uint8_t)((SD_Handle.CSD[3] & 0x0000FF00) >> 8);
    SD_CardInfo.SD_csd.FileFormatGrouop = (Temp & 0x80) >> 7;
    SD_CardInfo.SD_csd.CopyFlag         = (Temp & 0x40) >> 6;
    SD_CardInfo.SD_csd.PermWrProtect    = (Temp & 0x20) >> 5;
    SD_CardInfo.SD_csd.TempWrProtect    = (Temp & 0x10) >> 4;
    SD_CardInfo.SD_csd.FileFormat       = (Temp & 0x0C) >> 2;
    SD_CardInfo.SD_csd.ECC              = (Temp & 0x03);

    Temp = (uint8_t)(SD_Handle.CSD[3] & 0x000000FF);
    SD_CardInfo.SD_csd.CSD_CRC   = (Temp & 0xFE) >> 1;
    SD_CardInfo.SD_csd.Reserved4 = 1;

    Temp = (uint8_t)((SD_Handle.CID[0] & 0xFF000000) >> 24);
    SD_CardInfo.SD_cid.ManufacturerID = Temp;

    Temp = (uint8_t)((SD_Handle.CID[0] & 0x00FF0000) >> 16);
    SD_CardInfo.SD_cid.OEM_AppliID = Temp << 8;

    Temp = (uint8_t)((SD_Handle.CID[0] & 0x0000FF00) >> 8);
    SD_CardInfo.SD_cid.OEM_AppliID |= Temp;

    Temp = (uint8_t)(SD_Handle.CID[0] & 0x000000FF);
    SD_CardInfo.SD_cid.ProdName1 = Temp << 24;

    Temp = (uint8_t)((SD_Handle.CID[1] & 0xFF000000) >> 24);
    SD_CardInfo.SD_cid.ProdName1 |= Temp << 16;

    Temp = (uint8_t)((SD_Handle.CID[1] & 0x00FF0000) >> 16);
    SD_CardInfo.SD_cid.ProdName1 |= Temp << 8;

    Temp = (uint8_t)((SD_Handle.CID[1] & 0x0000FF00) >> 8);
    SD_CardInfo.SD_cid.ProdName1 |= Temp;

    Temp = (uint8_t)(SD_Handle.CID[1] & 0x000000FF);
    SD_CardInfo.SD_cid.ProdName2 = Temp;

    Temp = (uint8_t)((SD_Handle.CID[2] & 0xFF000000) >> 24);
    SD_CardInfo.SD_cid.ProdRev = Temp;

    Temp = (uint8_t)((SD_Handle.CID[2] & 0x00FF0000) >> 16);
    SD_CardInfo.SD_cid.ProdSN = Temp << 24;

    Temp = (uint8_t)((SD_Handle.CID[2] & 0x0000FF00) >> 8);
    SD_CardInfo.SD_cid.ProdSN |= Temp << 16;

    Temp = (uint8_t)(SD_Handle.CID[2] & 0x000000FF);
    SD_CardInfo.SD_cid.ProdSN |= Temp << 8;

    Temp = (uint8_t)((SD_Handle.CID[3] & 0xFF000000) >> 24);
    SD_CardInfo.SD_cid.ProdSN |= Temp;

    Temp = (uint8_t)((SD_Handle.CID[3] & 0x00FF0000) >> 16);
    SD_CardInfo.SD_cid.Reserved1   |= (Temp & 0xF0) >> 4;
    SD_CardInfo.SD_cid.ManufactDate = (Temp & 0x0F) << 8;

    Temp = (uint8_t)((SD_Handle.CID[3] & 0x0000FF00) >> 8);
    SD_CardInfo.SD_cid.ManufactDate |= Temp;

    Temp = (uint8_t)(SD_Handle.CID[3] & 0x000000FF);
    SD_CardInfo.SD_cid.CID_CRC   = (Temp & 0xFE) >> 1;
    SD_CardInfo.SD_cid.Reserved2 = 1;

    return ErrorState;
}

/** -----------------------------------------------------------------------------------------------------------------*/
static SD_Error_t SD_WideBusOperationConfig(uint32_t WideMode)
{
    SD_Error_t ErrorState = SD_OK;
    uint32_t   Temp;
    uint32_t   SCR[2] = {0, 0};

    if ((SD_CardType == SD_STD_CAPACITY_V1_1) || (SD_CardType == SD_STD_CAPACITY_V2_0) ||
            (SD_CardType == SD_HIGH_CAPACITY))
    {
        if (WideMode == SDIO_CARD_WIDE_8B)
        {
            ErrorState = SD_UNSUPPORTED_FEATURE;
        }
        else if ((WideMode == SDIO_CARD_WIDE_4B) || (WideMode == SDIO_CARD_WIDE_1B))
        {
            if ((SDIO->RESP0 & SD_CARD_LOCKED) != SD_CARD_LOCKED)
            {
                ErrorState = SD_FindSCR(SCR);
                if (ErrorState == SD_OK)
                {
                    Temp = (WideMode == SDIO_CARD_WIDE_4B) ? SD_WIDE_BUS_SUPPORT : SD_SINGLE_BUS_SUPPORT;

                    if ((SCR[1] & Temp) != SD_ALLZERO)
                    {
                        ErrorState = SD_TransmitCommand((SDMMC_CMD_APP_CMD | SD_CMD_RESPONSE_SHORT), SD_CardRCA, 1);
                        if (ErrorState == SD_OK)
                        {
                            Temp = (WideMode == SDIO_CARD_WIDE_4B) ? 2 : 0;
                            ErrorState = SD_TransmitCommand((SDMMC_CMD_APP_SD_SET_BUSWIDTH | SD_CMD_RESPONSE_SHORT), Temp, 1);
                        }
                    }
                    else
                    {
                        ErrorState = SD_REQUEST_NOT_APPLICABLE;
                    }
                }
            }
            else
            {
                ErrorState = SD_LOCK_UNLOCK_FAILED;
            }
        }
        else
        {
            ErrorState = SD_INVALID_PARAMETER;
        }

        if (ErrorState == SD_OK)
        {
            // Use SDIO_CardWidthConfig instead of MODIFY_REG(SDIO->CLKCR)
            SDIO_CardWidthConfig(WideMode);
        }
    }
    else {
        ErrorState = SD_UNSUPPORTED_FEATURE;
    }

    return ErrorState;
}

/** -----------------------------------------------------------------------------------------------------------------*/
static SD_Error_t SD_HighSpeed(void)
{
    SD_Error_t  ErrorState;
    uint8_t     SD_hs[64]  = {0};
    uint32_t    SD_scr[2]  = {0, 0};
    uint32_t    SD_SPEC    = 0;
    uint32_t*   Buffer     = (uint32_t *)SD_hs;

    if ((ErrorState = SD_FindSCR(SD_scr)) != SD_OK)
    {
        return ErrorState;
    }

    SD_SPEC = (SD_scr[1]  & 0x01000000) | (SD_scr[1]  & 0x02000000);

    if (SD_SPEC != SD_ALLZERO)
    {
        if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_SET_BLOCKLEN | SD_CMD_RESPONSE_SHORT), 64, 1)) != SD_OK)
        {
            return ErrorState;
        }

        SD_DataTransferInit(64, SDIO_DATABLOCK_SIZE_64B, true);

        if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_HS_SWITCH | SD_CMD_RESPONSE_SHORT | SD_CMD_DATA_EXPECTED_BIT), 0x80FFFF01, 1)) != SD_OK)
        {
            return ErrorState;
        }

        ErrorState = SD_ReadFifo(Buffer, 16);
        if (ErrorState != SD_OK) {
            return ErrorState;
        }

        if ((SD_hs[13] & 2) != 2)
        {
            ErrorState = SD_UNSUPPORTED_FEATURE;
        }
    }

    return ErrorState;
}

/** -----------------------------------------------------------------------------------------------------------------*/
static SD_Error_t SD_GetStatus(void)
{
    SD_Error_t     ErrorState;
    uint32_t       Response1;
    SD_CardState_t CardState;

    if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_SEND_STATUS | SD_CMD_RESPONSE_SHORT), SD_CardRCA, 1)) == SD_OK)
    {
        Response1 = SDIO->RESP0;
        CardState = (SD_CardState_t)((Response1 >> 9) & 0x0F);

        if     (CardState == SD_CARD_TRANSFER)  ErrorState = SD_OK;
        else if(CardState == SD_CARD_ERROR)     ErrorState = SD_ERROR;
        else                                    ErrorState = SD_BUSY;
    }
    else
    {
        ErrorState = SD_ERROR;
    }

    return ErrorState;
}

/** -----------------------------------------------------------------------------------------------------------------*/
SD_Error_t SD_GetCardStatus(SD_CardStatus_t* pCardStatus)
{
    SD_Error_t ErrorState;
    uint32_t   Temp = 0;
    uint32_t   Status[16];

    if ((SDIO->RESP0 & SD_CARD_LOCKED) == SD_CARD_LOCKED)
    {
        return SD_LOCK_UNLOCK_FAILED;
    }

    if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_SET_BLOCKLEN | SD_CMD_RESPONSE_SHORT), 64, 1)) != SD_OK)
    {
        return ErrorState;
    }

    if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_APP_CMD | SD_CMD_RESPONSE_SHORT), SD_CardRCA, 1)) != SD_OK)
    {
        return ErrorState;
    }

    SD_DataTransferInit(64, SDIO_DATABLOCK_SIZE_64B, true);

    if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_SD_APP_STATUS | SD_CMD_RESPONSE_SHORT | SD_CMD_DATA_EXPECTED_BIT), 0, 1)) != SD_OK)
    {
        return ErrorState;
    }

    ErrorState = SD_ReadFifo(Status, 16);
    if (ErrorState != SD_OK) {
        return ErrorState;
    }

    Temp = (Status[0] & 0xC0) >> 6;
    pCardStatus->DAT_BUS_WIDTH = (uint8_t)Temp;

    Temp = (Status[0] & 0x20) >> 5;
    pCardStatus->SECURED_MODE = (uint8_t)Temp;

    Temp = (Status[2] & 0xFF);
    pCardStatus->SD_CARD_TYPE = (uint8_t)(Temp << 8);

    Temp = (Status[3] & 0xFF);
    pCardStatus->SD_CARD_TYPE |= (uint8_t)Temp;

    Temp = (Status[4] & 0xFF);
    pCardStatus->SIZE_OF_PROTECTED_AREA = (uint8_t)(Temp << 24);

    Temp = (Status[5] & 0xFF);
    pCardStatus->SIZE_OF_PROTECTED_AREA |= (uint8_t)(Temp << 16);

    Temp = (Status[6] & 0xFF);
    pCardStatus->SIZE_OF_PROTECTED_AREA |= (uint8_t)(Temp << 8);

    Temp = (Status[7] & 0xFF);
    pCardStatus->SIZE_OF_PROTECTED_AREA |= (uint8_t)Temp;

    Temp = (Status[8] & 0xFF);
    pCardStatus->SPEED_CLASS = (uint8_t)Temp;

    Temp = (Status[9] & 0xFF);
    pCardStatus->PERFORMANCE_MOVE = (uint8_t)Temp;

    Temp = (Status[10] & 0xF0) >> 4;
    pCardStatus->AU_SIZE = (uint8_t)Temp;

    Temp = (Status[11] & 0xFF);
    pCardStatus->ERASE_SIZE = (uint8_t)(Temp << 8);

    Temp = (Status[12] & 0xFF);
    pCardStatus->ERASE_SIZE |= (uint8_t)Temp;

    Temp = (Status[13] & 0xFC) >> 2;
    pCardStatus->ERASE_TIMEOUT = (uint8_t)Temp;

    Temp = (Status[13] & 0x3);
    pCardStatus->ERASE_OFFSET = (uint8_t)Temp;

    return SD_OK;
}

/** -----------------------------------------------------------------------------------------------------------------*/
static SD_Error_t SD_PowerON(void)
{
    SD_Error_t ErrorState;
    uint32_t   Response = 0;
    uint32_t   Count;
    uint32_t   ValidVoltage;
    uint32_t   SD_Type;

    Count        = 0;
    ValidVoltage = 0;
    SD_Type      = SD_RESP_STD_CAPACITY;

    // Power ON sequence using DesignWare registers
    // Disable clock
    SDIO->CLKENA &= ~SDIO_CLKENA_CCLK_ENABLE_0;
    // Set power state to ON
    SDIO_PowerEnableConfig(SDIO_POWER_ON_ENABLE);
    delay(2);
    // Enable clock
    SDIO->CLKENA |= SDIO_CLKENA_CCLK_ENABLE_0;

    // CMD0: GO_IDLE_STATE
    if ((ErrorState = SD_TransmitCommand(SDMMC_CMD_GO_IDLE_STATE, 0, 0)) != SD_OK)
    {
        return ErrorState;
    }

    // CMD8: SEND_IF_COND
    if ((ErrorState = SD_TransmitCommand((SD_SDIO_SEND_IF_COND | SD_CMD_RESPONSE_SHORT), SD_CHECK_PATTERN, 7)) == SD_OK)
    {
        SD_CardType = SD_STD_CAPACITY_V2_0;
        SD_Type     = SD_RESP_HIGH_CAPACITY;
    }

    // CMD55
    if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_APP_CMD | SD_CMD_RESPONSE_SHORT), 0, 1)) == SD_OK)
    {
        while ((ValidVoltage == 0) && (Count < SD_MAX_VOLT_TRIAL))
        {
            if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_APP_CMD | SD_CMD_RESPONSE_SHORT), 0, 1)) != SD_OK)
            {
                return ErrorState;
            }

            if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_SD_APP_OP_COND | SD_CMD_RESPONSE_SHORT), SD_VOLTAGE_WINDOW_SD | SD_Type, 3)) != SD_OK)
            {
                return ErrorState;
            }

            // Read response from RESP0 instead of RESP1
            Response = SDIO->RESP0;
            ValidVoltage = (((Response >> 31) == 1) ? 1 : 0);
            Count++;
        }

        if (Count >= SD_MAX_VOLT_TRIAL)
        {
            return SD_INVALID_VOLTRANGE;
        }

        if ((Response & SD_RESP_HIGH_CAPACITY) == SD_RESP_HIGH_CAPACITY)
        {
            SD_CardType = SD_HIGH_CAPACITY;
        }
    }

    return ErrorState;
}

/** -----------------------------------------------------------------------------------------------------------------*/
static SD_Error_t SD_FindSCR(uint32_t *pSCR)
{
    SD_Error_t ErrorState;
    uint32_t tempscr[2] = {0, 0};

    if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_SET_BLOCKLEN | SD_CMD_RESPONSE_SHORT), 8, 1)) == SD_OK)
    {
        if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_APP_CMD | SD_CMD_RESPONSE_SHORT), SD_CardRCA, 1)) == SD_OK)
        {
            SD_DataTransferInit(8, SDIO_DATABLOCK_SIZE_8B, true);

            if ((ErrorState = SD_TransmitCommand((SDMMC_CMD_SD_APP_SEND_SCR | SD_CMD_RESPONSE_SHORT | SD_CMD_DATA_EXPECTED_BIT), 0, 1)) == SD_OK)
            {
                ErrorState = SD_ReadFifo(tempscr, 2);
                if (ErrorState == SD_OK) {
                    *(pSCR + 1) = ((tempscr[0] & SD_0TO7BITS) << 24)  | ((tempscr[0] & SD_8TO15BITS) << 8) |
                                  ((tempscr[0] & SD_16TO23BITS) >> 8) | ((tempscr[0] & SD_24TO31BITS) >> 24);

                    *(pSCR) = ((tempscr[1] & SD_0TO7BITS) << 24)  | ((tempscr[1] & SD_8TO15BITS) << 8) |
                              ((tempscr[1] & SD_16TO23BITS) >> 8) | ((tempscr[1] & SD_24TO31BITS) >> 24);
                }
            }
        }
    }

    return ErrorState;
}

/** -----------------------------------------------------------------------------------------------------------------*/
bool SD_InitialiseHardware(dmaResource_t *dma)
{
    UNUSED(dma);
    // Complete rewrite for FT32 DesignWare SDIO + DesignWare DMA

    // Get DMA spec from request map
    sdioDmaSpec = dmaGetChannelSpecByPeripheral(DMA_PERIPH_SDIO, 0, sdioConfig()->dmaopt);
    if (!sdioDmaSpec) {
        return false;
    }

    const dmaIdentifier_e dmaIdentifier = dmaGetIdentifier(sdioDmaSpec->ref);
    if (!dmaAllocate(dmaIdentifier, OWNER_SDCARD, 0)) {
        return false;
    }

    sdioDmaResource = sdioDmaSpec->ref;

    // Reset SDIO Module
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_SDIO, ENABLE);
    delay(1);
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_SDIO, DISABLE);
    delay(1);

    // Enable SDIO clock
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SDIO, ENABLE);

    // Enable DMA2 clocks
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

    // Configure GPIO pins
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOD, ENABLE);

    uint8_t is4BitWidth = sdioConfig()->use4BitWidth;

    const IO_t d0 = IOGetByTag(IO_TAG(PC8));
    const IO_t d1 = IOGetByTag(IO_TAG(PC9));
    const IO_t d2 = IOGetByTag(IO_TAG(PC10));
    const IO_t d3 = IOGetByTag(IO_TAG(PC11));
    const IO_t clk = IOGetByTag(IO_TAG(PC12));
    const IO_t cmd = IOGetByTag(IO_TAG(PD2));

    IOInit(d0, OWNER_SDCARD, 0);
    if (is4BitWidth) {
        IOInit(d1, OWNER_SDCARD, 0);
        IOInit(d2, OWNER_SDCARD, 0);
        IOInit(d3, OWNER_SDCARD, 0);
    }
    IOInit(clk, OWNER_SDCARD, 0);
    IOInit(cmd, OWNER_SDCARD, 0);

    // SDIO uses GPIO_AF_6 for alternate function mapping
    IOConfigGPIOAF(d0, IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_100MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL), GPIO_AF_6);
    if (is4BitWidth) {
        IOConfigGPIOAF(d1, IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_100MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL), GPIO_AF_6);
        IOConfigGPIOAF(d2, IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_100MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL), GPIO_AF_6);
        IOConfigGPIOAF(d3, IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_100MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL), GPIO_AF_6);
    }
    IOConfigGPIOAF(clk, IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_100MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL), GPIO_AF_6);
    IOConfigGPIOAF(cmd, IO_CONFIG(GPIO_Mode_AF, GPIO_Speed_100MHz, GPIO_OType_PP, GPIO_PuPd_NOPULL), GPIO_AF_6);

    // NVIC configuration for SDIO interrupts
    // SDIO_IRQn vector number is 48
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = SDIO_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = NVIC_PRIORITY_BASE(1);
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = NVIC_PRIORITY_SUB(0);
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // Configure DMA
    dmaEnable(dmaIdentifier);
    dmaSetHandler(dmaIdentifier, SDIO_DMA_IRQHandler, NVIC_PRIORITY_BASE(1), 0);

    // Set FIFO threshold for DMA operation
    // FIFO depth = 32 words. RX watermark triggers when > RX_WMark words in FIFO.
    // TX watermark triggers when <= TX_WMark words in FIFO.
    // RX_WMark = 15 (trigger at 16+ words), TX_WMark = 16 (trigger at 16- words)
    SDIO_FifoThConfig((15UL << SDIO_FIFOTH_RX_WMark_Pos) | (16UL << SDIO_FIFOTH_TX_WMark_Pos));

    return true;
}

/** -----------------------------------------------------------------------------------------------------------------*/
bool SD_GetState(void)
{
    if (SD_GetStatus() == SD_OK) return true;
    return false;
}

/** -----------------------------------------------------------------------------------------------------------------*/
static SD_Error_t SD_DoInit(void)
{
    SD_Error_t errorState;

    // Set initial clock divider via SDIO_ChangeCardClock
    SDIO_ChangeCardClock(SDIO_INIT_CLK_DIV);

    errorState = SD_PowerON();
    if (errorState != SD_OK) {
        return errorState;
    }

    errorState = SD_InitializeCard();
    if (errorState != SD_OK) {
        return errorState;
    }

    errorState = SD_GetCardInfo();
    if (errorState != SD_OK) {
        return errorState;
    }

    // Select the Card
    errorState = SD_TransmitCommand((SDMMC_CMD_SEL_DESEL_CARD | SD_CMD_RESPONSE_SHORT), SD_CardRCA, 1);

    // Change to normal clock speed via SDIO_ChangeCardClock
    SDIO_ChangeCardClock(SDIO_CLK_DIV);

    // Configure SD Bus width
    if (errorState == SD_OK)
    {
        if (sdioConfig()->use4BitWidth) {
            errorState = SD_WideBusOperationConfig(SDIO_CARD_WIDE_4B);
        } else {
            errorState = SD_WideBusOperationConfig(SDIO_CARD_WIDE_1B);
        }
        if (errorState == SD_OK && sdioConfig()->clockBypass) {
            if (SD_HighSpeed()) {
                // No clock bypass in DesignWare, use minimum divider
                // DIV=1 => 105M/(2*2) = 26.25MHz, within HS 50MHz limit
                SDIO_ChangeCardClock(1);
            }
        }
    }

    return errorState;
}

SD_Error_t SD_Init(void)
{
    static bool sdInitAttempted = false;
    static SD_Error_t result = SD_ERROR;

    if (sdInitAttempted) {
        return result;
    }

    sdInitAttempted = true;

    result = SD_DoInit();

    return result;
}

/** -----------------------------------------------------------------------------------------------------------------*/
void SDIO_IRQHandler(void)
{
    // Use MINTSTS for masked interrupt status, RINTSTS for clearing
    if (SDIO_GetITFlag(SDIO_IT_FLAG_DTO)) {
        SDIO_ClearITFlag(SDIO_IT_CLEAN_DTO);
        SDIO->RINTSTS = 0xFFFFFFFF;

        // Disable data-related interrupts
        SDIO->INTMASK &= ~(SDIO_IT_MASK_DTO | SDIO_IT_MASK_DCRC | SDIO_IT_MASK_DRTO |
                           SDIO_IT_MASK_FRUN | SDIO_IT_MASK_TXDR | SDIO_IT_MASK_RXDR);

        if ((SD_Handle.Operation & 0x02) == (SDIO_DIR_TX << 1)) {
            xDMA_Cmd(sdioDmaResource, DISABLE);
            SDIO->CTRL &= ~SDIO_CTRL_DMA_ENABLE;
            SD_Handle.TXCplt = 0;
            if ((SD_Handle.Operation & 0x01) == SD_MULTIPLE_BLOCK) {
                SD_TransmitCommand((SDMMC_CMD_STOP_TRANSMISSION | SD_CMD_RESPONSE_SHORT), 0, 1);
            }
        }
        SD_Handle.TransferComplete = 1;
        SD_Handle.TransferError = SD_OK;
    }
    else if (SDIO_GetITFlag(SDIO_IT_FLAG_DCRC))
        SD_Handle.TransferError = SD_DATA_CRC_FAIL;
    else if (SDIO_GetITFlag(SDIO_IT_FLAG_DRTO))
        SD_Handle.TransferError = SD_DATA_TIMEOUT;
    else if (SDIO_GetITFlag(SDIO_IT_FLAG_FRUN))
        SD_Handle.TransferError = SD_RX_OVERRUN;

    SDIO->RINTSTS = 0xFFFFFFFF;

    // Disable all SDIO interrupt sources
    SDIO->INTMASK = 0;
}

/** -----------------------------------------------------------------------------------------------------------------*/
// Single DMA IRQ handler for DesignWare DMA architecture
// Uses DMA_GET_FLAG_STATUS/DMA_CLEAR_FLAG instead of direct register access
void SDIO_DMA_IRQHandler(dmaChannelDescriptor_t *descriptor)
{
    if (DMA_GET_FLAG_STATUS(descriptor, DMA_IT_ERR)) {
        DMA_CLEAR_FLAG(descriptor, DMA_IT_ERR);
    }

    if (DMA_GET_FLAG_STATUS(descriptor, DMA_IT_TCIF)) {
        DMA_CLEAR_FLAG(descriptor, DMA_IT_TCIF);
        SD_DMA_Complete(descriptor);
    }
}

/** -----------------------------------------------------------------------------------------------------------------*/
bool SD_IsDetected(void)
{
    return true;
}

/** -----------------------------------------------------------------------------------------------------------------*/
#endif
