/**
 * @file dll_tx.c
 * @brief EtherCAT Data Link Layer - Transmission Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "dll_internal.h"
#include "ethercat/dll_errors.h"
#include <string.h>

/* ========================================================================== */
/* Transmission Functions                                                     */
/* ========================================================================== */

dl_status_t dl_send_req(const dl_send_req_t* req)
{
    dll_context_t* ctx = dll_get_context();

    /* Validate parameters */
    if (req == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "dl_send_req: req is NULL");
        return DL_STATUS_INVALID_PARAM;
    }

    if (req->frame_data == NULL || req->frame_length == 0) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "dl_send_req: invalid frame data");
        return DL_STATUS_INVALID_PARAM;
    }

    if (req->frame_length > ECAT_MAX_FRAME_SIZE) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "dl_send_req: frame too large");
        return DL_STATUS_INVALID_PARAM;
    }

    /* Check if initialized and running */
    if (!ctx->initialized) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_send_req: not initialized");
        return DL_STATUS_ERROR;
    }

    if (ctx->state != DL_STATE_READY && ctx->state != DL_STATE_RUNNING) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_send_req: invalid state");
        return DL_STATUS_ERROR;
    }

    /* Check if TX queue is full */
    if (dl_queue_is_full(ctx->tx_queue)) {
        dl_set_error(DL_ERROR_QUEUE_FULL, "dl_send_req: TX queue full");
        ctx->statistics.tx_queue_overflows++;
        return DL_STATUS_ERROR;
    }

    /* Create queue entry */
    dl_queue_entry_t entry;
    entry.buffer = req->frame_data;
    entry.length = req->frame_length;
    entry.priority = req->priority;
    entry.user_data = req->user_data;
    entry.timestamp = 0;  /* TODO: Add timestamp */

    /* Enqueue frame */
    dl_status_t status = dl_queue_enqueue(ctx->tx_queue, &entry);
    if (status != DL_STATUS_SUCCESS) {
        dl_set_error(DL_ERROR_QUEUE_FULL, "dl_send_req: enqueue failed");
        ctx->statistics.tx_queue_overflows++;
        return DL_STATUS_ERROR;
    }

    /* Transition to RUNNING state if this is the first frame */
    if (ctx->state == DL_STATE_READY) {
        dl_state_set(DL_STATE_RUNNING);
        ctx->state = dl_state_get();
    }

    return DL_STATUS_SUCCESS;
}

/**
 * @brief Process TX queue (internal function)
 *
 * This function would be called by a cyclic task or interrupt handler
 * to actually transmit frames from the queue.
 *
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dll_process_tx_queue(void)
{
    dll_context_t* ctx = dll_get_context();

    if (!ctx->initialized || !ctx->running) {
        return DL_STATUS_ERROR;
    }

    /* Check if there are frames to send */
    if (dl_queue_is_empty(ctx->tx_queue)) {
        return DL_STATUS_SUCCESS;
    }

    /* Dequeue frame */
    dl_queue_entry_t entry;
    dl_status_t status = dl_queue_dequeue(ctx->tx_queue, &entry);
    if (status != DL_STATUS_SUCCESS) {
        return DL_STATUS_ERROR;
    }

    /* Allocate HAL TX buffer */
    hal_frame_buffer_t* hal_buffer = NULL;
    hal_status_t hal_status = hal_alloc_tx_buffer(entry.length, &hal_buffer);
    if (hal_status != HAL_STATUS_SUCCESS) {
        dl_set_error(DL_ERROR_NO_MEMORY, "dll_process_tx_queue: failed to allocate HAL buffer");
        ctx->statistics.send_errors++;
        return DL_STATUS_ERROR;
    }

    /* Copy frame data to HAL buffer */
    memcpy(hal_buffer->data, entry.buffer, entry.length);
    hal_buffer->length = entry.length;
    hal_buffer->user_data = entry.user_data;

    /* Send frame via HAL */
    hal_status = hal_send_frame(hal_buffer);
    if (hal_status != HAL_STATUS_SUCCESS) {
        dl_set_error(DL_ERROR_SEND_FAILED, "dll_process_tx_queue: HAL send failed");
        ctx->statistics.send_errors++;
        hal_free_tx_buffer(hal_buffer);
        return DL_STATUS_ERROR;
    }

    /* Free HAL buffer */
    hal_free_tx_buffer(hal_buffer);

    /* Update statistics */
    ctx->statistics.frames_sent++;

    /* Invoke send confirmation callback */
    if (ctx->send_callback != NULL) {
        dl_send_con_t con;
        con.status = DL_STATUS_SUCCESS;
        con.user_data = entry.user_data;
        ctx->send_callback(&con);
    }

    return DL_STATUS_SUCCESS;
}
