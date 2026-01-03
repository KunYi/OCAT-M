/**
 * @file dll_rx.c
 * @brief EtherCAT Data Link Layer - Reception Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "dll_internal.h"
#include "ethercat/dll_errors.h"
#include <string.h>

/* ========================================================================== */
/* Reception Functions                                                        */
/* ========================================================================== */

/**
 * @brief Process received frame (internal function)
 *
 * This function would be called by an interrupt handler or polling task
 * when a frame is received from the hardware.
 *
 * @param frame_data Pointer to received frame data
 * @param frame_length Length of received frame
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dll_process_rx_frame(const uint8_t* frame_data, uint16_t frame_length)
{
    dll_context_t* ctx = dll_get_context();

    /* Validate parameters */
    if (frame_data == NULL || frame_length == 0) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "dll_process_rx_frame: invalid parameters");
        ctx->statistics.receive_errors++;
        return DL_STATUS_INVALID_PARAM;
    }

    /* Check if initialized and running */
    if (!ctx->initialized || !ctx->running) {
        return DL_STATUS_ERROR;
    }

    /* Validate frame length */
    if (frame_length < ECAT_MIN_FRAME_SIZE || frame_length > ECAT_MAX_FRAME_SIZE) {
        dl_set_error(DL_ERROR_INVALID_FRAME, "dll_process_rx_frame: invalid frame length");
        ctx->statistics.receive_errors++;
        return DL_STATUS_ERROR;
    }

    /* Initialize frame parser */
    ecat_frame_parser_t parser;
    dl_status_t status = ecat_frame_parser_init(&parser, frame_data, frame_length);
    if (status != DL_STATUS_SUCCESS) {
        dl_set_error(DL_ERROR_INVALID_FRAME, "dll_process_rx_frame: parser init failed");
        ctx->statistics.receive_errors++;
        return DL_STATUS_ERROR;
    }

    /* Validate frame */
    status = ecat_frame_parser_validate(&parser);
    if (status != DL_STATUS_SUCCESS) {
        dl_set_error(DL_ERROR_INVALID_FRAME, "dll_process_rx_frame: frame validation failed");
        ctx->statistics.receive_errors++;
        return DL_STATUS_ERROR;
    }

    /* Verify FCS (CRC) */
    if (!ecat_frame_parser_verify_fcs(&parser)) {
        dl_set_error(DL_ERROR_INVALID_FRAME, "dll_process_rx_frame: FCS verification failed");
        ctx->statistics.receive_errors++;
        return DL_STATUS_ERROR;
    }

    /* Check if RX queue is full */
    if (dl_queue_is_full(ctx->rx_queue)) {
        dl_set_error(DL_ERROR_QUEUE_FULL, "dll_process_rx_frame: RX queue full");
        ctx->statistics.rx_queue_overflows++;
        return DL_STATUS_ERROR;
    }

    /* Copy frame to RX buffer (in real implementation, might use DMA) */
    memcpy(ctx->rx_frame_buffer, frame_data, frame_length);

    /* Create queue entry */
    dl_queue_entry_t entry;
    entry.buffer = ctx->rx_frame_buffer;
    entry.length = frame_length;
    entry.priority = 0;
    entry.user_data = NULL;
    entry.timestamp = 0;  /* TODO: Add timestamp */

    /* Enqueue frame */
    status = dl_queue_enqueue(ctx->rx_queue, &entry);
    if (status != DL_STATUS_SUCCESS) {
        dl_set_error(DL_ERROR_QUEUE_FULL, "dll_process_rx_frame: enqueue failed");
        ctx->statistics.rx_queue_overflows++;
        return DL_STATUS_ERROR;
    }

    /* Update statistics */
    ctx->statistics.frames_received++;

    /* Invoke receive indication callback */
    if (ctx->receive_callback != NULL) {
        dl_receive_ind_t ind;
        ind.frame_data = ctx->rx_frame_buffer;
        ind.frame_length = frame_length;
        ind.timestamp = 0;  /* TODO: Add timestamp */
        ind.port = 0;
        ctx->receive_callback(&ind);
    }

    return DL_STATUS_SUCCESS;
}

/**
 * @brief Process RX queue (internal function)
 *
 * This function polls the HAL for received frames and processes them.
 *
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dll_process_rx_queue(void)
{
    dll_context_t* ctx = dll_get_context();

    if (!ctx->initialized || !ctx->running) {
        return DL_STATUS_ERROR;
    }

    /* Poll HAL for received frames */
    hal_frame_buffer_t* hal_buffer = NULL;
    hal_status_t hal_status = hal_receive_frame(&hal_buffer);

    if (hal_status == HAL_STATUS_WOULD_BLOCK) {
        /* No frame available */
        return DL_STATUS_SUCCESS;
    }

    if (hal_status != HAL_STATUS_SUCCESS) {
        /* Error receiving frame */
        return DL_STATUS_ERROR;
    }

    if (hal_buffer == NULL || hal_buffer->data == NULL) {
        return DL_STATUS_ERROR;
    }

    /* Process the received frame */
    dl_status_t status = dll_process_rx_frame(hal_buffer->data, hal_buffer->length);

    /* Free HAL RX buffer */
    hal_free_rx_buffer(hal_buffer);

    return status;
}
