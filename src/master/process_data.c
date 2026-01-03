/**
 * @file process_data.c
 * @brief EtherCAT Process Data - Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "ethercat/process_data.h"
#include "ethercat/frame.h"
#include "ethercat/frame_builder.h"
#include "ethercat/frame_parser.h"
#include "ethercat/dll.h"
#include "ethercat/hal.h"
#include <string.h>
#include <stdlib.h>

/* ========================================================================== */
/* Constants                                                                 */
/* ========================================================================== */

#define PD_LOGICAL_ADDRESS_BASE     0x00000000  /**< Base logical address */
#define PD_MAX_FRAME_SIZE           1518        /**< Maximum frame size */
#define PD_DEFAULT_TIMEOUT_MS       100         /**< Default timeout (ms) */

/* EtherCAT broadcast MAC address */
static const uint8_t ECAT_BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* ========================================================================== */
/* Process Data Context                                                      */
/* ========================================================================== */

typedef struct {
    bool initialized;
    uint8_t master_mac[6];
    pd_statistics_t stats;
} pd_context_t;

static pd_context_t g_pd_context = {0};

/* ========================================================================== */
/* Helper Functions                                                          */
/* ========================================================================== */

/**
 * @brief Send frame and wait for response
 */
static pd_status_t send_and_receive(const uint8_t* frame_data,
                                     uint16_t frame_length,
                                     ecat_parsed_datagram_t* response,
                                     uint32_t timeout_ms)
{
    /* Send frame */
    dl_send_req_t send_req = {
        .frame_data = (uint8_t*)frame_data,
        .frame_length = frame_length,
        .priority = 0,
        .user_data = NULL
    };

    dl_status_t status = dl_send_req(&send_req);
    if (status != DL_STATUS_SUCCESS) {
        return PD_STATUS_ERROR;
    }

    /* Wait for response */
    uint64_t start_time = hal_get_time_ms();

    while (1) {
        /* Check timeout */
        if (timeout_ms > 0) {
            uint64_t elapsed = hal_get_time_ms() - start_time;
            if (elapsed >= timeout_ms) {
                g_pd_context.stats.timeout_count++;
                return PD_STATUS_TIMEOUT;
            }
        }

        /* Check for received frame */
        hal_frame_buffer_t* rx_buffer = NULL;
        hal_status_t hal_status = hal_receive_frame(&rx_buffer);

        if (hal_status == HAL_STATUS_SUCCESS && rx_buffer != NULL) {
            /* Parse frame */
            ecat_frame_parser_t parser;

            status = ecat_frame_parser_init(&parser, rx_buffer->data, rx_buffer->length);
            if (status != DL_STATUS_SUCCESS) {
                hal_free_rx_buffer(rx_buffer);
                continue;
            }

            status = ecat_frame_parser_validate(&parser);
            if (status != DL_STATUS_SUCCESS) {
                hal_free_rx_buffer(rx_buffer);
                continue;
            }

            /* Get first datagram */
            if (ecat_frame_parser_has_more(&parser)) {
                status = ecat_frame_parser_next_datagram(&parser, response);
                if (status == DL_STATUS_SUCCESS) {
                    hal_free_rx_buffer(rx_buffer);
                    return PD_STATUS_SUCCESS;
                }
            }

            hal_free_rx_buffer(rx_buffer);
        }

        hal_sleep_us(100);
    }

    return PD_STATUS_TIMEOUT;
}

/* ========================================================================== */
/* Initialization and Shutdown                                               */
/* ========================================================================== */

pd_status_t pd_init(void)
{
    if (g_pd_context.initialized) {
        return PD_STATUS_ERROR;
    }

    memset(&g_pd_context, 0, sizeof(pd_context_t));

    /* Get master MAC address from HAL */
    hal_device_info_t dev_info;
    if (hal_get_device_info(&dev_info) == HAL_STATUS_SUCCESS) {
        memcpy(g_pd_context.master_mac, dev_info.mac_address, 6);
    }

    /* Initialize statistics */
    g_pd_context.stats.min_cycle_time_us = UINT32_MAX;
    g_pd_context.stats.max_cycle_time_us = 0;
    g_pd_context.stats.avg_cycle_time_us = 0;

    g_pd_context.initialized = true;

    return PD_STATUS_SUCCESS;
}

pd_status_t pd_shutdown(void)
{
    if (!g_pd_context.initialized) {
        return PD_STATUS_NOT_INITIALIZED;
    }

    memset(&g_pd_context, 0, sizeof(pd_context_t));
    return PD_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Image Management                                                          */
/* ========================================================================== */

pd_status_t pd_allocate_image(pd_image_t* image,
                               uint32_t input_size,
                               uint32_t output_size,
                               const pd_redundancy_config_t* redundancy)
{
    if (!g_pd_context.initialized || image == NULL) {
        return PD_STATUS_INVALID_PARAM;
    }

    if (input_size == 0 && output_size == 0) {
        return PD_STATUS_INVALID_PARAM;
    }

    /* Clear image structure */
    memset(image, 0, sizeof(pd_image_t));

    /* Allocate input buffer */
    if (input_size > 0) {
        image->input_data = (uint8_t*)malloc(input_size);
        if (image->input_data == NULL) {
            return PD_STATUS_ERROR;
        }
        memset(image->input_data, 0, input_size);
        image->input_size = input_size;
    }

    /* Allocate output buffer */
    if (output_size > 0) {
        image->output_data = (uint8_t*)malloc(output_size);
        if (image->output_data == NULL) {
            if (image->input_data != NULL) {
                free(image->input_data);
                image->input_data = NULL;
            }
            return PD_STATUS_ERROR;
        }
        memset(image->output_data, 0, output_size);
        image->output_size = output_size;
    }

    /* Set logical address */
    image->logical_address = PD_LOGICAL_ADDRESS_BASE;

    /* Configure redundancy if provided */
    if (redundancy != NULL) {
        memcpy(&image->redundancy, redundancy, sizeof(pd_redundancy_config_t));
        image->current_port = redundancy->active_port;
    } else {
        /* Default: no redundancy */
        image->redundancy.mode = PD_REDUNDANCY_NONE;
        image->current_port = PD_PORT_PRIMARY;
    }

    /* Initialize frame management */
    image->frame_index = 0;
    image->frame_pending = false;

    return PD_STATUS_SUCCESS;
}

pd_status_t pd_free_image(pd_image_t* image)
{
    if (!g_pd_context.initialized || image == NULL) {
        return PD_STATUS_INVALID_PARAM;
    }

    /* Free input buffer */
    if (image->input_data != NULL) {
        free(image->input_data);
        image->input_data = NULL;
    }

    /* Free output buffer */
    if (image->output_data != NULL) {
        free(image->output_data);
        image->output_data = NULL;
    }

    /* Clear image structure */
    memset(image, 0, sizeof(pd_image_t));

    return PD_STATUS_SUCCESS;
}

pd_status_t pd_map_slave(uint16_t slave_count,
                          const pd_slave_mapping_t* mappings,
                          pd_image_t* image)
{
    if (!g_pd_context.initialized || mappings == NULL || image == NULL) {
        return PD_STATUS_INVALID_PARAM;
    }

    if (slave_count == 0) {
        return PD_STATUS_INVALID_PARAM;
    }

    /* Validate mappings */
    for (uint16_t i = 0; i < slave_count; i++) {
        const pd_slave_mapping_t* mapping = &mappings[i];

        /* Check input mapping */
        if (mapping->input_size > 0) {
            if (mapping->input_offset + mapping->input_size > image->input_size) {
                return PD_STATUS_BUFFER_OVERFLOW;
            }
        }

        /* Check output mapping */
        if (mapping->output_size > 0) {
            if (mapping->output_offset + mapping->output_size > image->output_size) {
                return PD_STATUS_BUFFER_OVERFLOW;
            }
        }
    }

    return PD_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Data Exchange (LRW Command)                                               */
/* ========================================================================== */

pd_status_t pd_exchange(pd_image_t* image,
                         uint16_t* working_counter,
                         uint32_t timeout_ms)
{
    if (!g_pd_context.initialized || image == NULL || working_counter == NULL) {
        return PD_STATUS_INVALID_PARAM;
    }

    if (image->output_data == NULL || image->input_data == NULL) {
        return PD_STATUS_INVALID_PARAM;
    }

    /* Record cycle start time */
    uint64_t cycle_start = hal_get_time_ns();

    /* Build LRW frame */
    uint8_t frame_buffer[PD_MAX_FRAME_SIZE];
    ecat_frame_builder_t builder;

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer, sizeof(frame_buffer),
                                                  g_pd_context.master_mac, ECAT_BROADCAST_MAC);
    if (status != DL_STATUS_SUCCESS) {
        return PD_STATUS_ERROR;
    }

    /* Calculate total data size (max of input and output) */
    uint32_t data_size = (image->input_size > image->output_size) ?
                         image->input_size : image->output_size;

    /* Add LRW datagram */
    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_LRW, image->frame_index,
                                              image->logical_address, image->output_data,
                                              data_size, false);
    if (status != DL_STATUS_SUCCESS) {
        return PD_STATUS_ERROR;
    }

    /* Finalize frame */
    uint16_t frame_length;
    status = ecat_frame_builder_finalize(&builder, &frame_length);
    if (status != DL_STATUS_SUCCESS) {
        return PD_STATUS_ERROR;
    }

    /* Send and receive */
    ecat_parsed_datagram_t response;
    pd_status_t pd_status = send_and_receive(frame_buffer, frame_length, &response, timeout_ms);

    if (pd_status != PD_STATUS_SUCCESS) {
        return pd_status;
    }

    /* Copy received data to input buffer */
    if (response.length >= image->input_size) {
        memcpy(image->input_data, response.data, image->input_size);
    } else {
        return PD_STATUS_ERROR;
    }

    /* Extract working counter */
    *working_counter = response.wkc;

    /* Update statistics */
    g_pd_context.stats.cycle_count++;
    g_pd_context.stats.last_working_counter = *working_counter;

    /* Calculate cycle time */
    uint64_t cycle_end = hal_get_time_ns();
    uint32_t cycle_time_us = (uint32_t)((cycle_end - cycle_start) / 1000);

    if (cycle_time_us < g_pd_context.stats.min_cycle_time_us) {
        g_pd_context.stats.min_cycle_time_us = cycle_time_us;
    }
    if (cycle_time_us > g_pd_context.stats.max_cycle_time_us) {
        g_pd_context.stats.max_cycle_time_us = cycle_time_us;
    }

    /* Update average cycle time */
    if (g_pd_context.stats.cycle_count > 0) {
        uint64_t total_time = (uint64_t)g_pd_context.stats.avg_cycle_time_us *
                              (g_pd_context.stats.cycle_count - 1) + cycle_time_us;
        g_pd_context.stats.avg_cycle_time_us = (uint32_t)(total_time / g_pd_context.stats.cycle_count);
    }

    /* Increment frame index */
    image->frame_index++;

    return PD_STATUS_SUCCESS;
}

pd_status_t pd_exchange_port(pd_image_t* image,
                              pd_port_select_t port,
                              uint16_t* working_counter,
                              uint32_t timeout_ms)
{
    /* Phase 5.2 - Not implemented yet */
    /* For now, just call pd_exchange() */
    (void)port;  /* Unused parameter */
    return pd_exchange(image, working_counter, timeout_ms);
}

/* ========================================================================== */
/* Working Counter Validation                                                */
/* ========================================================================== */

bool pd_validate_wkc(uint16_t expected, uint16_t actual)
{
    if (actual != expected) {
        g_pd_context.stats.wkc_error_count++;
        return false;
    }
    return true;
}

/* ========================================================================== */
/* Redundancy Control (Phase 5.2 - Stubs)                                   */
/* ========================================================================== */

pd_status_t pd_switch_port(pd_image_t* image, pd_port_select_t new_port)
{
    /* Phase 5.2 - Not implemented yet */
    if (!g_pd_context.initialized || image == NULL) {
        return PD_STATUS_INVALID_PARAM;
    }

    /* For now, just update the current port */
    image->current_port = new_port;
    return PD_STATUS_SUCCESS;
}

pd_status_t pd_check_port_health(pd_image_t* image,
                                  pd_port_select_t port,
                                  bool* healthy)
{
    /* Phase 5.2 - Not implemented yet */
    if (!g_pd_context.initialized || image == NULL || healthy == NULL) {
        return PD_STATUS_INVALID_PARAM;
    }

    (void)port;  /* Unused parameter */

    /* For now, always report healthy */
    *healthy = true;
    return PD_STATUS_SUCCESS;
}

pd_status_t pd_get_port_status(pd_image_t* image,
                                pd_port_select_t port,
                                pd_port_status_t* status)
{
    /* Phase 5.2 - Not implemented yet */
    if (!g_pd_context.initialized || image == NULL || status == NULL) {
        return PD_STATUS_INVALID_PARAM;
    }

    if (port >= 2) {
        return PD_STATUS_INVALID_PARAM;
    }

    /* Return stored port status */
    memcpy(status, &image->port_status[port], sizeof(pd_port_status_t));
    return PD_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Statistics                                                                */
/* ========================================================================== */

pd_status_t pd_get_statistics(pd_statistics_t* stats)
{
    if (!g_pd_context.initialized || stats == NULL) {
        return PD_STATUS_INVALID_PARAM;
    }

    memcpy(stats, &g_pd_context.stats, sizeof(pd_statistics_t));
    return PD_STATUS_SUCCESS;
}

pd_status_t pd_reset_statistics(void)
{
    if (!g_pd_context.initialized) {
        return PD_STATUS_NOT_INITIALIZED;
    }

    memset(&g_pd_context.stats, 0, sizeof(pd_statistics_t));
    g_pd_context.stats.min_cycle_time_us = UINT32_MAX;
    g_pd_context.stats.max_cycle_time_us = 0;
    g_pd_context.stats.avg_cycle_time_us = 0;

    return PD_STATUS_SUCCESS;
}
