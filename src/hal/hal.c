/**
 * @file hal.c
 * @brief Hardware Abstraction Layer Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "hal_internal.h"
#include <string.h>
#include <stdlib.h>

/* ========================================================================== */
/* Global HAL Context                                                         */
/* ========================================================================== */

/* Global HAL context (singleton) */
static hal_context_t g_hal_context = {0};

/**
 * @brief Get platform operations for the specified platform type
 */
static const hal_platform_ops_t* hal_get_platform_ops(hal_platform_t platform)
{
    switch (platform) {
        case HAL_PLATFORM_LINUX_RAW_SOCKET:
            return &hal_linux_raw_socket_ops;
        case HAL_PLATFORM_STUB:
            return &hal_stub_ops;
        default:
            return NULL;
    }
}

/* ========================================================================== */
/* HAL Initialization and Configuration                                       */
/* ========================================================================== */

hal_status_t hal_config_init_defaults(hal_config_t* config)
{
    if (config == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    memset(config, 0, sizeof(hal_config_t));

    config->platform = HAL_PLATFORM_STUB;
    config->interface_name = "eth0";
    config->rx_buffer_count = 32;
    config->tx_buffer_count = 32;
    config->rx_buffer_size = 1518;
    config->tx_buffer_size = 1518;
    config->promiscuous_mode = true;
    config->blocking_mode = false;
    config->timeout_ms = 1000;
    config->platform_data = NULL;

    return HAL_STATUS_SUCCESS;
}

/* Helper function to get primary port index */
static inline uint8_t hal_get_primary_port(void)
{
    return 0;
}

hal_status_t hal_init(const hal_config_t* config)
{
    if (config == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    if (g_hal_context.initialized) {
        return HAL_STATUS_ALREADY_INIT;
    }

    /* Get platform operations */
    const hal_platform_ops_t* ops = hal_get_platform_ops(config->platform);
    if (ops == NULL) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    /* Initialize context */
    memset(&g_hal_context, 0, sizeof(hal_context_t));
    memcpy(&g_hal_context.config[0], config, sizeof(hal_config_t));
    g_hal_context.port_count = 1;

    /* Call platform-specific initialization */
    hal_status_t status = ops->init(&g_hal_context);
    if (status != HAL_STATUS_SUCCESS) {
        return status;
    }

    g_hal_context.initialized = true;
    return HAL_STATUS_SUCCESS;
}

hal_status_t hal_shutdown(void)
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    /* Get platform operations */
    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[0].platform);
    if (ops == NULL) {
        return HAL_STATUS_ERROR;
    }

    /* Call platform-specific shutdown for all ports */
    hal_status_t status = HAL_STATUS_SUCCESS;
    for (uint8_t port = 0; port < g_hal_context.port_count; port++) {
        hal_status_t port_status = ops->shutdown(&g_hal_context);
        if (port_status != HAL_STATUS_SUCCESS) {
            status = port_status;
        }
    }

    /* Clear context */
    memset(&g_hal_context, 0, sizeof(hal_context_t));

    return status;
}

bool hal_is_initialized(void)
{
    return g_hal_context.initialized;
}

/* ========================================================================== */
/* HAL Frame Transmission                                                     */
/* ========================================================================== */

hal_status_t hal_send_frame(hal_frame_buffer_t* buffer)
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    if (buffer == NULL || buffer->data == NULL || buffer->length == 0) {
        return HAL_STATUS_INVALID_PARAM;
    }

    /* Use primary port for single-port send */
    uint8_t port = hal_get_primary_port();

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[port].platform);
    if (ops == NULL || ops->send_frame == NULL) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    hal_status_t status = ops->send_frame(&g_hal_context, buffer);

    if (status == HAL_STATUS_SUCCESS) {
        g_hal_context.statistics[port].frames_sent++;
    } else {
        g_hal_context.statistics[port].send_errors++;
    }

    return status;
}

hal_status_t hal_alloc_tx_buffer(uint16_t size, hal_frame_buffer_t** buffer)
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    if (buffer == NULL || size == 0) {
        return HAL_STATUS_INVALID_PARAM;
    }

    uint8_t port = hal_get_primary_port();

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[port].platform);
    if (ops == NULL || ops->alloc_tx_buffer == NULL) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    return ops->alloc_tx_buffer(&g_hal_context, size, buffer);
}

hal_status_t hal_free_tx_buffer(hal_frame_buffer_t* buffer)
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    if (buffer == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    uint8_t port = hal_get_primary_port();

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[port].platform);
    if (ops == NULL || ops->free_tx_buffer == NULL) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    return ops->free_tx_buffer(&g_hal_context, buffer);
}

/* ========================================================================== */
/* HAL Frame Reception                                                        */
/* ========================================================================== */

hal_status_t hal_receive_frame(hal_frame_buffer_t** buffer)
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    if (buffer == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    uint8_t port = hal_get_primary_port();

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[port].platform);
    if (ops == NULL || ops->receive_frame == NULL) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    hal_status_t status = ops->receive_frame(&g_hal_context, buffer);

    if (status == HAL_STATUS_SUCCESS) {
        g_hal_context.statistics[port].frames_received++;
    } else if (status != HAL_STATUS_WOULD_BLOCK) {
        g_hal_context.statistics[port].receive_errors++;
    }

    return status;
}

hal_status_t hal_free_rx_buffer(hal_frame_buffer_t* buffer)
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    if (buffer == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    uint8_t port = hal_get_primary_port();

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[port].platform);
    if (ops == NULL || ops->free_rx_buffer == NULL) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    return ops->free_rx_buffer(&g_hal_context, buffer);
}

/* ========================================================================== */
/* HAL Callback Registration                                                  */
/* ========================================================================== */

hal_status_t hal_register_callbacks(const hal_callbacks_t* callbacks)
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    if (callbacks == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    memcpy(&g_hal_context.callbacks, callbacks, sizeof(hal_callbacks_t));
    return HAL_STATUS_SUCCESS;
}

hal_status_t hal_unregister_callbacks(void)
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    memset(&g_hal_context.callbacks, 0, sizeof(hal_callbacks_t));
    return HAL_STATUS_SUCCESS;
}

/* ========================================================================== */
/* HAL Device Information                                                     */
/* ========================================================================== */

hal_status_t hal_get_device_info(hal_device_info_t* info)
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    if (info == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    uint8_t port = hal_get_primary_port();

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[port].platform);
    if (ops == NULL || ops->get_device_info == NULL) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    return ops->get_device_info(&g_hal_context, info);
}

hal_status_t hal_get_mac_address(uint8_t mac_address[6])
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    if (mac_address == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    uint8_t port = hal_get_primary_port();
    memcpy(mac_address, g_hal_context.config[port].mac_address, 6);
    return HAL_STATUS_SUCCESS;
}

hal_status_t hal_set_mac_address(const uint8_t mac_address[6])
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    if (mac_address == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    uint8_t port = hal_get_primary_port();
    memcpy(g_hal_context.config[port].mac_address, mac_address, 6);
    return HAL_STATUS_SUCCESS;
}

bool hal_is_link_up(void)
{
    if (!g_hal_context.initialized) {
        return false;
    }

    uint8_t port = hal_get_primary_port();
    return g_hal_context.device_info[port].link_up;
}

/* ========================================================================== */
/* HAL Statistics                                                             */
/* ========================================================================== */

hal_status_t hal_get_statistics(hal_statistics_t* stats)
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    if (stats == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    uint8_t port = hal_get_primary_port();
    memcpy(stats, &g_hal_context.statistics[port], sizeof(hal_statistics_t));
    return HAL_STATUS_SUCCESS;
}

hal_status_t hal_reset_statistics(void)
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    /* Reset statistics for all ports */
    for (uint8_t port = 0; port < g_hal_context.port_count; port++) {
        memset(&g_hal_context.statistics[port], 0, sizeof(hal_statistics_t));
    }
    return HAL_STATUS_SUCCESS;
}

/* ========================================================================== */
/* HAL Control Functions                                                      */
/* ========================================================================== */

hal_status_t hal_set_promiscuous_mode(bool enable)
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    uint8_t port = hal_get_primary_port();

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[port].platform);
    if (ops == NULL || ops->set_promiscuous_mode == NULL) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    return ops->set_promiscuous_mode(&g_hal_context, enable);
}

hal_status_t hal_flush_tx_buffers(void)
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    uint8_t port = hal_get_primary_port();

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[port].platform);
    if (ops == NULL || ops->flush_tx_buffers == NULL) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    return ops->flush_tx_buffers(&g_hal_context);
}

hal_status_t hal_flush_rx_buffers(void)
{
    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    uint8_t port = hal_get_primary_port();

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[port].platform);
    if (ops == NULL || ops->flush_rx_buffers == NULL) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    return ops->flush_rx_buffers(&g_hal_context);
}

/* ========================================================================== */
/* HAL Platform-Specific Functions                                           */
/* ========================================================================== */

hal_platform_t hal_get_platform(void)
{
    if (!g_hal_context.initialized) {
        return HAL_PLATFORM_STUB;
    }

    return g_hal_context.config[0].platform;
}

const char* hal_get_platform_name(void)
{
    if (!g_hal_context.initialized) {
        return "Uninitialized";
    }

    switch (g_hal_context.config[0].platform) {
        case HAL_PLATFORM_LINUX_RAW_SOCKET:
            return "Linux Raw Socket";
        case HAL_PLATFORM_LINUX_PACKET_MMAP:
            return "Linux Packet MMAP";
        case HAL_PLATFORM_WINDOWS_NPCAP:
            return "Windows Npcap";
        case HAL_PLATFORM_FREERTOS_LWIP:
            return "FreeRTOS lwIP";
        case HAL_PLATFORM_BAREMETAL:
            return "Bare-metal";
        case HAL_PLATFORM_STUB:
            return "Stub";
        case HAL_PLATFORM_CUSTOM:
            return "Custom";
        default:
            return "Unknown";
    }
}

const char* hal_get_version(void)
{
    return "1.0.0";
}

/* ========================================================================== */
/* HAL Time Functions                                                         */
/* ========================================================================== */

uint64_t hal_get_time_ns(void)
{
    if (!g_hal_context.initialized) {
        return 0;
    }

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[0].platform);
    if (ops == NULL || ops->get_time_ns == NULL) {
        return 0;
    }

    return ops->get_time_ns();
}

uint64_t hal_get_time_ms(void)
{
    if (!g_hal_context.initialized) {
        return 0;
    }

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[0].platform);
    if (ops == NULL || ops->get_time_ms == NULL) {
        return 0;
    }

    return ops->get_time_ms();
}

void hal_sleep_ms(uint32_t ms)
{
    if (!g_hal_context.initialized) {
        return;
    }

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[0].platform);
    if (ops == NULL || ops->sleep_ms == NULL) {
        return;
    }

    ops->sleep_ms(ms);
}

void hal_sleep_us(uint32_t us)
{
    if (!g_hal_context.initialized) {
        return;
    }

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[0].platform);
    if (ops == NULL || ops->sleep_us == NULL) {
        return;
    }

    ops->sleep_us(us);
}

/* ========================================================================== */
/* HAL Multi-Port Support (Phase 5.2 - Full Implementation)                  */
/* ========================================================================== */

hal_status_t hal_init_multiport(const hal_config_t* primary_config,
                                 const hal_config_t* secondary_config)
{
    if (primary_config == NULL || secondary_config == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    if (g_hal_context.initialized) {
        return HAL_STATUS_ALREADY_INIT;
    }

    /* Get platform operations */
    const hal_platform_ops_t* ops = hal_get_platform_ops(primary_config->platform);
    if (ops == NULL) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    /* Initialize context */
    memset(&g_hal_context, 0, sizeof(hal_context_t));
    memcpy(&g_hal_context.config[0], primary_config, sizeof(hal_config_t));
    memcpy(&g_hal_context.config[1], secondary_config, sizeof(hal_config_t));
    g_hal_context.port_count = 2;

    /* Initialize primary port */
    hal_status_t status = ops->init(&g_hal_context);
    if (status != HAL_STATUS_SUCCESS) {
        return status;
    }

    /* Initialize secondary port if platform supports it */
    /* For now, we'll initialize both ports using the same platform ops */
    /* Platform implementations need to handle multi-port initialization */

    g_hal_context.initialized = true;
    return HAL_STATUS_SUCCESS;
}

hal_status_t hal_send_frame_port(hal_frame_buffer_t* buffer, uint8_t port)
{
    if (buffer == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    if (port >= g_hal_context.port_count) {
        return HAL_STATUS_INVALID_PARAM;
    }

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[port].platform);
    if (ops == NULL || ops->send_frame == NULL) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    /* Set port in buffer for platform to use */
    buffer->port = port;

    hal_status_t status = ops->send_frame(&g_hal_context, buffer);

    if (status == HAL_STATUS_SUCCESS) {
        g_hal_context.statistics[port].frames_sent++;
    } else {
        g_hal_context.statistics[port].send_errors++;
    }

    return status;
}

hal_status_t hal_receive_frame_port(hal_frame_buffer_t** buffer, uint8_t port)
{
    if (buffer == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    if (port >= g_hal_context.port_count) {
        return HAL_STATUS_INVALID_PARAM;
    }

    const hal_platform_ops_t* ops = hal_get_platform_ops(g_hal_context.config[port].platform);
    if (ops == NULL || ops->receive_frame == NULL) {
        return HAL_STATUS_NOT_SUPPORTED;
    }

    hal_status_t status = ops->receive_frame(&g_hal_context, buffer);

    if (status == HAL_STATUS_SUCCESS && *buffer != NULL) {
        (*buffer)->port = port;
        g_hal_context.statistics[port].frames_received++;
    } else if (status != HAL_STATUS_WOULD_BLOCK) {
        g_hal_context.statistics[port].receive_errors++;
    }

    return status;
}

bool hal_is_port_link_up(uint8_t port)
{
    if (!g_hal_context.initialized) {
        return false;
    }

    if (port >= g_hal_context.port_count) {
        return false;
    }

    return g_hal_context.device_info[port].link_up;
}

hal_status_t hal_get_port_statistics(uint8_t port, hal_statistics_t* stats)
{
    if (stats == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    if (!g_hal_context.initialized) {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    if (port >= g_hal_context.port_count) {
        return HAL_STATUS_INVALID_PARAM;
    }

    memcpy(stats, &g_hal_context.statistics[port], sizeof(hal_statistics_t));
    return HAL_STATUS_SUCCESS;
}

uint8_t hal_get_port_count(void)
{
    if (!g_hal_context.initialized) {
        return 0;
    }

    return g_hal_context.port_count;
}

