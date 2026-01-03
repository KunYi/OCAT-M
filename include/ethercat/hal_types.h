/**
 * @file hal_types.h
 * @brief Hardware Abstraction Layer Type Definitions
 * @version 1.0.0
 * @date 2026-01-03
 *
 * This file defines the types and structures used by the HAL layer.
 * The HAL provides a platform-independent interface for frame transmission
 * and reception, allowing the EtherCAT stack to be ported to different
 * hardware platforms and operating systems.
 */

#ifndef ETHERCAT_HAL_TYPES_H
#define ETHERCAT_HAL_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* HAL Status Codes                                                           */
/* ========================================================================== */

/**
 * @brief HAL operation status codes
 */
typedef enum {
    HAL_STATUS_SUCCESS = 0,      /**< Operation successful */
    HAL_STATUS_ERROR,            /**< General error */
    HAL_STATUS_INVALID_PARAM,    /**< Invalid parameter */
    HAL_STATUS_NOT_INITIALIZED,  /**< HAL not initialized */
    HAL_STATUS_ALREADY_INIT,     /**< HAL already initialized */
    HAL_STATUS_NO_DEVICE,        /**< Network device not found */
    HAL_STATUS_DEVICE_BUSY,      /**< Device is busy */
    HAL_STATUS_TIMEOUT,          /**< Operation timeout */
    HAL_STATUS_NO_BUFFER,        /**< No buffer available */
    HAL_STATUS_WOULD_BLOCK,      /**< Operation would block */
    HAL_STATUS_NOT_SUPPORTED     /**< Operation not supported */
} hal_status_t;

/* ========================================================================== */
/* HAL Platform Types                                                         */
/* ========================================================================== */

/**
 * @brief Platform types supported by HAL
 */
typedef enum {
    HAL_PLATFORM_LINUX_RAW_SOCKET,  /**< Linux raw socket */
    HAL_PLATFORM_LINUX_PACKET_MMAP, /**< Linux packet mmap (zero-copy) */
    HAL_PLATFORM_WINDOWS_NPCAP,     /**< Windows Npcap/WinPcap */
    HAL_PLATFORM_FREERTOS_LWIP,     /**< FreeRTOS with lwIP */
    HAL_PLATFORM_BAREMETAL,         /**< Bare-metal (custom driver) */
    HAL_PLATFORM_STUB,              /**< Stub implementation (testing) */
    HAL_PLATFORM_CUSTOM             /**< Custom platform */
} hal_platform_t;

/* ========================================================================== */
/* HAL Configuration                                                          */
/* ========================================================================== */

/**
 * @brief HAL configuration structure
 */
typedef struct {
    hal_platform_t platform;        /**< Platform type */
    const char* interface_name;     /**< Network interface name (e.g., "eth0") */
    uint8_t mac_address[6];         /**< MAC address of the interface */
    uint32_t rx_buffer_count;       /**< Number of RX buffers */
    uint32_t tx_buffer_count;       /**< Number of TX buffers */
    uint32_t rx_buffer_size;        /**< Size of each RX buffer */
    uint32_t tx_buffer_size;        /**< Size of each TX buffer */
    bool promiscuous_mode;          /**< Enable promiscuous mode */
    bool blocking_mode;             /**< Enable blocking I/O */
    uint32_t timeout_ms;            /**< Timeout for blocking operations (ms) */
    void* platform_data;            /**< Platform-specific data */
} hal_config_t;

/* ========================================================================== */
/* HAL Frame Buffer                                                           */
/* ========================================================================== */

/**
 * @brief HAL frame buffer structure
 *
 * This structure represents a frame buffer used for transmission or reception.
 * The buffer is managed by the HAL layer and should not be freed by the user.
 */
typedef struct {
    uint8_t* data;                  /**< Pointer to frame data */
    uint16_t length;                /**< Length of frame data */
    uint16_t capacity;              /**< Capacity of buffer */
    uint64_t timestamp;             /**< Timestamp (nanoseconds) */
    uint8_t port;                   /**< Port number (for multi-port devices) */
    void* user_data;                /**< User context pointer */
    void* hal_private;              /**< HAL private data (internal use) */
} hal_frame_buffer_t;

/* ========================================================================== */
/* HAL Statistics                                                             */
/* ========================================================================== */

/**
 * @brief HAL statistics structure
 */
typedef struct {
    uint64_t frames_sent;           /**< Total frames sent */
    uint64_t frames_received;       /**< Total frames received */
    uint64_t send_errors;           /**< Send errors */
    uint64_t receive_errors;        /**< Receive errors */
    uint64_t rx_buffer_overflows;   /**< RX buffer overflows */
    uint64_t tx_buffer_overflows;   /**< TX buffer overflows */
    uint64_t crc_errors;            /**< CRC errors */
    uint64_t dropped_frames;        /**< Dropped frames */
} hal_statistics_t;

/* ========================================================================== */
/* HAL Callbacks                                                              */
/* ========================================================================== */

/**
 * @brief Frame transmission complete callback
 *
 * This callback is invoked when a frame transmission is complete.
 *
 * @param buffer Pointer to the transmitted frame buffer
 * @param status Transmission status
 */
typedef void (*hal_tx_complete_callback_t)(hal_frame_buffer_t* buffer,
                                            hal_status_t status);

/**
 * @brief Frame reception callback
 *
 * This callback is invoked when a frame is received.
 *
 * @param buffer Pointer to the received frame buffer
 */
typedef void (*hal_rx_callback_t)(hal_frame_buffer_t* buffer);

/**
 * @brief Error callback
 *
 * This callback is invoked when an error occurs in the HAL layer.
 *
 * @param error_code Error code
 * @param error_message Error message string
 */
typedef void (*hal_error_callback_t)(hal_status_t error_code,
                                      const char* error_message);

/* ========================================================================== */
/* HAL Callbacks Structure                                                    */
/* ========================================================================== */

/**
 * @brief HAL callbacks structure
 */
typedef struct {
    hal_tx_complete_callback_t tx_complete;  /**< TX complete callback */
    hal_rx_callback_t rx_callback;           /**< RX callback */
    hal_error_callback_t error_callback;     /**< Error callback */
} hal_callbacks_t;

/* ========================================================================== */
/* HAL Device Information                                                     */
/* ========================================================================== */

/**
 * @brief HAL device information structure
 */
typedef struct {
    char interface_name[64];        /**< Interface name */
    uint8_t mac_address[6];         /**< MAC address */
    uint32_t mtu;                   /**< Maximum Transmission Unit */
    uint32_t speed_mbps;            /**< Link speed in Mbps */
    bool link_up;                   /**< Link status */
    bool full_duplex;               /**< Duplex mode */
    hal_platform_t platform;        /**< Platform type */
} hal_device_info_t;

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_HAL_TYPES_H */
