/**
 * @file hal.h
 * @brief Hardware Abstraction Layer Interface
 * @version 1.0.0
 * @date 2026-01-03
 *
 * This file defines the Hardware Abstraction Layer (HAL) interface for
 * EtherCAT frame transmission and reception. The HAL provides a platform-
 * independent interface that can be implemented for different hardware
 * platforms and operating systems.
 *
 * Supported platforms:
 * - Linux (raw socket, packet mmap)
 * - Windows (Npcap/WinPcap)
 * - FreeRTOS (lwIP)
 * - Bare-metal (custom driver)
 * - Stub (testing)
 */

#ifndef ETHERCAT_HAL_H
#define ETHERCAT_HAL_H

#include "ethercat/hal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* HAL Initialization and Configuration                                       */
/* ========================================================================== */

/**
 * @brief Initialize the Hardware Abstraction Layer
 *
 * This function initializes the HAL with the specified configuration.
 * It must be called before any other HAL functions.
 *
 * @param config Pointer to HAL configuration structure
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_init(const hal_config_t* config);

/**
 * @brief Shutdown the Hardware Abstraction Layer
 *
 * This function shuts down the HAL and releases all resources.
 *
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_shutdown(void);

/**
 * @brief Check if HAL is initialized
 *
 * @return true if initialized, false otherwise
 */
bool hal_is_initialized(void);

/**
 * @brief Get HAL configuration defaults
 *
 * This function fills the configuration structure with default values.
 *
 * @param config Pointer to configuration structure to fill
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_config_init_defaults(hal_config_t* config);

/* ========================================================================== */
/* HAL Frame Transmission                                                     */
/* ========================================================================== */

/**
 * @brief Send a frame
 *
 * This function sends a frame through the network interface.
 * The behavior depends on the blocking_mode configuration:
 * - Blocking mode: Waits until frame is sent or timeout occurs
 * - Non-blocking mode: Returns immediately, callback invoked when complete
 *
 * @param buffer Pointer to frame buffer to send
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_send_frame(hal_frame_buffer_t* buffer);

/**
 * @brief Allocate a TX frame buffer
 *
 * This function allocates a frame buffer for transmission.
 * The buffer must be freed with hal_free_tx_buffer() after use.
 *
 * @param size Size of buffer to allocate
 * @param buffer Pointer to store allocated buffer
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_alloc_tx_buffer(uint16_t size, hal_frame_buffer_t** buffer);

/**
 * @brief Free a TX frame buffer
 *
 * This function frees a previously allocated TX frame buffer.
 *
 * @param buffer Pointer to buffer to free
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_free_tx_buffer(hal_frame_buffer_t* buffer);

/* ========================================================================== */
/* HAL Frame Reception                                                        */
/* ========================================================================== */

/**
 * @brief Receive a frame
 *
 * This function receives a frame from the network interface.
 * The behavior depends on the blocking_mode configuration:
 * - Blocking mode: Waits until frame is received or timeout occurs
 * - Non-blocking mode: Returns immediately with HAL_STATUS_WOULD_BLOCK if no frame
 *
 * @param buffer Pointer to store received frame buffer
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_receive_frame(hal_frame_buffer_t** buffer);

/**
 * @brief Free an RX frame buffer
 *
 * This function frees a received frame buffer and returns it to the pool.
 *
 * @param buffer Pointer to buffer to free
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_free_rx_buffer(hal_frame_buffer_t* buffer);

/* ========================================================================== */
/* HAL Callback Registration                                                  */
/* ========================================================================== */

/**
 * @brief Register HAL callbacks
 *
 * This function registers callbacks for asynchronous events.
 *
 * @param callbacks Pointer to callbacks structure
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_register_callbacks(const hal_callbacks_t* callbacks);

/**
 * @brief Unregister HAL callbacks
 *
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_unregister_callbacks(void);

/* ========================================================================== */
/* HAL Device Information                                                     */
/* ========================================================================== */

/**
 * @brief Get device information
 *
 * This function retrieves information about the network device.
 *
 * @param info Pointer to device information structure to fill
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_get_device_info(hal_device_info_t* info);

/**
 * @brief Get MAC address
 *
 * This function retrieves the MAC address of the network interface.
 *
 * @param mac_address Buffer to store MAC address (6 bytes)
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_get_mac_address(uint8_t mac_address[6]);

/**
 * @brief Set MAC address
 *
 * This function sets the MAC address of the network interface.
 * Note: Not all platforms support changing MAC address.
 *
 * @param mac_address MAC address to set (6 bytes)
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_set_mac_address(const uint8_t mac_address[6]);

/**
 * @brief Check link status
 *
 * @return true if link is up, false otherwise
 */
bool hal_is_link_up(void);

/* ========================================================================== */
/* HAL Statistics                                                             */
/* ========================================================================== */

/**
 * @brief Get HAL statistics
 *
 * This function retrieves statistics from the HAL layer.
 *
 * @param stats Pointer to statistics structure to fill
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_get_statistics(hal_statistics_t* stats);

/**
 * @brief Reset HAL statistics
 *
 * This function resets all statistics counters to zero.
 *
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_reset_statistics(void);

/* ========================================================================== */
/* HAL Control Functions                                                      */
/* ========================================================================== */

/**
 * @brief Enable promiscuous mode
 *
 * This function enables promiscuous mode on the network interface.
 *
 * @param enable true to enable, false to disable
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_set_promiscuous_mode(bool enable);

/**
 * @brief Flush TX buffers
 *
 * This function flushes all pending TX buffers.
 *
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_flush_tx_buffers(void);

/**
 * @brief Flush RX buffers
 *
 * This function flushes all pending RX buffers.
 *
 * @return HAL_STATUS_SUCCESS on success, error code otherwise
 */
hal_status_t hal_flush_rx_buffers(void);

/* ========================================================================== */
/* HAL Time Functions                                                         */
/* ========================================================================== */

/**
 * @brief Get current time in nanoseconds
 *
 * This function returns the current system time in nanoseconds.
 * The time is monotonic and suitable for measuring time intervals.
 *
 * @return Current time in nanoseconds
 */
uint64_t hal_get_time_ns(void);

/**
 * @brief Get current time in milliseconds
 *
 * This function returns the current system time in milliseconds.
 * The time is monotonic and suitable for measuring time intervals.
 *
 * @return Current time in milliseconds
 */
uint64_t hal_get_time_ms(void);

/**
 * @brief Sleep for specified milliseconds
 *
 * This function suspends execution for the specified number of milliseconds.
 *
 * @param ms Number of milliseconds to sleep
 */
void hal_sleep_ms(uint32_t ms);

/**
 * @brief Sleep for specified microseconds
 *
 * This function suspends execution for the specified number of microseconds.
 *
 * @param us Number of microseconds to sleep
 */
void hal_sleep_us(uint32_t us);

/* ========================================================================== */
/* HAL Platform-Specific Functions                                           */
/* ========================================================================== */

/**
 * @brief Get platform type
 *
 * @return Platform type
 */
hal_platform_t hal_get_platform(void);

/**
 * @brief Get platform name string
 *
 * @return Platform name string
 */
const char* hal_get_platform_name(void);

/**
 * @brief Get HAL version string
 *
 * @return Version string
 */
const char* hal_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_HAL_H */
