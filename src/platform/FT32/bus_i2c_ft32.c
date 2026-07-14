/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later
 * version.
 *
 * Betaflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#if defined(USE_I2C) && !defined(USE_SOFT_I2C)

#include "drivers/io.h"
#include "drivers/io_impl.h"
#include "drivers/nvic.h"
#include "drivers/time.h"
#include "platform/rcc.h"

#include "drivers/bus_i2c.h"
#include "drivers/bus_i2c_impl.h"

static volatile uint16_t i2cErrorCount;
static volatile bool i2cAsyncErrorCounted[I2CDEV_COUNT];
static volatile bool i2cRecoveryPending[I2CDEV_COUNT];
static volatile bool i2cAsyncWatchdogArmed[I2CDEV_COUNT];
static volatile uint32_t i2cAsyncProgressAt[I2CDEV_COUNT];

bool i2cReinit(i2cDevice_e device);

static i2c_handle_type *i2cGetStoredHandle(i2cDevice_e device)
{
    if ((unsigned)device >= I2CDEV_COUNT) {
        return NULL;
    }

    const i2cDevice_t *pDev = &i2cDevice[device];
    const i2cHardware_t *hardware = pDev->hardware;
    if (!hardware || hardware->device != device || !hardware->reg || pDev->reg != hardware->reg || !pDev->halHandle) {
        return NULL;
    }

    return &pDev->halHandle->hal;
}

static i2c_handle_type *i2cGetHandle(i2cDevice_e device)
{
    i2c_handle_type *pHandle = i2cGetStoredHandle(device);
    if (!pHandle || pHandle->i2cx != (I2C_TypeDef *)i2cDevice[device].hardware->reg) {
        return NULL;
    }

    return pHandle;
}

static void i2cRecordAsyncError(i2cDevice_e device, const i2c_handle_type *pHandle)
{
    if (pHandle->error_code != I2C_OK && !i2cAsyncErrorCounted[device]) {
        i2cErrorCount++;
        i2cAsyncErrorCounted[device] = true;
    }
}

static bool i2cCheckAsyncTimeout(i2cDevice_e device, i2c_handle_type *pHandle)
{
    if (!i2cAsyncWatchdogArmed[device] || pHandle->state == I2C_END || pHandle->error_code != I2C_OK) {
        i2cAsyncWatchdogArmed[device] = false;
        return false;
    }

    if ((int32_t)cmpTimeUs(microsISR(), i2cAsyncProgressAt[device]) < (int32_t)I2C_TIMEOUT_US) {
        return false;
    }

    const IRQn_Type irqn = (IRQn_Type)i2cDevice[device].hardware->irqn;
    NVIC_DisableIRQ(irqn);

    if (!i2cAsyncWatchdogArmed[device] || pHandle->state == I2C_END || pHandle->error_code != I2C_OK ||
        (int32_t)cmpTimeUs(microsISR(), i2cAsyncProgressAt[device]) < (int32_t)I2C_TIMEOUT_US) {
        NVIC_EnableIRQ(irqn);
        return false;
    }

    i2cAsyncWatchdogArmed[device] = false;
    pHandle->error_code = I2C_ERR_TIMEOUT;
    pHandle->state = I2C_END;
    i2cRecoveryPending[device] = true;
    return true;
}

static bool i2cTryRecovery(i2cDevice_e device)
{
    if ((unsigned)device >= I2CDEV_COUNT) {
        return false;
    }

    i2c_handle_type *pHandle = i2cGetStoredHandle(device);
    if (!pHandle) {
        return false;
    }

    if (!i2cRecoveryPending[device] && pHandle->i2cx) {
        return true;
    }

    i2cRecoveryPending[device] = true;

    if (!i2cReinit(device)) {
        return false;
    }

    i2cAsyncWatchdogArmed[device] = false;
    i2cRecoveryPending[device] = false;
    return true;
}

static i2c_handle_type *i2cPrepareHandle(i2cDevice_e device)
{
    i2c_handle_type *pHandle = i2cGetStoredHandle(device);
    if (!pHandle) {
        return NULL;
    }

    i2cCheckAsyncTimeout(device, pHandle);
    i2cRecordAsyncError(device, pHandle);
    if (!i2cTryRecovery(device)) {
        return NULL;
    }

    pHandle = i2cGetHandle(device);
    return pHandle && pHandle->state == I2C_END ? pHandle : NULL;
}

static void i2cIrqHandler(i2cDevice_e device)
{
    i2c_handle_type *pHandle = i2cGetHandle(device);
    if (!pHandle) {
        return;
    }

    const uint16_t regCount = pHandle->pcount[I2C_STEP_REG];
    const uint16_t dataCount = pHandle->pcount[I2C_STEP_DATA];
    const uint16_t transferSize = pHandle->psize;
    const i2cStep_t step = pHandle->step;
    const bool transferComplete = I2C_GetFlagStatus(pHandle->i2cx, I2C_FLAG_TC) == SET;

    i2c_err_irq_handler(pHandle);
    if (pHandle->state != I2C_END) {
        i2c_evt_irq_handler(pHandle);
    }

    if (pHandle->state == I2C_END || pHandle->error_code != I2C_OK) {
        i2cAsyncWatchdogArmed[device] = false;
    } else if (regCount != pHandle->pcount[I2C_STEP_REG] || dataCount != pHandle->pcount[I2C_STEP_DATA] ||
        transferSize != pHandle->psize || step != pHandle->step || transferComplete) {
        i2cAsyncProgressAt[device] = microsISR();
    }

    if (pHandle->error_code == I2C_ERR_ACKFAIL && pHandle->state == I2C_END) {
        i2cRecoveryPending[device] = false;
    } else if (pHandle->error_code != I2C_OK) {
        i2cRecoveryPending[device] = true;
    }
}

#ifdef USE_I2C_DEVICE_1
void I2C1_IRQHandler(void)
{
    i2cIrqHandler(I2CDEV_1);
}
#endif

#ifdef USE_I2C_DEVICE_2
void I2C2_IRQHandler(void)
{
    i2cIrqHandler(I2CDEV_2);
}
#endif

#ifdef USE_I2C_DEVICE_3
void I2C3_IRQHandler(void)
{
    i2cIrqHandler(I2CDEV_3);
}
#endif

static bool i2cHandleHardwareFailure(i2cDevice_e device)
{
    i2c_handle_type *pHandle = i2cGetHandle(device);
    if (!pHandle) {
        return false;
    }

    i2cErrorCount++;
    i2cAsyncErrorCounted[device] = true;
    i2cAsyncWatchdogArmed[device] = false;
    pHandle->state = I2C_END;

    if (pHandle->error_code == I2C_ERR_ACKFAIL && I2C_GetFlagStatus(pHandle->i2cx, I2C_FLAG_BUSY) == RESET) {
        I2C_ITConfig(pHandle->i2cx, I2C_IT_ERRI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI | I2C_IT_ADDRI | I2C_IT_TXI | I2C_IT_RXI, DISABLE);
        I2C_ClearFlag(pHandle->i2cx, I2C_FLAG_NACKF | I2C_FLAG_STOPF);
        i2c_reset_ctrl2_register(pHandle);
        i2cRecoveryPending[device] = false;
    } else {
        i2cRecoveryPending[device] = true;
        i2cTryRecovery(device);
    }

    return false;
}

uint16_t i2cGetErrorCounter(void)
{
    return i2cErrorCount;
}

// Write single byte to I2C device
// device: I2C device identifier
// addr_: 7-bit device address (will be shifted left by 1)
// reg_: Register address (0xFF for no register address, direct transmit)
// data: Data byte to write
// Returns: true on success, false on failure
bool i2cWrite(i2cDevice_e device, uint8_t addr_, uint8_t reg_, uint8_t data)
{
    i2c_handle_type *pHandle = i2cPrepareHandle(device);
    if (!pHandle) {
        return false;
    }

    i2c_status_type status;

    if (reg_ == 0xFF) {
        status = i2c_master_transmit(pHandle, addr_ << 1, &data, 1, I2C_TIMEOUT_US);
    } else {
        status = i2c_memory_write(pHandle, I2C_MEM_ADDR_WIDIH_8, addr_ << 1, reg_, &data, 1, I2C_TIMEOUT_US);
    }

    if (status != I2C_OK) {
        return i2cHandleHardwareFailure(device);
    }

    return true;
}

// Write data buffer to I2C device
// device: I2C device identifier
// addr_: 7-bit device address (will be shifted left by 1)
// reg_: Register start address
// len_: Number of bytes to write
// data: Pointer to data buffer
// Returns: true on success, false on failure
bool i2cWriteBuffer(i2cDevice_e device, uint8_t addr_, uint8_t reg_, uint8_t len_, uint8_t *data)
{
    if (!data || !len_) {
        return false;
    }

    i2c_handle_type *pHandle = i2cPrepareHandle(device);
    if (!pHandle || i2c_flag_get(pHandle->i2cx, I2C_BUSYF_FLAG) == SET) {
        return false;
    }

    i2cAsyncErrorCounted[device] = false;
    i2cRecoveryPending[device] = false;
    i2cAsyncProgressAt[device] = microsISR();
    i2cAsyncWatchdogArmed[device] = true;
    const i2c_status_type status = i2c_memory_write_int(pHandle, I2C_MEM_ADDR_WIDIH_8, addr_ << 1, reg_, data, len_, I2C_TIMEOUT_US);

    if (status == I2C_ERR_STEP_1) {
        i2cAsyncWatchdogArmed[device] = false;
        return false;
    }

    if (status != I2C_OK) {
        return i2cHandleHardwareFailure(device);
    }

    return true;
}

// Read data from I2C device
// device: I2C device identifier
// addr_: 7-bit device address (will be shifted left by 1)
// reg_: Register start address (0xFF for no register address, direct receive)
// len: Number of bytes to read
// buf: Pointer to destination buffer
// Returns: true on success, false on failure
bool i2cRead(i2cDevice_e device, uint8_t addr_, uint8_t reg_, uint8_t len, uint8_t* buf)
{
    if (!buf || !len) {
        return false;
    }

    i2c_handle_type *pHandle = i2cPrepareHandle(device);
    if (!pHandle) {
        return false;
    }

    i2c_status_type status;

    if (reg_ == 0xFF) {
        status = i2c_master_receive(pHandle, addr_ << 1, buf, len, I2C_TIMEOUT_US);
    } else {
        status = i2c_memory_read(pHandle, I2C_MEM_ADDR_WIDIH_8, addr_ << 1, reg_, buf, len, I2C_TIMEOUT_US);
    }

    if (status != I2C_OK) {
        return i2cHandleHardwareFailure(device);
    }

    return true;
}

// Read data buffer from I2C device using interrupt-driven transfer
// device: I2C device identifier
// addr_: 7-bit device address (will be shifted left by 1)
// reg_: Register start address
// len: Number of bytes to read
// buf: Pointer to destination buffer
// Returns: true on success, false on failure
bool i2cReadBuffer(i2cDevice_e device, uint8_t addr_, uint8_t reg_, uint8_t len, uint8_t* buf)
{
    if (!buf || !len) {
        return false;
    }

    i2c_handle_type *pHandle = i2cPrepareHandle(device);
    if (!pHandle || i2c_flag_get(pHandle->i2cx, I2C_BUSYF_FLAG) == SET) {
        return false;
    }

    i2cAsyncErrorCounted[device] = false;
    i2cRecoveryPending[device] = false;
    i2cAsyncProgressAt[device] = microsISR();
    i2cAsyncWatchdogArmed[device] = true;
    const i2c_status_type status = i2c_memory_read_int(pHandle, I2C_MEM_ADDR_WIDIH_8, addr_ << 1, reg_, buf, len, I2C_TIMEOUT_US);

    if (status == I2C_ERR_STEP_1) {
        i2cAsyncWatchdogArmed[device] = false;
        return false;
    }

    if (status != I2C_OK) {
        return i2cHandleHardwareFailure(device);
    }

    return true;
}

// Check if I2C bus is busy
// device: I2C device identifier
// error: Optional pointer to store error code (can be NULL)
// Returns: true while the transfer state or hardware bus is busy
bool i2cBusy(i2cDevice_e device, bool *error)
{
    i2c_handle_type *pHandle = i2cGetStoredHandle(device);
    if (!pHandle) {
        if (error) {
            *error = true;
        }
        return false;
    }

    i2cCheckAsyncTimeout(device, pHandle);
    const bool transferError = pHandle->error_code != I2C_OK;
    i2cRecordAsyncError(device, pHandle);

    if (error) {
        *error = transferError;
    }

    if (!i2cTryRecovery(device)) {
        pHandle = i2cGetHandle(device);
        return pHandle && (pHandle->state != I2C_END || i2c_flag_get(pHandle->i2cx, I2C_BUSYF_FLAG) == SET);
    }

    pHandle = i2cGetHandle(device);
    if (!pHandle) {
        return false;
    }

    return pHandle->state != I2C_END || i2c_flag_get(pHandle->i2cx, I2C_BUSYF_FLAG) == SET;
}

#endif
