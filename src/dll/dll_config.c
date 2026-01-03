/**
 * @file dll_config.c
 * @brief EtherCAT Data Link Layer - Configuration Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "ethercat/dll_config.h"
#include "ethercat/dll_errors.h"
#include <string.h>

/* ========================================================================== */
/* Public Functions                                                           */
/* ========================================================================== */

dl_status_t dl_config_init_defaults(dl_config_t* config)
{
    if (config == NULL) {
        return DL_STATUS_INVALID_PARAM;
    }

    /* Clear the structure */
    memset(config, 0, sizeof(dl_config_t));

    /* Set default MAC address (00:00:00:00:00:00 - to be configured by user) */
    memset(config->mac_address, 0, 6);

    /* Set default values */
    config->max_frame_size = DL_DEFAULT_MAX_FRAME_SIZE;
    config->tx_queue_size = DL_DEFAULT_TX_QUEUE_SIZE;
    config->rx_queue_size = DL_DEFAULT_RX_QUEUE_SIZE;
    config->cycle_time_us = DL_DEFAULT_CYCLE_TIME_US;
    config->num_ports = DL_DEFAULT_NUM_PORTS;
    config->enable_redundancy = DL_DEFAULT_REDUNDANCY_ENABLE;
    config->enable_distributed_clocks = DL_DEFAULT_DC_ENABLE;

    return DL_STATUS_SUCCESS;
}

dl_status_t dl_config_validate(const dl_config_t* config)
{
    if (config == NULL) {
        return DL_STATUS_INVALID_PARAM;
    }

    /* Validate frame size */
    if (config->max_frame_size < DL_MIN_FRAME_SIZE ||
        config->max_frame_size > DL_MAX_FRAME_SIZE) {
        return DL_STATUS_INVALID_PARAM;
    }

    /* Validate queue sizes */
    if (config->tx_queue_size < DL_MIN_QUEUE_SIZE ||
        config->tx_queue_size > DL_MAX_QUEUE_SIZE) {
        return DL_STATUS_INVALID_PARAM;
    }

    if (config->rx_queue_size < DL_MIN_QUEUE_SIZE ||
        config->rx_queue_size > DL_MAX_QUEUE_SIZE) {
        return DL_STATUS_INVALID_PARAM;
    }

    /* Validate cycle time */
    if (config->cycle_time_us < DL_MIN_CYCLE_TIME_US ||
        config->cycle_time_us > DL_MAX_CYCLE_TIME_US) {
        return DL_STATUS_INVALID_PARAM;
    }

    /* Validate number of ports */
    if (config->num_ports == 0 || config->num_ports > DL_MAX_NUM_PORTS) {
        return DL_STATUS_INVALID_PARAM;
    }

    /* Validate MAC address (should not be all zeros or broadcast) */
    bool all_zero = true;
    bool all_ff = true;
    for (int i = 0; i < 6; i++) {
        if (config->mac_address[i] != 0x00) {
            all_zero = false;
        }
        if (config->mac_address[i] != 0xFF) {
            all_ff = false;
        }
    }

    if (all_zero || all_ff) {
        return DL_STATUS_INVALID_PARAM;
    }

    return DL_STATUS_SUCCESS;
}

dl_status_t dl_config_set_parameter(dl_config_t* config,
                                     dl_param_id_t param_id,
                                     const void* value,
                                     uint16_t length)
{
    if (config == NULL || value == NULL) {
        return DL_STATUS_INVALID_PARAM;
    }

    switch (param_id) {
        case DL_PARAM_MAC_ADDRESS:
            if (length != 6) {
                return DL_STATUS_INVALID_PARAM;
            }
            memcpy(config->mac_address, value, 6);
            break;

        case DL_PARAM_MAX_FRAME_SIZE:
            if (length != sizeof(uint16_t)) {
                return DL_STATUS_INVALID_PARAM;
            }
            config->max_frame_size = *(const uint16_t*)value;
            break;

        case DL_PARAM_CYCLE_TIME:
            if (length != sizeof(uint32_t)) {
                return DL_STATUS_INVALID_PARAM;
            }
            config->cycle_time_us = *(const uint32_t*)value;
            break;

        case DL_PARAM_TX_QUEUE_SIZE:
            if (length != sizeof(uint16_t)) {
                return DL_STATUS_INVALID_PARAM;
            }
            config->tx_queue_size = *(const uint16_t*)value;
            break;

        case DL_PARAM_RX_QUEUE_SIZE:
            if (length != sizeof(uint16_t)) {
                return DL_STATUS_INVALID_PARAM;
            }
            config->rx_queue_size = *(const uint16_t*)value;
            break;

        case DL_PARAM_REDUNDANCY_ENABLE:
            if (length != sizeof(bool)) {
                return DL_STATUS_INVALID_PARAM;
            }
            config->enable_redundancy = *(const bool*)value;
            break;

        case DL_PARAM_DC_ENABLE:
            if (length != sizeof(bool)) {
                return DL_STATUS_INVALID_PARAM;
            }
            config->enable_distributed_clocks = *(const bool*)value;
            break;

        case DL_PARAM_NUM_PORTS:
            if (length != sizeof(uint8_t)) {
                return DL_STATUS_INVALID_PARAM;
            }
            config->num_ports = *(const uint8_t*)value;
            break;

        default:
            return DL_STATUS_INVALID_PARAM;
    }

    return DL_STATUS_SUCCESS;
}

dl_status_t dl_config_get_parameter(const dl_config_t* config,
                                     dl_param_id_t param_id,
                                     void* value,
                                     uint16_t* length)
{
    if (config == NULL || value == NULL || length == NULL) {
        return DL_STATUS_INVALID_PARAM;
    }

    switch (param_id) {
        case DL_PARAM_MAC_ADDRESS:
            if (*length < 6) {
                return DL_STATUS_INVALID_PARAM;
            }
            memcpy(value, config->mac_address, 6);
            *length = 6;
            break;

        case DL_PARAM_MAX_FRAME_SIZE:
            if (*length < sizeof(uint16_t)) {
                return DL_STATUS_INVALID_PARAM;
            }
            *(uint16_t*)value = config->max_frame_size;
            *length = sizeof(uint16_t);
            break;

        case DL_PARAM_CYCLE_TIME:
            if (*length < sizeof(uint32_t)) {
                return DL_STATUS_INVALID_PARAM;
            }
            *(uint32_t*)value = config->cycle_time_us;
            *length = sizeof(uint32_t);
            break;

        case DL_PARAM_TX_QUEUE_SIZE:
            if (*length < sizeof(uint16_t)) {
                return DL_STATUS_INVALID_PARAM;
            }
            *(uint16_t*)value = config->tx_queue_size;
            *length = sizeof(uint16_t);
            break;

        case DL_PARAM_RX_QUEUE_SIZE:
            if (*length < sizeof(uint16_t)) {
                return DL_STATUS_INVALID_PARAM;
            }
            *(uint16_t*)value = config->rx_queue_size;
            *length = sizeof(uint16_t);
            break;

        case DL_PARAM_REDUNDANCY_ENABLE:
            if (*length < sizeof(bool)) {
                return DL_STATUS_INVALID_PARAM;
            }
            *(bool*)value = config->enable_redundancy;
            *length = sizeof(bool);
            break;

        case DL_PARAM_DC_ENABLE:
            if (*length < sizeof(bool)) {
                return DL_STATUS_INVALID_PARAM;
            }
            *(bool*)value = config->enable_distributed_clocks;
            *length = sizeof(bool);
            break;

        case DL_PARAM_NUM_PORTS:
            if (*length < sizeof(uint8_t)) {
                return DL_STATUS_INVALID_PARAM;
            }
            *(uint8_t*)value = config->num_ports;
            *length = sizeof(uint8_t);
            break;

        default:
            return DL_STATUS_INVALID_PARAM;
    }

    return DL_STATUS_SUCCESS;
}
