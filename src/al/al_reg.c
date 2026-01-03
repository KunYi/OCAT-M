/**
 * @file al_reg.c
 * @brief Application Layer Register Access and Sync Manager Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "al_internal.h"
#include "ethercat/dll.h"
#include "ethercat/frame_builder.h"
#include "ethercat/frame.h"
#include <string.h>

/* ========================================================================== */
/* Register Access Functions                                                 */
/* ========================================================================== */

al_status_t al_read_reg8(uint16_t slave_address, uint16_t offset, uint8_t* value)
{
    if (value == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Use DLL FPRD command to read register */
    uint8_t frame_buffer[ECAT_MAX_FRAME_SIZE];
    ecat_frame_builder_t builder;

    /* Build frame */
    uint8_t src_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer,
                                                  sizeof(frame_buffer),
                                                  src_mac, dst_mac);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Add FPRD datagram */
    uint32_t address = ecat_addr_configured(slave_address, offset);
    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_FPRD, 0,
                                              address, NULL, 1, false);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Finalize frame */
    uint16_t frame_length;
    status = ecat_frame_builder_finalize(&builder, &frame_length);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Send frame via DLL */
    dl_send_req_t req = {
        .frame_data = frame_buffer,
        .frame_length = frame_length,
        .priority = 0,
        .user_data = NULL
    };

    status = dl_send_req(&req);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* TODO: Wait for response and extract value */
    /* For now, return success */
    *value = 0;

    return AL_STATUS_SUCCESS;
}

al_status_t al_write_reg8(uint16_t slave_address, uint16_t offset, uint8_t value)
{
    /* Use DLL FPWR command to write register */
    uint8_t frame_buffer[ECAT_MAX_FRAME_SIZE];
    ecat_frame_builder_t builder;

    /* Build frame */
    uint8_t src_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer,
                                                  sizeof(frame_buffer),
                                                  src_mac, dst_mac);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Add FPWR datagram */
    uint32_t address = ecat_addr_configured(slave_address, offset);
    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_FPWR, 0,
                                              address, &value, 1, false);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Finalize frame */
    uint16_t frame_length;
    status = ecat_frame_builder_finalize(&builder, &frame_length);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Send frame via DLL */
    dl_send_req_t req = {
        .frame_data = frame_buffer,
        .frame_length = frame_length,
        .priority = 0,
        .user_data = NULL
    };

    status = dl_send_req(&req);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    return AL_STATUS_SUCCESS;
}

al_status_t al_read_reg16(uint16_t slave_address, uint16_t offset, uint16_t* value)
{
    if (value == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Read as 2 bytes */
    uint8_t data[2];
    al_status_t status = al_read_block(slave_address, offset, data, 2);
    if (status != AL_STATUS_SUCCESS) {
        return status;
    }

    /* Convert to uint16_t (little-endian) */
    *value = (uint16_t)data[0] | ((uint16_t)data[1] << 8);

    return AL_STATUS_SUCCESS;
}

al_status_t al_write_reg16(uint16_t slave_address, uint16_t offset, uint16_t value)
{
    /* Convert to bytes (little-endian) */
    uint8_t data[2];
    data[0] = (uint8_t)(value & 0xFF);
    data[1] = (uint8_t)((value >> 8) & 0xFF);

    return al_write_block(slave_address, offset, data, 2);
}

al_status_t al_read_reg32(uint16_t slave_address, uint16_t offset, uint32_t* value)
{
    if (value == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Read as 4 bytes */
    uint8_t data[4];
    al_status_t status = al_read_block(slave_address, offset, data, 4);
    if (status != AL_STATUS_SUCCESS) {
        return status;
    }

    /* Convert to uint32_t (little-endian) */
    *value = (uint32_t)data[0] |
             ((uint32_t)data[1] << 8) |
             ((uint32_t)data[2] << 16) |
             ((uint32_t)data[3] << 24);

    return AL_STATUS_SUCCESS;
}

al_status_t al_write_reg32(uint16_t slave_address, uint16_t offset, uint32_t value)
{
    /* Convert to bytes (little-endian) */
    uint8_t data[4];
    data[0] = (uint8_t)(value & 0xFF);
    data[1] = (uint8_t)((value >> 8) & 0xFF);
    data[2] = (uint8_t)((value >> 16) & 0xFF);
    data[3] = (uint8_t)((value >> 24) & 0xFF);

    return al_write_block(slave_address, offset, data, 4);
}

al_status_t al_read_block(uint16_t slave_address, uint16_t offset, uint8_t* data, uint16_t length)
{
    if (data == NULL || length == 0) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Use DLL FPRD command to read block */
    uint8_t frame_buffer[ECAT_MAX_FRAME_SIZE];
    ecat_frame_builder_t builder;

    /* Build frame */
    uint8_t src_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer,
                                                  sizeof(frame_buffer),
                                                  src_mac, dst_mac);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Add FPRD datagram */
    uint32_t address = ecat_addr_configured(slave_address, offset);
    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_FPRD, 0,
                                              address, NULL, length, false);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Finalize frame */
    uint16_t frame_length;
    status = ecat_frame_builder_finalize(&builder, &frame_length);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Send frame via DLL */
    dl_send_req_t req = {
        .frame_data = frame_buffer,
        .frame_length = frame_length,
        .priority = 0,
        .user_data = NULL
    };

    status = dl_send_req(&req);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* TODO: Wait for response and extract data */
    /* For now, zero the buffer */
    memset(data, 0, length);

    return AL_STATUS_SUCCESS;
}

al_status_t al_write_block(uint16_t slave_address, uint16_t offset, const uint8_t* data, uint16_t length)
{
    if (data == NULL || length == 0) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Use DLL FPWR command to write block */
    uint8_t frame_buffer[ECAT_MAX_FRAME_SIZE];
    ecat_frame_builder_t builder;

    /* Build frame */
    uint8_t src_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer,
                                                  sizeof(frame_buffer),
                                                  src_mac, dst_mac);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Add FPWR datagram */
    uint32_t address = ecat_addr_configured(slave_address, offset);
    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_FPWR, 0,
                                              address, data, length, false);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Finalize frame */
    uint16_t frame_length;
    status = ecat_frame_builder_finalize(&builder, &frame_length);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Send frame via DLL */
    dl_send_req_t req = {
        .frame_data = frame_buffer,
        .frame_length = frame_length,
        .priority = 0,
        .user_data = NULL
    };

    status = dl_send_req(&req);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    return AL_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Sync Manager Functions                                                    */
/* ========================================================================== */

al_status_t al_sm_config(uint16_t slave_address,
                          uint8_t sm_index,
                          const sm_config_t* config)
{
    if (config == NULL || sm_index >= SM_MAX_COUNT) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Calculate SM register address */
    uint16_t sm_addr = SM_CONFIG_BASE_ADDR + (sm_index * SM_CONFIG_SIZE);

    /* Write SM configuration */
    return al_write_block(slave_address, sm_addr, (const uint8_t*)config, sizeof(sm_config_t));
}

al_status_t al_sm_read_config(uint16_t slave_address,
                               uint8_t sm_index,
                               sm_config_t* config)
{
    if (config == NULL || sm_index >= SM_MAX_COUNT) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Calculate SM register address */
    uint16_t sm_addr = SM_CONFIG_BASE_ADDR + (sm_index * SM_CONFIG_SIZE);

    /* Read SM configuration */
    return al_read_block(slave_address, sm_addr, (uint8_t*)config, sizeof(sm_config_t));
}

al_status_t al_sm_enable(uint16_t slave_address, uint8_t sm_index)
{
    if (sm_index >= SM_MAX_COUNT) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Calculate SM enable register address */
    uint16_t enable_addr = SM_CONFIG_BASE_ADDR + (sm_index * SM_CONFIG_SIZE) + 6;

    /* Write enable bit */
    return al_write_reg8(slave_address, enable_addr, 0x01);
}

al_status_t al_sm_disable(uint16_t slave_address, uint8_t sm_index)
{
    if (sm_index >= SM_MAX_COUNT) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Calculate SM enable register address */
    uint16_t enable_addr = SM_CONFIG_BASE_ADDR + (sm_index * SM_CONFIG_SIZE) + 6;

    /* Write disable bit */
    return al_write_reg8(slave_address, enable_addr, 0x00);
}

/* ========================================================================== */
/* SII (EEPROM) Functions                                                    */
/* ========================================================================== */

al_status_t al_sii_read(uint16_t slave_address,
                         uint16_t offset,
                         uint16_t* data,
                         uint16_t length)
{
    if (data == NULL || length == 0) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Suppress unused parameter warnings */
    (void)slave_address;
    (void)offset;

    /* SII read is not implemented in this basic version */
    /* TODO: Implement SII read via EEPROM control/status registers */

    return AL_STATUS_NOT_SUPPORTED;
}

al_status_t al_sii_write(uint16_t slave_address,
                          uint16_t offset,
                          const uint16_t* data,
                          uint16_t length)
{
    if (data == NULL || length == 0) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Suppress unused parameter warnings */
    (void)slave_address;
    (void)offset;

    /* SII write is not implemented in this basic version */
    /* TODO: Implement SII write via EEPROM control/status registers */

    return AL_STATUS_NOT_SUPPORTED;
}
