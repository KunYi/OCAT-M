/**
 * @file dll_init.c
 * @brief EtherCAT Data Link Layer - Initialization Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "dll_internal.h"
#include "ethercat/dll_errors.h"
#include <string.h>

/* ========================================================================== */
/* Global DLL Context                                                         */
/* ========================================================================== */

static dll_context_t g_dll_context;

dll_context_t* dll_get_context(void)
{
    return &g_dll_context;
}

/* ========================================================================== */
/* Initialization Functions                                                   */
/* ========================================================================== */

dl_status_t dl_init(const dl_config_t* config)
{
    dll_context_t* ctx = dll_get_context();

    /* Check if already initialized */
    if (ctx->initialized) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_init: already initialized");
        return DL_STATUS_ERROR;
    }

    /* Validate configuration */
    if (config == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "dl_init: config is NULL");
        return DL_STATUS_INVALID_PARAM;
    }

    dl_status_t status = dl_config_validate(config);
    if (status != DL_STATUS_SUCCESS) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "dl_init: invalid configuration");
        return status;
    }

    /* Clear context */
    memset(ctx, 0, sizeof(dll_context_t));

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(dl_config_t));

    /* Initialize state machine */
    dl_state_init();
    ctx->state = dl_state_get();

    /* Create TX queue (with priority support) */
    ctx->tx_queue = dl_queue_create(config->tx_queue_size, true);
    if (ctx->tx_queue == NULL) {
        dl_set_error(DL_ERROR_NO_MEMORY, "dl_init: failed to create TX queue");
        return DL_STATUS_ERROR;
    }

    /* Create RX queue (FIFO) */
    ctx->rx_queue = dl_queue_create(config->rx_queue_size, false);
    if (ctx->rx_queue == NULL) {
        dl_queue_destroy(ctx->tx_queue);
        dl_set_error(DL_ERROR_NO_MEMORY, "dl_init: failed to create RX queue");
        return DL_STATUS_ERROR;
    }

    /* Initialize HAL */
    hal_config_t hal_config;
    hal_config_init_defaults(&hal_config);
    hal_config.platform = HAL_PLATFORM_STUB;  /* Default to stub for testing */
    memcpy(hal_config.mac_address, config->mac_address, 6);
    hal_config.rx_buffer_count = config->rx_queue_size;
    hal_config.tx_buffer_count = config->tx_queue_size;
    hal_config.promiscuous_mode = true;
    hal_config.blocking_mode = false;

    hal_status_t hal_status = hal_init(&hal_config);
    if (hal_status != HAL_STATUS_SUCCESS) {
        dl_queue_destroy(ctx->tx_queue);
        dl_queue_destroy(ctx->rx_queue);
        dl_set_error(DL_ERROR_INIT_FAILED, "dl_init: HAL initialization failed");
        return DL_STATUS_ERROR;
    }

    /* Initialize statistics */
    memset(&ctx->statistics, 0, sizeof(dl_statistics_t));
    ctx->statistics.min_cycle_time_us = 0xFFFFFFFF;

    /* Set initialization flag */
    ctx->initialized = true;

    /* Transition to INITIALIZED state */
    status = dl_state_set(DL_STATE_INITIALIZED);
    if (status != DL_STATUS_SUCCESS) {
        dl_queue_destroy(ctx->tx_queue);
        dl_queue_destroy(ctx->rx_queue);
        ctx->initialized = false;
        dl_set_error(DL_ERROR_INIT_FAILED, "dl_init: state transition failed");
        return DL_STATUS_ERROR;
    }

    ctx->state = dl_state_get();

    return DL_STATUS_SUCCESS;
}

dl_status_t dl_shutdown(void)
{
    dll_context_t* ctx = dll_get_context();

    /* Check if initialized */
    if (!ctx->initialized) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_shutdown: not initialized");
        return DL_STATUS_ERROR;
    }

    /* Check state */
    if (ctx->state != DL_STATE_INITIALIZED && ctx->state != DL_STATE_READY) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_shutdown: invalid state");
        return DL_STATUS_ERROR;
    }

    /* Stop if running */
    if (ctx->running) {
        dl_stop();
    }

    /* Shutdown HAL */
    if (hal_is_initialized()) {
        hal_shutdown();
    }

    /* Destroy queues */
    if (ctx->tx_queue != NULL) {
        dl_queue_destroy(ctx->tx_queue);
        ctx->tx_queue = NULL;
    }

    if (ctx->rx_queue != NULL) {
        dl_queue_destroy(ctx->rx_queue);
        ctx->rx_queue = NULL;
    }

    /* Transition to UNINITIALIZED state */
    dl_state_set(DL_STATE_UNINITIALIZED);
    ctx->state = dl_state_get();

    /* Clear initialization flag */
    ctx->initialized = false;

    return DL_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Parameter Access Functions                                                 */
/* ========================================================================== */

dl_status_t dl_set_parameter(uint16_t param_id, const void* value, uint16_t length)
{
    dll_context_t* ctx = dll_get_context();

    if (!ctx->initialized) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_set_parameter: not initialized");
        return DL_STATUS_ERROR;
    }

    /* Some parameters can only be set when not running */
    if (ctx->running) {
        switch (param_id) {
            case DL_PARAM_MAC_ADDRESS:
            case DL_PARAM_MAX_FRAME_SIZE:
            case DL_PARAM_TX_QUEUE_SIZE:
            case DL_PARAM_RX_QUEUE_SIZE:
            case DL_PARAM_NUM_PORTS:
                dl_set_error(DL_ERROR_INVALID_STATE, "dl_set_parameter: cannot change while running");
                return DL_STATUS_ERROR;
            default:
                break;
        }
    }

    return dl_config_set_parameter(&ctx->config, (dl_param_id_t)param_id, value, length);
}

dl_status_t dl_get_parameter(uint16_t param_id, void* value, uint16_t* length)
{
    dll_context_t* ctx = dll_get_context();

    if (!ctx->initialized) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_get_parameter: not initialized");
        return DL_STATUS_ERROR;
    }

    return dl_config_get_parameter(&ctx->config, (dl_param_id_t)param_id, value, length);
}

/* ========================================================================== */
/* Callback Registration Functions                                            */
/* ========================================================================== */

dl_status_t dl_register_send_callback(dl_send_con_cb_t callback)
{
    dll_context_t* ctx = dll_get_context();

    if (!ctx->initialized) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_register_send_callback: not initialized");
        return DL_STATUS_ERROR;
    }

    ctx->send_callback = callback;
    return DL_STATUS_SUCCESS;
}

dl_status_t dl_register_receive_callback(dl_receive_ind_cb_t callback)
{
    dll_context_t* ctx = dll_get_context();

    if (!ctx->initialized) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_register_receive_callback: not initialized");
        return DL_STATUS_ERROR;
    }

    ctx->receive_callback = callback;
    return DL_STATUS_SUCCESS;
}

/* ========================================================================== */
/* State Query Functions                                                      */
/* ========================================================================== */

dl_state_t dl_get_state(void)
{
    dll_context_t* ctx = dll_get_context();
    return ctx->state;
}

/* ========================================================================== */
/* Queue Query Functions                                                      */
/* ========================================================================== */

uint16_t dl_get_tx_queue_count(void)
{
    dll_context_t* ctx = dll_get_context();

    if (!ctx->initialized || ctx->tx_queue == NULL) {
        return 0;
    }

    return dl_queue_count(ctx->tx_queue);
}

uint16_t dl_get_rx_queue_count(void)
{
    dll_context_t* ctx = dll_get_context();

    if (!ctx->initialized || ctx->rx_queue == NULL) {
        return 0;
    }

    return dl_queue_count(ctx->rx_queue);
}

dl_status_t dl_flush_tx_queue(void)
{
    dll_context_t* ctx = dll_get_context();

    if (!ctx->initialized) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_flush_tx_queue: not initialized");
        return DL_STATUS_ERROR;
    }

    if (ctx->tx_queue == NULL) {
        return DL_STATUS_ERROR;
    }

    return dl_queue_flush(ctx->tx_queue);
}

dl_status_t dl_flush_rx_queue(void)
{
    dll_context_t* ctx = dll_get_context();

    if (!ctx->initialized) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_flush_rx_queue: not initialized");
        return DL_STATUS_ERROR;
    }

    if (ctx->rx_queue == NULL) {
        return DL_STATUS_ERROR;
    }

    return dl_queue_flush(ctx->rx_queue);
}

/* ========================================================================== */
/* Statistics Functions                                                       */
/* ========================================================================== */

dl_status_t dl_get_statistics(dl_statistics_t* stats)
{
    dll_context_t* ctx = dll_get_context();

    if (!ctx->initialized) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_get_statistics: not initialized");
        return DL_STATUS_ERROR;
    }

    if (stats == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "dl_get_statistics: stats is NULL");
        return DL_STATUS_INVALID_PARAM;
    }

    memcpy(stats, &ctx->statistics, sizeof(dl_statistics_t));
    return DL_STATUS_SUCCESS;
}

dl_status_t dl_reset_statistics(void)
{
    dll_context_t* ctx = dll_get_context();

    if (!ctx->initialized) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_reset_statistics: not initialized");
        return DL_STATUS_ERROR;
    }

    memset(&ctx->statistics, 0, sizeof(dl_statistics_t));
    ctx->statistics.min_cycle_time_us = 0xFFFFFFFF;

    return DL_STATUS_SUCCESS;
}
