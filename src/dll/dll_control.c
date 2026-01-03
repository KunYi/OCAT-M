/**
 * @file dll_control.c
 * @brief EtherCAT Data Link Layer - Control Functions Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "dll_internal.h"
#include "ethercat/dll_errors.h"

/* ========================================================================== */
/* Control Functions                                                          */
/* ========================================================================== */

dl_status_t dl_start(void)
{
    dll_context_t* ctx = dll_get_context();

    /* Check if initialized */
    if (!ctx->initialized) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_start: not initialized");
        return DL_STATUS_ERROR;
    }

    /* Check current state */
    if (ctx->state != DL_STATE_INITIALIZED) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_start: invalid state");
        return DL_STATUS_ERROR;
    }

    /* Initialize frame builder */
    dl_status_t status = ecat_frame_builder_init(
        &ctx->frame_builder,
        ctx->tx_frame_buffer,
        sizeof(ctx->tx_frame_buffer),
        ctx->config.mac_address,
        (uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}  /* Broadcast MAC */
    );

    if (status != DL_STATUS_SUCCESS) {
        dl_set_error(DL_ERROR_INIT_FAILED, "dl_start: frame builder init failed");
        return DL_STATUS_ERROR;
    }

    /* Transition to READY state */
    status = dl_state_set(DL_STATE_READY);
    if (status != DL_STATUS_SUCCESS) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_start: state transition failed");
        return DL_STATUS_ERROR;
    }

    ctx->state = dl_state_get();
    ctx->running = true;

    return DL_STATUS_SUCCESS;
}

dl_status_t dl_stop(void)
{
    dll_context_t* ctx = dll_get_context();

    /* Check if initialized */
    if (!ctx->initialized) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_stop: not initialized");
        return DL_STATUS_ERROR;
    }

    /* Check current state */
    if (ctx->state != DL_STATE_READY && ctx->state != DL_STATE_RUNNING) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_stop: invalid state");
        return DL_STATUS_ERROR;
    }

    /* Flush queues */
    dl_flush_tx_queue();
    dl_flush_rx_queue();

    /* Transition to INITIALIZED state */
    dl_status_t status = dl_state_set(DL_STATE_INITIALIZED);
    if (status != DL_STATUS_SUCCESS) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_stop: state transition failed");
        return DL_STATUS_ERROR;
    }

    ctx->state = dl_state_get();
    ctx->running = false;

    return DL_STATUS_SUCCESS;
}

dl_status_t dl_reset(void)
{
    dll_context_t* ctx = dll_get_context();

    /* Check if initialized */
    if (!ctx->initialized) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_reset: not initialized");
        return DL_STATUS_ERROR;
    }

    /* Check current state */
    if (ctx->state != DL_STATE_ERROR) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_reset: not in error state");
        return DL_STATUS_ERROR;
    }

    /* Flush queues */
    dl_flush_tx_queue();
    dl_flush_rx_queue();

    /* Reset statistics */
    dl_reset_statistics();

    /* Transition to INITIALIZED state */
    dl_status_t status = dl_state_set(DL_STATE_INITIALIZED);
    if (status != DL_STATUS_SUCCESS) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_reset: state transition failed");
        return DL_STATUS_ERROR;
    }

    ctx->state = dl_state_get();
    ctx->running = false;

    return DL_STATUS_SUCCESS;
}
