/**
 * @file dll_internal.h
 * @brief EtherCAT Data Link Layer - Internal Definitions
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Internal structures and definitions for DLL implementation.
 * This file is not part of the public API.
 */

#ifndef ETHERCAT_DLL_INTERNAL_H
#define ETHERCAT_DLL_INTERNAL_H

#include "ethercat/dll.h"
#include "ethercat/dll_state.h"
#include "ethercat/dll_queue.h"
#include "ethercat/frame_builder.h"
#include "ethercat/frame_parser.h"
#include "ethercat/hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DLL context structure
 *
 * Internal context holding all DLL state and resources
 */
typedef struct {
    /* Configuration */
    dl_config_t config;                  /**< DLL configuration */

    /* State */
    dl_state_t state;                    /**< Current DLL state */

    /* Queues */
    dl_queue_handle_t tx_queue;          /**< Transmission queue */
    dl_queue_handle_t rx_queue;          /**< Reception queue */

    /* Callbacks */
    dl_send_con_cb_t send_callback;      /**< Send confirmation callback */
    dl_receive_ind_cb_t receive_callback; /**< Receive indication callback */
    dl_error_cb_t error_callback;        /**< Error callback */

    /* Statistics */
    dl_statistics_t statistics;          /**< Statistics counters */

    /* Frame buffers */
    uint8_t tx_frame_buffer[ECAT_MAX_FRAME_SIZE];  /**< TX frame buffer */
    uint8_t rx_frame_buffer[ECAT_MAX_FRAME_SIZE];  /**< RX frame buffer */

    /* Frame builder/parser */
    ecat_frame_builder_t frame_builder;  /**< Frame builder context */
    ecat_frame_parser_t frame_parser;    /**< Frame parser context */

    /* Hardware Abstraction Layer */
    hal_frame_buffer_t* hal_tx_buffer;   /**< HAL TX buffer */
    hal_frame_buffer_t* hal_rx_buffer;   /**< HAL RX buffer */

    /* Timing */
    uint64_t last_cycle_start_ns;        /**< Last cycle start time */

    /* Flags */
    bool initialized;                    /**< Initialization flag */
    bool running;                        /**< Running flag */

} dll_context_t;

/**
 * @brief Get DLL context
 *
 * Returns pointer to the global DLL context.
 * This is a singleton pattern for embedded systems.
 *
 * @return Pointer to DLL context
 */
dll_context_t* dll_get_context(void);

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_DLL_INTERNAL_H */
