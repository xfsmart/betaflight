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

// I2C bus timeout: ~7.5us at 210MHz
#define I2C_TIMEOUT                      0x627

// FT32F4 uses a single IRQ vector for both event and error interrupts
// According to spec: I2C1_IRQn (IRQ 31), I2C2_IRQn (IRQ 32), I2C3_IRQn (IRQ 65)
// merge event and error handling into one IRQ handler

#ifdef USE_I2C_DEVICE_1
void I2C1_IRQHandler(void)
{
    i2c_handle_type *hi2c = &i2cDevice[I2CDEV_1].halHandle->hal;
    
    // FT32F4: Single IRQ handles both event and error interrupts
    i2c_evt_irq_handler(hi2c);
    i2c_err_irq_handler(hi2c);
}
#endif

#ifdef USE_I2C_DEVICE_2
void I2C2_IRQHandler(void)
{
    i2c_handle_type *hi2c = &i2cDevice[I2CDEV_2].halHandle->hal;
    
    // FT32F4: Single IRQ handles both event and error interrupts
    i2c_evt_irq_handler(hi2c);
    i2c_err_irq_handler(hi2c);
}
#endif

#ifdef USE_I2C_DEVICE_3
void I2C3_IRQHandler(void)
{
    i2c_handle_type *hi2c = &i2cDevice[I2CDEV_3].halHandle->hal;
    
    // FT32F4: Single IRQ handles both event and error interrupts
    i2c_evt_irq_handler(hi2c);
    i2c_err_irq_handler(hi2c);
}
#endif

static volatile uint16_t i2cErrorCount = 0;

// Handle I2C hardware failure and increment error counter
// device: I2C device identifier
// Returns: false to indicate operation failed
static bool i2cHandleHardwareFailure(i2cDevice_e device)
{
    i2cErrorCount++;
    
    i2c_handle_type *pHandle = &i2cDevice[device].halHandle->hal;
    if (!pHandle->i2cx) {
        return false;
    }
    
    // Clear all error flags (BERR, ARLO, OVR, NACKF)
    I2C_ClearFlag(pHandle->i2cx, I2C_FLAG_BERR | I2C_FLAG_ARLO | I2C_FLAG_OVR | I2C_FLAG_NACKF);
    
    // Attempt bus recovery: generate STOP condition to release bus
    I2C_GenerateSTOP(pHandle->i2cx, ENABLE);
    
    // Wait for STOPF flag to be set (STOP condition completed)
    // Uses Betaflight timeout pattern: microsISR() + cmpTimeUs()
    i2c_wait_flag(pHandle, I2C_STOPF_FLAG, I2C_EVENT_CHECK_NONE, I2C_TIMEOUT);
    
    // Clear STOPF flag
    i2c_flag_clear(pHandle->i2cx, I2C_STOPF_FLAG);
    
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
    if (device == I2CINVALID || device >= I2CDEV_COUNT) {
        return false;
    }

    i2c_handle_type *pHandle = &i2cDevice[device].halHandle->hal;

    if (!pHandle->i2cx) {
        return false;
    }

    i2c_status_type status;

    if (reg_ == 0xFF) {
        status = i2c_master_transmit(pHandle, addr_ << 1, &data, 1, I2C_TIMEOUT);

        if (status != I2C_OK) {
            i2c_wait_flag(pHandle, I2C_STOPF_FLAG, I2C_EVENT_CHECK_NONE, I2C_TIMEOUT);
            i2c_flag_clear(pHandle->i2cx, I2C_STOPF_FLAG);
        }
    } else {
        status = i2c_memory_write(pHandle, I2C_MEM_ADDR_WIDIH_8, addr_ << 1, reg_, &data, 1, I2C_TIMEOUT_US);

        if(status !=  I2C_OK) {
            i2c_wait_flag(pHandle, I2C_STOPF_FLAG, I2C_EVENT_CHECK_NONE, I2C_TIMEOUT);
            i2c_flag_clear(pHandle->i2cx, I2C_STOPF_FLAG);
        }
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
    if (device == I2CINVALID || device >= I2CDEV_COUNT) {
        return false;
    }

    i2c_handle_type *pHandle = &i2cDevice[device].halHandle->hal;

    if (!pHandle->i2cx) {
        return false;
    }

    i2c_status_type status;
    status = i2c_memory_write_int(pHandle, I2C_MEM_ADDR_WIDIH_8, addr_ << 1, reg_,data, len_, I2C_TIMEOUT);

    if (status != I2C_OK) {
        i2c_wait_flag(pHandle, I2C_STOPF_FLAG, I2C_EVENT_CHECK_NONE, I2C_TIMEOUT);
        i2c_flag_clear(pHandle->i2cx, I2C_STOPF_FLAG);
    }

    if (status == I2C_ERR_STEP_1) {
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
    if (device == I2CINVALID || device >= I2CDEV_COUNT) {
        return false;
    }

    i2c_handle_type *pHandle = &i2cDevice[device].halHandle->hal;

    if (!pHandle->i2cx) {
        return false;
    }

    i2c_status_type status;

    if (reg_ == 0xFF) {
        status = i2c_master_receive(pHandle ,addr_ << 1 , buf, len, I2C_TIMEOUT);

        if (status !=  I2C_OK) {
            i2c_wait_flag(pHandle, I2C_STOPF_FLAG, I2C_EVENT_CHECK_NONE, I2C_TIMEOUT);
            i2c_flag_clear(pHandle->i2cx, I2C_STOPF_FLAG);
        }
    } else {
        status = i2c_memory_read(pHandle, I2C_MEM_ADDR_WIDIH_8, addr_ << 1, reg_, buf, len, I2C_TIMEOUT);

        if (status !=  I2C_OK) {
            i2c_wait_flag(pHandle, I2C_STOPF_FLAG, I2C_EVENT_CHECK_NONE, I2C_TIMEOUT);
            i2c_flag_clear(pHandle->i2cx, I2C_STOPF_FLAG);
        }
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
    if (device == I2CINVALID || device >= I2CDEV_COUNT) {
        return false;
    }

    i2c_handle_type *pHandle = &i2cDevice[device].halHandle->hal;

    if (!pHandle->i2cx) {
        return false;
    }

    i2c_status_type status;

    status = i2c_memory_read_int(pHandle, I2C_MEM_ADDR_WIDIH_8, addr_ << 1, reg_, buf, len, I2C_TIMEOUT);

    if (status !=  I2C_OK) {
        i2c_wait_flag(pHandle, I2C_STOPF_FLAG, I2C_EVENT_CHECK_NONE, I2C_TIMEOUT);
        i2c_flag_clear(pHandle->i2cx, I2C_STOPF_FLAG);
    }

    if (status == I2C_ERR_STEP_1) {
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
// Returns: true if bus is busy or in error state, false if ready
bool i2cBusy(i2cDevice_e device, bool *error)
{
    i2c_handle_type *pHandle = &i2cDevice[device].halHandle->hal;

    if (error) {
        *error = pHandle->error_code;
    }

    // I2C_ERR_ACKFAIL indicates that the last access wasn't acknowledged, but doesn't mean the bus is busy
    if ((pHandle->error_code == I2C_OK) || (pHandle->error_code == I2C_ERR_ACKFAIL)) {
        if (i2c_flag_get(pHandle->i2cx, I2C_BUSYF_FLAG) == SET) {
            return true;
        }
        return false;
    }

    return true;
}

#endif
