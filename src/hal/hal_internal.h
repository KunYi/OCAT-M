/**
 * @file hal_internal.h
 * @brief HAL Internal Definitions
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Internal structures and definitions for HAL implementation.
 * This file is not part of the public API.
 */

#ifndef ETHERCAT_HAL_INTERNAL_H
#define ETHERCAT_HAL_INTERNAL_H

#include "ethercat/hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* HAL Context Structure                                                      */
/* ========================================================================== */

typedef struct {
    bool initialized;
    hal_config_t config;
    hal_callbacks_t callbacks;
    hal_statistics_t statistics;
    hal_device_info_t device_info;
    void* platform_context;  /* Platform-specific context */
} hal_context_t;

/* ========================================================================== */
/* Platform-Specific Interface                                                */
/* ========================================================================== */

/**
 * @brief Platform-specific operations interface
 *
 * Each platform implementation must provide these functions.
 */
typedef struct {
    hal_status_t (*init)(hal_context_t* ctx);
    hal_status_t (*shutdown)(hal_context_t* ctx);
    hal_status_t (*send_frame)(hal_context_t* ctx, hal_frame_buffer_t* buffer);
    hal_status_t (*receive_frame)(hal_context_t* ctx, hal_frame_buffer_t** buffer);
    hal_status_t (*alloc_tx_buffer)(hal_context_t* ctx, uint16_t size, hal_frame_buffer_t** buffer);
    hal_status_t (*free_tx_buffer)(hal_context_t* ctx, hal_frame_buffer_t* buffer);
    hal_status_t (*free_rx_buffer)(hal_context_t* ctx, hal_frame_buffer_t* buffer);
    hal_status_t (*get_device_info)(hal_context_t* ctx, hal_device_info_t* info);
    hal_status_t (*set_promiscuous_mode)(hal_context_t* ctx, bool enable);
    hal_status_t (*flush_tx_buffers)(hal_context_t* ctx);
    hal_status_t (*flush_rx_buffers)(hal_context_t* ctx);
} hal_platform_ops_t;

/* Forward declarations for platform implementations */
extern const hal_platform_ops_t hal_linux_raw_socket_ops;
extern const hal_platform_ops_t hal_stub_ops;

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_HAL_INTERNAL_H */
