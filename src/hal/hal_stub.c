/**
 * @file hal_stub.c
 * @brief HAL Stub Implementation for Testing
 * @version 1.0.0
 * @date 2026-01-03
 *
 * This is a stub implementation of the HAL platform interface.
 * It provides a minimal implementation for testing purposes without
 * requiring actual hardware or network interfaces.
 */

#include "hal_internal.h"
#include <string.h>
#include <stdlib.h>

/* ========================================================================== */
/* Stub Platform Context                                                      */
/* ========================================================================== */

typedef struct {
    hal_frame_buffer_t* tx_buffers[32];
    hal_frame_buffer_t* rx_buffers[32];
    uint32_t tx_buffer_count;
    uint32_t rx_buffer_count;
    bool link_up;
} hal_stub_context_t;

/* ========================================================================== */
/* Stub Platform Operations                                                   */
/* ========================================================================== */

static hal_status_t hal_stub_init(hal_context_t* ctx)
{
    if (ctx == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    /* Allocate stub context */
    hal_stub_context_t* stub_ctx = (hal_stub_context_t*)calloc(1, sizeof(hal_stub_context_t));
    if (stub_ctx == NULL) {
        return HAL_STATUS_ERROR;
    }

    stub_ctx->link_up = true;
    ctx->platform_context = stub_ctx;

    /* Initialize device info */
    strncpy(ctx->device_info.interface_name, "stub0", sizeof(ctx->device_info.interface_name) - 1);
    memcpy(ctx->device_info.mac_address, ctx->config.mac_address, 6);
    ctx->device_info.mtu = 1500;
    ctx->device_info.speed_mbps = 100;
    ctx->device_info.link_up = true;
    ctx->device_info.full_duplex = true;
    ctx->device_info.platform = HAL_PLATFORM_STUB;

    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_stub_shutdown(hal_context_t* ctx)
{
    if (ctx == NULL || ctx->platform_context == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    hal_stub_context_t* stub_ctx = (hal_stub_context_t*)ctx->platform_context;

    /* Free all allocated buffers */
    for (uint32_t i = 0; i < stub_ctx->tx_buffer_count; i++) {
        if (stub_ctx->tx_buffers[i] != NULL) {
            free(stub_ctx->tx_buffers[i]->data);
            free(stub_ctx->tx_buffers[i]);
        }
    }

    for (uint32_t i = 0; i < stub_ctx->rx_buffer_count; i++) {
        if (stub_ctx->rx_buffers[i] != NULL) {
            free(stub_ctx->rx_buffers[i]->data);
            free(stub_ctx->rx_buffers[i]);
        }
    }

    free(stub_ctx);
    ctx->platform_context = NULL;

    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_stub_send_frame(hal_context_t* ctx, hal_frame_buffer_t* buffer)
{
    if (ctx == NULL || buffer == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    /* Stub implementation: just pretend to send */
    (void)ctx;
    (void)buffer;

    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_stub_receive_frame(hal_context_t* ctx, hal_frame_buffer_t** buffer)
{
    if (ctx == NULL || buffer == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    /* Stub implementation: no frames to receive */
    (void)ctx;
    *buffer = NULL;

    return HAL_STATUS_WOULD_BLOCK;
}

static hal_status_t hal_stub_alloc_tx_buffer(hal_context_t* ctx, uint16_t size, hal_frame_buffer_t** buffer)
{
    if (ctx == NULL || buffer == NULL || size == 0) {
        return HAL_STATUS_INVALID_PARAM;
    }

    hal_stub_context_t* stub_ctx = (hal_stub_context_t*)ctx->platform_context;
    if (stub_ctx == NULL) {
        return HAL_STATUS_ERROR;
    }

    if (stub_ctx->tx_buffer_count >= 32) {
        return HAL_STATUS_NO_BUFFER;
    }

    /* Allocate buffer structure */
    hal_frame_buffer_t* buf = (hal_frame_buffer_t*)calloc(1, sizeof(hal_frame_buffer_t));
    if (buf == NULL) {
        return HAL_STATUS_ERROR;
    }

    /* Allocate data buffer */
    buf->data = (uint8_t*)malloc(size);
    if (buf->data == NULL) {
        free(buf);
        return HAL_STATUS_ERROR;
    }

    buf->length = 0;
    buf->capacity = size;
    buf->timestamp = 0;
    buf->port = 0;
    buf->user_data = NULL;
    buf->hal_private = NULL;

    stub_ctx->tx_buffers[stub_ctx->tx_buffer_count++] = buf;
    *buffer = buf;

    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_stub_free_tx_buffer(hal_context_t* ctx, hal_frame_buffer_t* buffer)
{
    if (ctx == NULL || buffer == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    hal_stub_context_t* stub_ctx = (hal_stub_context_t*)ctx->platform_context;
    if (stub_ctx == NULL) {
        return HAL_STATUS_ERROR;
    }

    /* Find and remove buffer from list */
    for (uint32_t i = 0; i < stub_ctx->tx_buffer_count; i++) {
        if (stub_ctx->tx_buffers[i] == buffer) {
            free(buffer->data);
            free(buffer);

            /* Shift remaining buffers */
            for (uint32_t j = i; j < stub_ctx->tx_buffer_count - 1; j++) {
                stub_ctx->tx_buffers[j] = stub_ctx->tx_buffers[j + 1];
            }
            stub_ctx->tx_buffer_count--;
            return HAL_STATUS_SUCCESS;
        }
    }

    return HAL_STATUS_ERROR;
}

static hal_status_t hal_stub_free_rx_buffer(hal_context_t* ctx, hal_frame_buffer_t* buffer)
{
    if (ctx == NULL || buffer == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    hal_stub_context_t* stub_ctx = (hal_stub_context_t*)ctx->platform_context;
    if (stub_ctx == NULL) {
        return HAL_STATUS_ERROR;
    }

    /* Find and remove buffer from list */
    for (uint32_t i = 0; i < stub_ctx->rx_buffer_count; i++) {
        if (stub_ctx->rx_buffers[i] == buffer) {
            free(buffer->data);
            free(buffer);

            /* Shift remaining buffers */
            for (uint32_t j = i; j < stub_ctx->rx_buffer_count - 1; j++) {
                stub_ctx->rx_buffers[j] = stub_ctx->rx_buffers[j + 1];
            }
            stub_ctx->rx_buffer_count--;
            return HAL_STATUS_SUCCESS;
        }
    }

    return HAL_STATUS_ERROR;
}

static hal_status_t hal_stub_get_device_info(hal_context_t* ctx, hal_device_info_t* info)
{
    if (ctx == NULL || info == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    memcpy(info, &ctx->device_info, sizeof(hal_device_info_t));
    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_stub_set_promiscuous_mode(hal_context_t* ctx, bool enable)
{
    if (ctx == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    /* Stub implementation: just accept the setting */
    (void)enable;
    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_stub_flush_tx_buffers(hal_context_t* ctx)
{
    if (ctx == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    /* Stub implementation: nothing to flush */
    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_stub_flush_rx_buffers(hal_context_t* ctx)
{
    if (ctx == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    /* Stub implementation: nothing to flush */
    return HAL_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Stub Platform Operations Table                                             */
/* ========================================================================== */

const hal_platform_ops_t hal_stub_ops = {
    .init = hal_stub_init,
    .shutdown = hal_stub_shutdown,
    .send_frame = hal_stub_send_frame,
    .receive_frame = hal_stub_receive_frame,
    .alloc_tx_buffer = hal_stub_alloc_tx_buffer,
    .free_tx_buffer = hal_stub_free_tx_buffer,
    .free_rx_buffer = hal_stub_free_rx_buffer,
    .get_device_info = hal_stub_get_device_info,
    .set_promiscuous_mode = hal_stub_set_promiscuous_mode,
    .flush_tx_buffers = hal_stub_flush_tx_buffers,
    .flush_rx_buffers = hal_stub_flush_rx_buffers
};
