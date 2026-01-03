/**
 * @file al_mailbox.c
 * @brief Application Layer Mailbox Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "al_internal.h"
#include <string.h>
#include <stdlib.h>

/* ========================================================================== */
/* Mailbox Initialization and Shutdown                                       */
/* ========================================================================== */

al_status_t al_mailbox_init(al_slave_context_t* slave)
{
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Default mailbox configuration (will be read from SII later) */
    slave->mbx_write_offset = 0x1000;
    slave->mbx_write_size = 128;
    slave->mbx_read_offset = 0x1080;
    slave->mbx_read_size = 128;

    /* Allocate mailbox buffers */
    slave->mbx_send_buffer = (uint8_t*)malloc(slave->mbx_write_size);
    if (slave->mbx_send_buffer == NULL) {
        return AL_STATUS_NO_MEMORY;
    }

    slave->mbx_recv_buffer = (uint8_t*)malloc(slave->mbx_read_size);
    if (slave->mbx_recv_buffer == NULL) {
        free(slave->mbx_send_buffer);
        slave->mbx_send_buffer = NULL;
        return AL_STATUS_NO_MEMORY;
    }

    slave->mbx_send_length = 0;
    slave->mbx_recv_length = 0;
    slave->mbx_state = MBOX_STATE_IDLE;

    /* Default protocol support (will be read from SII later) */
    slave->supports_coe = true;
    slave->supports_foe = true;
    slave->supports_soe = false;
    slave->supports_voe = false;
    slave->supports_eoe = false;
    slave->supports_aoe = false;

    return AL_STATUS_SUCCESS;
}

void al_mailbox_shutdown(al_slave_context_t* slave)
{
    if (slave == NULL) {
        return;
    }

    /* Free mailbox buffers */
    if (slave->mbx_send_buffer != NULL) {
        free(slave->mbx_send_buffer);
        slave->mbx_send_buffer = NULL;
    }

    if (slave->mbx_recv_buffer != NULL) {
        free(slave->mbx_recv_buffer);
        slave->mbx_recv_buffer = NULL;
    }

    slave->mbx_send_length = 0;
    slave->mbx_recv_length = 0;
    slave->mbx_state = MBOX_STATE_IDLE;
}

/* ========================================================================== */
/* Mailbox Communication Functions                                           */
/* ========================================================================== */

al_status_t al_mailbox_send(const mbx_send_req_t* req)
{
    if (req == NULL || req->data == NULL || req->length == 0) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Get slave context */
    al_slave_context_t* slave = al_get_slave_context(req->slave_address);
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Check if mailbox is idle */
    if (slave->mbx_state != MBOX_STATE_IDLE) {
        return AL_STATUS_BUSY;
    }

    /* Check buffer size */
    uint16_t total_length = MAILBOX_HEADER_SIZE + req->length;
    if (total_length > slave->mbx_write_size) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Build mailbox header */
    mailbox_header_t* header = (mailbox_header_t*)slave->mbx_send_buffer;
    header->length = req->length;
    header->address = req->slave_address;
    header->channel = 0;
    header->priority = req->priority & 0x03;
    header->type = req->type;

    /* Copy data */
    memcpy(slave->mbx_send_buffer + MAILBOX_HEADER_SIZE, req->data, req->length);
    slave->mbx_send_length = total_length;

    /* Write to mailbox */
    al_status_t status = al_mailbox_write(slave, slave->mbx_send_buffer, total_length);
    if (status != AL_STATUS_SUCCESS) {
        return status;
    }

    slave->mbx_state = MBOX_STATE_WRITE_IN_PROGRESS;

    return AL_STATUS_SUCCESS;
}

al_status_t al_mailbox_check(uint16_t slave_address, bool* available)
{
    if (available == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Get slave context */
    al_slave_context_t* slave = al_get_slave_context(slave_address);
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Check SM1 status (mailbox read) */
    sm_config_t sm_config;
    al_status_t status = al_sm_read_config(slave_address, 1, &sm_config);
    if (status != AL_STATUS_SUCCESS) {
        return status;
    }

    /* Check if data is available (bit 3 of status register) */
    *available = (sm_config.status & 0x08) != 0;

    return AL_STATUS_SUCCESS;
}

al_status_t al_mailbox_receive(uint16_t slave_address,
                                mailbox_type_t* type,
                                uint8_t* data,
                                uint16_t* length)
{
    if (type == NULL || data == NULL || length == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Get slave context */
    al_slave_context_t* slave = al_get_slave_context(slave_address);
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Check if data is available */
    bool available;
    al_status_t status = al_mailbox_check(slave_address, &available);
    if (status != AL_STATUS_SUCCESS) {
        return status;
    }

    if (!available) {
        return AL_STATUS_BUSY;
    }

    /* Read from mailbox */
    uint16_t recv_length = slave->mbx_read_size;
    status = al_mailbox_read(slave, slave->mbx_recv_buffer, &recv_length);
    if (status != AL_STATUS_SUCCESS) {
        return status;
    }

    /* Parse mailbox header */
    if (recv_length < MAILBOX_HEADER_SIZE) {
        return AL_STATUS_ERROR;
    }

    mailbox_header_t* header = (mailbox_header_t*)slave->mbx_recv_buffer;
    *type = (mailbox_type_t)header->type;

    /* Copy data */
    uint16_t data_length = header->length;
    if (data_length > (*length)) {
        data_length = *length;
    }

    memcpy(data, slave->mbx_recv_buffer + MAILBOX_HEADER_SIZE, data_length);
    *length = data_length;

    return AL_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Mailbox Low-Level Functions                                               */
/* ========================================================================== */

al_status_t al_mailbox_write(al_slave_context_t* slave, const uint8_t* data, uint16_t length)
{
    if (slave == NULL || data == NULL || length == 0) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Write data to mailbox write SM (SM0) */
    al_status_t status = al_write_block(slave->station_address,
                                        slave->mbx_write_offset,
                                        data,
                                        length);
    if (status != AL_STATUS_SUCCESS) {
        return status;
    }

    /* TODO: Set mailbox full flag in SM0 control register */

    return AL_STATUS_SUCCESS;
}

al_status_t al_mailbox_read(al_slave_context_t* slave, uint8_t* data, uint16_t* length)
{
    if (slave == NULL || data == NULL || length == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Read data from mailbox read SM (SM1) */
    al_status_t status = al_read_block(slave->station_address,
                                       slave->mbx_read_offset,
                                       data,
                                       *length);
    if (status != AL_STATUS_SUCCESS) {
        return status;
    }

    /* TODO: Clear mailbox full flag in SM1 control register */

    return AL_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Mailbox State Machine Processing                                          */
/* ========================================================================== */

al_status_t al_mailbox_process(al_slave_context_t* slave)
{
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    switch (slave->mbx_state) {
        case MBOX_STATE_IDLE:
            /* Check for incoming messages */
            {
                bool available;
                al_status_t status = al_mailbox_check(slave->station_address, &available);
                if (status == AL_STATUS_SUCCESS && available) {
                    slave->mbx_state = MBOX_STATE_READ_REQUESTED;
                }
            }
            break;

        case MBOX_STATE_WRITE_REQUESTED:
            /* Transition to write in progress */
            slave->mbx_state = MBOX_STATE_WRITE_IN_PROGRESS;
            break;

        case MBOX_STATE_WRITE_IN_PROGRESS:
            /* Check if write completed */
            /* TODO: Check SM0 status */
            slave->mbx_state = MBOX_STATE_IDLE;
            break;

        case MBOX_STATE_READ_REQUESTED:
            /* Transition to read in progress */
            slave->mbx_state = MBOX_STATE_READ_IN_PROGRESS;
            break;

        case MBOX_STATE_READ_IN_PROGRESS:
            /* Read mailbox data */
            {
                mailbox_type_t type;
                uint8_t data[1024];
                uint16_t length = sizeof(data);

                al_status_t status = al_mailbox_receive(slave->station_address,
                                                        &type, data, &length);
                if (status == AL_STATUS_SUCCESS) {
                    /* Trigger mailbox receive callback */
                    /* TODO: Implement callback trigger */
                    slave->mbx_state = MBOX_STATE_IDLE;
                } else if (status != AL_STATUS_BUSY) {
                    slave->mbx_state = MBOX_STATE_ERROR;
                }
            }
            break;

        case MBOX_STATE_ERROR:
            /* Reset to idle */
            slave->mbx_state = MBOX_STATE_IDLE;
            break;

        default:
            slave->mbx_state = MBOX_STATE_IDLE;
            break;
    }

    return AL_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Mailbox Protocol Support                                                  */
/* ========================================================================== */

const char* al_mailbox_get_protocol_name(mailbox_type_t type)
{
    switch (type) {
        case MBOX_TYPE_ERROR: return "Error";
        case MBOX_TYPE_AOE:   return "AoE (ADS over EtherCAT)";
        case MBOX_TYPE_EOE:   return "EoE (Ethernet over EtherCAT)";
        case MBOX_TYPE_COE:   return "CoE (CANopen over EtherCAT)";
        case MBOX_TYPE_FOE:   return "FoE (File over EtherCAT)";
        case MBOX_TYPE_SOE:   return "SoE (Servo over EtherCAT)";
        case MBOX_TYPE_VOE:   return "VoE (Vendor specific)";
        default:              return "Unknown";
    }
}

al_status_t al_mailbox_check_protocol_support(uint16_t slave_address,
                                               mailbox_type_t type,
                                               bool* supported)
{
    if (supported == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Get slave context */
    al_slave_context_t* slave = al_get_slave_context(slave_address);
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Check protocol support */
    switch (type) {
        case MBOX_TYPE_COE:
            *supported = slave->supports_coe;
            break;
        case MBOX_TYPE_FOE:
            *supported = slave->supports_foe;
            break;
        case MBOX_TYPE_SOE:
            *supported = slave->supports_soe;
            break;
        case MBOX_TYPE_VOE:
            *supported = slave->supports_voe;
            break;
        case MBOX_TYPE_EOE:
            *supported = slave->supports_eoe;
            break;
        case MBOX_TYPE_AOE:
            *supported = slave->supports_aoe;
            break;
        default:
            *supported = false;
            break;
    }

    return AL_STATUS_SUCCESS;
}
