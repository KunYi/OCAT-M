/**
 * @file foe.c
 * @brief File over EtherCAT (FoE) Protocol Implementation
 *
 * This file implements the FoE protocol as defined in ETG1000.6 specification.
 * FoE provides file transfer capabilities for firmware updates and data exchange.
 *
 * @author EtherCAT Master Stack
 * @date 2026-01-04
 * @version 1.0.0
 */

#include "ethercat/foe.h"
#include "ethercat/al.h"
#include "ethercat/al_types.h"
#include "ethercat/hal.h"
#include <string.h>
#include <stdio.h>

/* ========================================================================== */
/*                             Internal Structures                            */
/* ========================================================================== */

/**
 * @brief FoE transfer state
 */
typedef enum {
    FOE_STATE_IDLE,                     /**< Idle, no transfer in progress */
    FOE_STATE_WAIT_FOR_ACK,             /**< Waiting for ACK */
    FOE_STATE_WAIT_FOR_DATA,            /**< Waiting for DATA */
    FOE_STATE_SENDING_DATA,             /**< Sending DATA packets */
    FOE_STATE_RECEIVING_DATA,           /**< Receiving DATA packets */
    FOE_STATE_COMPLETED,                /**< Transfer completed */
    FOE_STATE_ERROR                     /**< Error occurred */
} foe_state_t;

/**
 * @brief FoE transfer context
 */
typedef struct {
    foe_state_t state;                  /**< Current transfer state */
    uint16_t slave_address;             /**< Slave station address */
    uint32_t packet_number;             /**< Current packet number */
    uint32_t total_bytes;               /**< Total bytes to transfer */
    uint32_t transferred_bytes;         /**< Bytes transferred so far */
    uint64_t start_time_ms;             /**< Transfer start time */
    uint32_t timeout_ms;                /**< Transfer timeout */
    foe_progress_callback_t progress_cb; /**< Progress callback */
    void* user_data;                    /**< User data for callback */
    foe_error_code_t last_error;        /**< Last error code from slave */
} foe_transfer_context_t;

/* ========================================================================== */
/*                          Internal Helper Functions                         */
/* ========================================================================== */

/**
 * @brief Build FoE READ request
 */
static foe_status_t foe_build_read_request(uint16_t slave_address __attribute__((unused)),
                                            const char* filename,
                                            uint8_t* buffer,
                                            uint16_t* length)
{
    if (!filename || !buffer || !length) {
        return FOE_STATUS_INVALID_PARAM;
    }

    size_t filename_len = strlen(filename);
    if (filename_len == 0 || filename_len >= FOE_MAX_FILENAME_LENGTH) {
        return FOE_STATUS_INVALID_PARAM;
    }

    /* Build FoE header */
    foe_header_t* header = (foe_header_t*)buffer;
    header->opcode = FOE_OPCODE_READ;
    header->reserved = 0;
    header->password = 0;  /* No password for read */

    /* Append filename */
    memcpy(buffer + FOE_HEADER_SIZE, filename, filename_len + 1);

    *length = FOE_HEADER_SIZE + filename_len + 1;
    return FOE_STATUS_SUCCESS;
}

/**
 * @brief Build FoE WRITE request
 */
static foe_status_t foe_build_write_request(uint16_t slave_address __attribute__((unused)),
                                             const char* filename,
                                             uint8_t* buffer,
                                             uint16_t* length)
{
    if (!filename || !buffer || !length) {
        return FOE_STATUS_INVALID_PARAM;
    }

    size_t filename_len = strlen(filename);
    if (filename_len == 0 || filename_len >= FOE_MAX_FILENAME_LENGTH) {
        return FOE_STATUS_INVALID_PARAM;
    }

    /* Build FoE header */
    foe_header_t* header = (foe_header_t*)buffer;
    header->opcode = FOE_OPCODE_WRITE;
    header->reserved = 0;
    header->password = 0;  /* No password for write */

    /* Append filename */
    memcpy(buffer + FOE_HEADER_SIZE, filename, filename_len + 1);

    *length = FOE_HEADER_SIZE + filename_len + 1;
    return FOE_STATUS_SUCCESS;
}

/**
 * @brief Build FoE DATA packet
 */
static foe_status_t foe_build_data_packet(uint32_t packet_number,
                                           const uint8_t* data,
                                           uint16_t data_size,
                                           uint8_t* buffer,
                                           uint16_t* length)
{
    if (!data || !buffer || !length || data_size > FOE_MAX_DATA_SIZE) {
        return FOE_STATUS_INVALID_PARAM;
    }

    /* Build FoE header */
    foe_header_t* header = (foe_header_t*)buffer;
    header->opcode = FOE_OPCODE_DATA;
    header->reserved = 0;
    header->packet_number = packet_number;

    /* Append data */
    memcpy(buffer + FOE_HEADER_SIZE, data, data_size);

    *length = FOE_HEADER_SIZE + data_size;
    return FOE_STATUS_SUCCESS;
}

/**
 * @brief Build FoE ACK packet
 */
static foe_status_t foe_build_ack_packet(uint32_t packet_number,
                                          uint8_t* buffer,
                                          uint16_t* length)
{
    if (!buffer || !length) {
        return FOE_STATUS_INVALID_PARAM;
    }

    /* Build FoE header */
    foe_header_t* header = (foe_header_t*)buffer;
    header->opcode = FOE_OPCODE_ACK;
    header->reserved = 0;
    header->packet_number = packet_number;

    *length = FOE_HEADER_SIZE;
    return FOE_STATUS_SUCCESS;
}

/**
 * @brief Parse FoE response
 */
static foe_status_t foe_parse_response(const uint8_t* buffer,
                                        uint16_t length,
                                        foe_opcode_t* opcode,
                                        uint32_t* packet_number,
                                        const uint8_t** data,
                                        uint16_t* data_size,
                                        foe_error_code_t* error_code)
{
    if (!buffer || length < FOE_HEADER_SIZE || !opcode) {
        return FOE_STATUS_INVALID_PARAM;
    }

    const foe_header_t* header = (const foe_header_t*)buffer;
    *opcode = (foe_opcode_t)header->opcode;

    switch (*opcode) {
        case FOE_OPCODE_ACK:
            if (packet_number) {
                *packet_number = header->packet_number;
            }
            break;

        case FOE_OPCODE_DATA:
            if (packet_number) {
                *packet_number = header->packet_number;
            }
            if (data && data_size) {
                *data = buffer + FOE_HEADER_SIZE;
                *data_size = length - FOE_HEADER_SIZE;
            }
            break;

        case FOE_OPCODE_BUSY:
            /* Slave is busy, retry later */
            break;

        case FOE_OPCODE_ERROR:
            if (error_code) {
                *error_code = (foe_error_code_t)header->error_code;
            }
            return FOE_STATUS_ERROR;

        default:
            return FOE_STATUS_ERROR;
    }

    return FOE_STATUS_SUCCESS;
}

/**
 * @brief Send FoE mailbox message
 */
static foe_status_t foe_send_mailbox(uint16_t slave_address,
                                      const uint8_t* data,
                                      uint16_t length,
                                      uint32_t timeout_ms __attribute__((unused)))
{
    mbx_send_req_t req = {
        .slave_address = slave_address,
        .type = MBOX_TYPE_FOE,
        .data = (uint8_t*)data,
        .length = length,
        .priority = 0,
        .user_data = NULL
    };

    al_status_t status = al_mailbox_send(&req);
    if (status != AL_STATUS_SUCCESS) {
        return FOE_STATUS_MAILBOX_ERROR;
    }

    return FOE_STATUS_SUCCESS;
}

/**
 * @brief Receive FoE mailbox message
 */
static foe_status_t foe_receive_mailbox(uint16_t slave_address,
                                         uint8_t* buffer,
                                         uint16_t* length,
                                         uint32_t timeout_ms)
{
    mailbox_type_t type;
    uint64_t start_time = hal_get_time_ms();

    while (1) {
        /* Check for timeout */
        uint64_t current_time = hal_get_time_ms();
        if ((current_time - start_time) > timeout_ms) {
            return FOE_STATUS_TIMEOUT;
        }

        /* Check if mailbox message is available */
        bool available = false;
        al_status_t status = al_mailbox_check(slave_address, &available);
        if (status != AL_STATUS_SUCCESS) {
            return FOE_STATUS_MAILBOX_ERROR;
        }

        if (!available) {
            hal_sleep_ms(1);  /* Wait 1ms before retry */
            continue;
        }

        /* Receive mailbox message */
        status = al_mailbox_receive(slave_address, &type, buffer, length);
        if (status != AL_STATUS_SUCCESS) {
            return FOE_STATUS_MAILBOX_ERROR;
        }

        /* Verify it's a FoE message */
        if (type != MBOX_TYPE_FOE) {
            continue;  /* Not FoE, wait for next message */
        }

        return FOE_STATUS_SUCCESS;
    }
}

/**
 * @brief Update transfer progress
 */
static void foe_update_progress(foe_transfer_context_t* ctx)
{
    if (ctx->progress_cb && ctx->total_bytes > 0) {
        ctx->progress_cb(ctx->transferred_bytes, ctx->total_bytes, ctx->user_data);
    }
}

/* ========================================================================== */
/*                          Public API Implementation                         */
/* ========================================================================== */

foe_status_t foe_read(uint16_t slave_address,
                      const char* filename,
                      uint8_t* data,
                      uint32_t* size,
                      uint32_t timeout_ms,
                      foe_progress_callback_t progress_callback,
                      void* user_data)
{
    if (!filename || !data || !size || *size == 0) {
        return FOE_STATUS_INVALID_PARAM;
    }

    if (timeout_ms == 0) {
        timeout_ms = FOE_DEFAULT_TIMEOUT_MS;
    }

    /* Initialize transfer context */
    foe_transfer_context_t ctx = {
        .state = FOE_STATE_IDLE,
        .slave_address = slave_address,
        .packet_number = 0,
        .total_bytes = 0,
        .transferred_bytes = 0,
        .start_time_ms = hal_get_time_ms(),
        .timeout_ms = timeout_ms,
        .progress_cb = progress_callback,
        .user_data = user_data,
        .last_error = 0
    };

    uint8_t tx_buffer[FOE_HEADER_SIZE + FOE_MAX_FILENAME_LENGTH];
    uint8_t rx_buffer[FOE_HEADER_SIZE + FOE_MAX_DATA_SIZE];
    uint16_t tx_length, rx_length;
    foe_status_t status;

    /* Step 1: Send READ request */
    status = foe_build_read_request(slave_address, filename, tx_buffer, &tx_length);
    if (status != FOE_STATUS_SUCCESS) {
        return status;
    }

    status = foe_send_mailbox(slave_address, tx_buffer, tx_length, FOE_PACKET_TIMEOUT_MS);
    if (status != FOE_STATUS_SUCCESS) {
        return status;
    }

    ctx.state = FOE_STATE_WAIT_FOR_DATA;

    /* Step 2: Receive DATA packets */
    uint32_t received_bytes = 0;
    uint32_t expected_packet = 1;

    while (ctx.state == FOE_STATE_WAIT_FOR_DATA) {
        /* Check for overall timeout */
        uint64_t current_time = hal_get_time_ms();
        if ((current_time - ctx.start_time_ms) > ctx.timeout_ms) {
            return FOE_STATUS_TIMEOUT;
        }

        /* Receive response */
        rx_length = sizeof(rx_buffer);
        status = foe_receive_mailbox(slave_address, rx_buffer, &rx_length, FOE_PACKET_TIMEOUT_MS);
        if (status != FOE_STATUS_SUCCESS) {
            return status;
        }

        /* Parse response */
        foe_opcode_t opcode;
        uint32_t packet_number;
        const uint8_t* packet_data;
        uint16_t packet_data_size;

        status = foe_parse_response(rx_buffer, rx_length, &opcode, &packet_number,
                                     &packet_data, &packet_data_size, &ctx.last_error);

        if (status != FOE_STATUS_SUCCESS) {
            return status;
        }

        switch (opcode) {
            case FOE_OPCODE_DATA:
                /* Verify packet number */
                if (packet_number != expected_packet) {
                    return FOE_STATUS_ERROR;
                }

                /* Check buffer overflow */
                if (received_bytes + packet_data_size > *size) {
                    return FOE_STATUS_INVALID_PARAM;
                }

                /* Copy data to buffer */
                memcpy(data + received_bytes, packet_data, packet_data_size);
                received_bytes += packet_data_size;
                ctx.transferred_bytes = received_bytes;

                /* Update progress */
                foe_update_progress(&ctx);

                /* Send ACK */
                status = foe_build_ack_packet(packet_number, tx_buffer, &tx_length);
                if (status != FOE_STATUS_SUCCESS) {
                    return status;
                }

                status = foe_send_mailbox(slave_address, tx_buffer, tx_length, FOE_PACKET_TIMEOUT_MS);
                if (status != FOE_STATUS_SUCCESS) {
                    return status;
                }

                expected_packet++;

                /* Check if this is the last packet (less than max size) */
                if (packet_data_size < FOE_MAX_DATA_SIZE) {
                    ctx.state = FOE_STATE_COMPLETED;
                }
                break;

            case FOE_OPCODE_BUSY:
                /* Slave is busy, wait and retry */
                hal_sleep_ms(FOE_BUSY_RETRY_MS);
                break;

            case FOE_OPCODE_ERROR:
                return FOE_STATUS_ERROR;

            default:
                return FOE_STATUS_ERROR;
        }
    }

    *size = received_bytes;
    return FOE_STATUS_SUCCESS;
}

foe_status_t foe_write(uint16_t slave_address,
                       const char* filename,
                       const uint8_t* data,
                       uint32_t size,
                       uint32_t timeout_ms,
                       foe_progress_callback_t progress_callback,
                       void* user_data)
{
    if (!filename || !data || size == 0) {
        return FOE_STATUS_INVALID_PARAM;
    }

    if (timeout_ms == 0) {
        timeout_ms = FOE_DEFAULT_TIMEOUT_MS;
    }

    /* Initialize transfer context */
    foe_transfer_context_t ctx = {
        .state = FOE_STATE_IDLE,
        .slave_address = slave_address,
        .packet_number = 0,
        .total_bytes = size,
        .transferred_bytes = 0,
        .start_time_ms = hal_get_time_ms(),
        .timeout_ms = timeout_ms,
        .progress_cb = progress_callback,
        .user_data = user_data,
        .last_error = 0
    };

    uint8_t tx_buffer[FOE_HEADER_SIZE + FOE_MAX_DATA_SIZE];
    uint8_t rx_buffer[FOE_HEADER_SIZE + FOE_MAX_DATA_SIZE];
    uint16_t tx_length, rx_length;
    foe_status_t status;

    /* Step 1: Send WRITE request */
    status = foe_build_write_request(slave_address, filename, tx_buffer, &tx_length);
    if (status != FOE_STATUS_SUCCESS) {
        return status;
    }

    status = foe_send_mailbox(slave_address, tx_buffer, tx_length, FOE_PACKET_TIMEOUT_MS);
    if (status != FOE_STATUS_SUCCESS) {
        return status;
    }

    ctx.state = FOE_STATE_WAIT_FOR_ACK;

    /* Step 2: Wait for initial ACK */
    rx_length = sizeof(rx_buffer);
    status = foe_receive_mailbox(slave_address, rx_buffer, &rx_length, FOE_PACKET_TIMEOUT_MS);
    if (status != FOE_STATUS_SUCCESS) {
        return status;
    }

    foe_opcode_t opcode;
    uint32_t ack_packet_number;
    status = foe_parse_response(rx_buffer, rx_length, &opcode, &ack_packet_number,
                                 NULL, NULL, &ctx.last_error);
    if (status != FOE_STATUS_SUCCESS) {
        return status;
    }

    if (opcode != FOE_OPCODE_ACK || ack_packet_number != 0) {
        return FOE_STATUS_ERROR;
    }

    /* Step 3: Send DATA packets */
    ctx.state = FOE_STATE_SENDING_DATA;
    uint32_t sent_bytes = 0;
    uint32_t packet_number = 1;
    int busy_retries = 0;

    while (sent_bytes < size) {
        /* Check for overall timeout */
        uint64_t current_time = hal_get_time_ms();
        if ((current_time - ctx.start_time_ms) > ctx.timeout_ms) {
            return FOE_STATUS_TIMEOUT;
        }

        /* Calculate data size for this packet */
        uint32_t remaining = size - sent_bytes;
        uint16_t packet_size = (remaining > FOE_MAX_DATA_SIZE) ? FOE_MAX_DATA_SIZE : remaining;

        /* Build DATA packet */
        status = foe_build_data_packet(packet_number, data + sent_bytes, packet_size,
                                        tx_buffer, &tx_length);
        if (status != FOE_STATUS_SUCCESS) {
            return status;
        }

        /* Send DATA packet */
        status = foe_send_mailbox(slave_address, tx_buffer, tx_length, FOE_PACKET_TIMEOUT_MS);
        if (status != FOE_STATUS_SUCCESS) {
            return status;
        }

        /* Wait for ACK */
        rx_length = sizeof(rx_buffer);
        status = foe_receive_mailbox(slave_address, rx_buffer, &rx_length, FOE_PACKET_TIMEOUT_MS);
        if (status != FOE_STATUS_SUCCESS) {
            return status;
        }

        /* Parse response */
        status = foe_parse_response(rx_buffer, rx_length, &opcode, &ack_packet_number,
                                     NULL, NULL, &ctx.last_error);
        if (status != FOE_STATUS_SUCCESS) {
            return status;
        }

        switch (opcode) {
            case FOE_OPCODE_ACK:
                /* Verify ACK packet number */
                if (ack_packet_number != packet_number) {
                    return FOE_STATUS_ERROR;
                }

                /* Update progress */
                sent_bytes += packet_size;
                ctx.transferred_bytes = sent_bytes;
                foe_update_progress(&ctx);

                packet_number++;
                busy_retries = 0;
                break;

            case FOE_OPCODE_BUSY:
                /* Slave is busy, wait and retry */
                if (++busy_retries > FOE_MAX_BUSY_RETRIES) {
                    return FOE_STATUS_BUSY;
                }
                hal_sleep_ms(FOE_BUSY_RETRY_MS);
                break;

            case FOE_OPCODE_ERROR:
                return FOE_STATUS_ERROR;

            default:
                return FOE_STATUS_ERROR;
        }
    }

    ctx.state = FOE_STATE_COMPLETED;
    return FOE_STATUS_SUCCESS;
}

foe_status_t foe_firmware_update(uint16_t slave_address,
                                  const uint8_t* firmware_data,
                                  uint32_t firmware_size,
                                  foe_progress_callback_t progress_callback,
                                  void* user_data,
                                  uint32_t timeout_ms)
{
    return foe_firmware_update_ex(slave_address, "ECAT.BIN", firmware_data,
                                   firmware_size, progress_callback, user_data, timeout_ms);
}

foe_status_t foe_firmware_update_ex(uint16_t slave_address,
                                     const char* filename,
                                     const uint8_t* firmware_data,
                                     uint32_t firmware_size,
                                     foe_progress_callback_t progress_callback,
                                     void* user_data,
                                     uint32_t timeout_ms)
{
    if (!filename || !firmware_data || firmware_size == 0) {
        return FOE_STATUS_INVALID_PARAM;
    }

    if (timeout_ms == 0) {
        timeout_ms = FOE_DEFAULT_TIMEOUT_MS * 6;  /* Longer timeout for firmware update */
    }

    al_status_t al_status;
    foe_status_t foe_status;

    /* Step 1: Get current AL state */
    al_state_t current_state;
    al_status = al_get_state(slave_address, &current_state);
    if (al_status != AL_STATUS_SUCCESS) {
        return FOE_STATUS_ERROR;
    }

    /* Step 2: Transition to Bootstrap state */
    if (current_state != AL_STATE_BOOT) {
        al_status = al_request_state(slave_address, AL_STATE_BOOT, timeout_ms / 6);
        if (al_status != AL_STATUS_SUCCESS) {
            return FOE_STATUS_ERROR;
        }
    }

    /* Step 3: Write firmware using FoE */
    foe_status = foe_write(slave_address, filename, firmware_data, firmware_size,
                           timeout_ms, progress_callback, user_data);
    if (foe_status != FOE_STATUS_SUCCESS) {
        return foe_status;
    }

    /* Step 4: Transition back to Init state (triggers restart) */
    al_status = al_request_state(slave_address, AL_STATE_INIT, timeout_ms / 6);
    if (al_status != AL_STATUS_SUCCESS) {
        return FOE_STATUS_ERROR;
    }

    /* Step 5: Wait for slave to restart */
    hal_sleep_ms(1000);  /* Wait 1 second for restart */

    return FOE_STATUS_SUCCESS;
}

/* ========================================================================== */
/*                          Utility Functions                                 */
/* ========================================================================== */

const char* foe_get_error_string(foe_error_code_t error_code)
{
    switch (error_code) {
        case FOE_ERROR_NOT_DEFINED:         return "Not defined";
        case FOE_ERROR_NOT_FOUND:           return "File not found";
        case FOE_ERROR_ACCESS_DENIED:       return "Access denied";
        case FOE_ERROR_DISK_FULL:           return "Disk full";
        case FOE_ERROR_ILLEGAL:             return "Illegal operation";
        case FOE_ERROR_PACKET_NUMBER:       return "Packet number error";
        case FOE_ERROR_ALREADY_EXISTS:      return "File already exists";
        case FOE_ERROR_NO_USER:             return "No user";
        case FOE_ERROR_BOOTSTRAP_ONLY:      return "Bootstrap mode only";
        case FOE_ERROR_NOT_BOOTSTRAP:       return "Not in bootstrap mode";
        case FOE_ERROR_NO_RIGHTS:           return "No rights";
        case FOE_ERROR_PROGRAM_ERROR:       return "Program error";
        default:                            return "Unknown error";
    }
}

const char* foe_get_status_string(foe_status_t status)
{
    switch (status) {
        case FOE_STATUS_SUCCESS:            return "Success";
        case FOE_STATUS_ERROR:              return "Error";
        case FOE_STATUS_TIMEOUT:            return "Timeout";
        case FOE_STATUS_BUSY:               return "Busy";
        case FOE_STATUS_INVALID_PARAM:      return "Invalid parameter";
        case FOE_STATUS_NOT_SUPPORTED:      return "Not supported";
        case FOE_STATUS_ABORTED:            return "Aborted";
        case FOE_STATUS_MAILBOX_ERROR:      return "Mailbox error";
        default:                            return "Unknown status";
    }
}

const char* foe_get_opcode_name(foe_opcode_t opcode)
{
    switch (opcode) {
        case FOE_OPCODE_READ:               return "READ";
        case FOE_OPCODE_WRITE:              return "WRITE";
        case FOE_OPCODE_DATA:               return "DATA";
        case FOE_OPCODE_ACK:                return "ACK";
        case FOE_OPCODE_ERROR:              return "ERROR";
        case FOE_OPCODE_BUSY:               return "BUSY";
        default:                            return "UNKNOWN";
    }
}

foe_status_t foe_check_support(uint16_t slave_address __attribute__((unused)), bool* supported)
{
    if (!supported) {
        return FOE_STATUS_INVALID_PARAM;
    }

    /* Check if slave supports FoE protocol via mailbox protocol support */
    /* Note: This function would need to be implemented in al.c or we can
     * read the SII EEPROM mailbox protocol support flags directly */

    /* For now, assume FoE is supported if mailbox is configured */
    /* TODO: Implement proper protocol support checking */
    *supported = true;

    return FOE_STATUS_SUCCESS;
}
