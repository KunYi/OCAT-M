/**
 * @file dll.h
 * @brief EtherCAT Data Link Layer - Main Interface
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Based on ETG1000.3 - EtherCAT Data Link Layer Services
 *
 * This is the main header file for the EtherCAT Data Link Layer.
 * Include this file to access all DLL functionality.
 */

#ifndef ETHERCAT_DLL_H
#define ETHERCAT_DLL_H

#include "dll_types.h"
#include "dll_config.h"
#include "dll_errors.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup DLL Data Link Layer
 * @{
 */

/* ========================================================================== */
/* Initialization and Shutdown                                                */
/* ========================================================================== */

/**
 * @brief Initialize the Data Link Layer
 *
 * Initializes the DLL with the provided configuration. This must be called
 * before any other DLL functions.
 *
 * @param config Pointer to DLL configuration structure
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 *
 * @note After successful initialization, the DLL is in INITIALIZED state.
 *       Call dl_start() to transition to READY state.
 */
dl_status_t dl_init(const dl_config_t* config);

/**
 * @brief Shutdown the Data Link Layer
 *
 * Shuts down the DLL and releases all resources. After this call,
 * dl_init() must be called again before using the DLL.
 *
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 *
 * @note The DLL must be in INITIALIZED or READY state to call this function.
 */
dl_status_t dl_shutdown(void);

/* ========================================================================== */
/* Control Functions                                                          */
/* ========================================================================== */

/**
 * @brief Start the Data Link Layer
 *
 * Starts the DLL operation. After this call, the DLL is ready to
 * send and receive frames.
 *
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 *
 * @note The DLL must be in INITIALIZED state to call this function.
 *       After successful start, the DLL transitions to READY state.
 */
dl_status_t dl_start(void);

/**
 * @brief Stop the Data Link Layer
 *
 * Stops the DLL operation. Pending frames in queues are discarded.
 *
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 *
 * @note The DLL must be in READY or RUNNING state to call this function.
 *       After successful stop, the DLL transitions to INITIALIZED state.
 */
dl_status_t dl_stop(void);

/**
 * @brief Reset the Data Link Layer
 *
 * Resets the DLL from ERROR state back to INITIALIZED state.
 * All queues are flushed and statistics are reset.
 *
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 *
 * @note The DLL must be in ERROR state to call this function.
 */
dl_status_t dl_reset(void);

/**
 * @brief Get current DLL state
 *
 * Returns the current state of the DLL state machine.
 *
 * @return Current DLL state
 */
dl_state_t dl_get_state(void);

/* ========================================================================== */
/* Parameter Access                                                           */
/* ========================================================================== */

/**
 * @brief Set DLL parameter
 *
 * Sets a runtime parameter of the DLL. Some parameters can only be
 * set when the DLL is not running.
 *
 * @param param_id Parameter identifier
 * @param value Pointer to parameter value
 * @param length Length of parameter value
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dl_set_parameter(uint16_t param_id, const void* value, uint16_t length);

/**
 * @brief Get DLL parameter
 *
 * Gets a runtime parameter of the DLL.
 *
 * @param param_id Parameter identifier
 * @param value Pointer to buffer for parameter value
 * @param length Pointer to length (in: buffer size, out: actual size)
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dl_get_parameter(uint16_t param_id, void* value, uint16_t* length);

/* ========================================================================== */
/* Frame Transmission                                                         */
/* ========================================================================== */

/**
 * @brief Send an EtherCAT frame
 *
 * Requests transmission of an EtherCAT frame. The frame is queued
 * and transmitted asynchronously. A confirmation callback is invoked
 * when transmission completes.
 *
 * @param req Pointer to send request structure
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 *
 * @note The frame_data buffer must remain valid until the confirmation
 *       callback is invoked.
 * @note The DLL must be in READY or RUNNING state.
 */
dl_status_t dl_send_req(const dl_send_req_t* req);

/**
 * @brief Register send confirmation callback
 *
 * Registers a callback function to be called when frame transmission
 * completes. Only one callback can be registered at a time.
 *
 * @param callback Callback function pointer (NULL to unregister)
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dl_register_send_callback(dl_send_con_cb_t callback);

/* ========================================================================== */
/* Frame Reception                                                            */
/* ========================================================================== */

/**
 * @brief Register receive indication callback
 *
 * Registers a callback function to be called when a frame is received.
 * Only one callback can be registered at a time.
 *
 * @param callback Callback function pointer (NULL to unregister)
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dl_register_receive_callback(dl_receive_ind_cb_t callback);

/* ========================================================================== */
/* Queue Management                                                           */
/* ========================================================================== */

/**
 * @brief Get number of frames in TX queue
 *
 * Returns the current number of frames waiting in the transmission queue.
 *
 * @return Number of queued frames
 */
uint16_t dl_get_tx_queue_count(void);

/**
 * @brief Get number of frames in RX queue
 *
 * Returns the current number of frames waiting in the reception queue.
 *
 * @return Number of queued frames
 */
uint16_t dl_get_rx_queue_count(void);

/**
 * @brief Flush TX queue
 *
 * Discards all pending frames in the transmission queue.
 *
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dl_flush_tx_queue(void);

/**
 * @brief Flush RX queue
 *
 * Discards all pending frames in the reception queue.
 *
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dl_flush_rx_queue(void);

/* ========================================================================== */
/* Statistics and Diagnostics                                                 */
/* ========================================================================== */

/**
 * @brief Get DLL statistics
 *
 * Retrieves current statistics and diagnostic counters.
 *
 * @param stats Pointer to statistics structure to fill
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dl_get_statistics(dl_statistics_t* stats);

/**
 * @brief Reset DLL statistics
 *
 * Resets all statistics counters to zero.
 *
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dl_reset_statistics(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_DLL_H */
