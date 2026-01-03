/**
 * @file dll_types.h
 * @brief EtherCAT Data Link Layer - Type Definitions
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Based on ETG1000.3 - EtherCAT Data Link Layer Services
 *
 * This file contains all type definitions for the DLL layer including
 * enumerations, structures, and callback function types.
 */

#ifndef ETHERCAT_DLL_TYPES_H
#define ETHERCAT_DLL_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup DLL_Types Data Link Layer Types
 * @{
 */

/* ========================================================================== */
/* Status and Error Codes                                                     */
/* ========================================================================== */

/**
 * @brief DLL status codes
 *
 * Return codes for DLL service functions
 */
typedef enum {
    DL_STATUS_SUCCESS = 0x00,        /**< Operation successful */
    DL_STATUS_ERROR = 0x01,          /**< General error */
    DL_STATUS_BUSY = 0x02,           /**< Resource busy */
    DL_STATUS_TIMEOUT = 0x03,        /**< Operation timeout */
    DL_STATUS_INVALID_PARAM = 0x04   /**< Invalid parameter */
} dl_status_t;

/**
 * @brief DLL error codes
 *
 * Detailed error codes for diagnostics
 */
typedef enum {
    DL_ERROR_NONE = 0x00,            /**< No error */
    DL_ERROR_INIT_FAILED = 0x01,     /**< Initialization failed */
    DL_ERROR_INVALID_STATE = 0x02,   /**< Invalid state for operation */
    DL_ERROR_QUEUE_FULL = 0x03,      /**< Queue is full */
    DL_ERROR_QUEUE_EMPTY = 0x04,     /**< Queue is empty */
    DL_ERROR_INVALID_FRAME = 0x05,   /**< Invalid frame format */
    DL_ERROR_TX_TIMEOUT = 0x06,      /**< Transmission timeout */
    DL_ERROR_RX_TIMEOUT = 0x07,      /**< Reception timeout */
    DL_ERROR_HARDWARE = 0x08,        /**< Hardware error */
    DL_ERROR_NO_MEMORY = 0x09,       /**< Memory allocation failed */
    DL_ERROR_INVALID_PARAM = 0x0A,   /**< Invalid parameter */
    DL_ERROR_SEND_FAILED = 0x0B      /**< Frame send failed */
} dl_error_t;

/**
 * @brief DLL state enumeration
 *
 * States of the Data Link Layer state machine
 */
typedef enum {
    DL_STATE_UNINITIALIZED = 0,      /**< Not initialized */
    DL_STATE_INITIALIZED,            /**< Initialized but not started */
    DL_STATE_READY,                  /**< Ready to send/receive */
    DL_STATE_RUNNING,                /**< Active operation */
    DL_STATE_ERROR                   /**< Error state */
} dl_state_t;

/* ========================================================================== */
/* Configuration and Parameters                                               */
/* ========================================================================== */

/**
 * @brief DLL parameter identifiers
 *
 * Parameter IDs for dl_set_parameter() and dl_get_parameter()
 */
typedef enum {
    DL_PARAM_MAC_ADDRESS = 0x0001,       /**< MAC address (6 bytes) */
    DL_PARAM_MAX_FRAME_SIZE = 0x0002,    /**< Maximum frame size (uint16_t) */
    DL_PARAM_CYCLE_TIME = 0x0003,        /**< Cycle time in us (uint32_t) */
    DL_PARAM_TX_QUEUE_SIZE = 0x0004,     /**< TX queue size (uint16_t) */
    DL_PARAM_RX_QUEUE_SIZE = 0x0005,     /**< RX queue size (uint16_t) */
    DL_PARAM_REDUNDANCY_ENABLE = 0x0006, /**< Redundancy enable (bool) */
    DL_PARAM_DC_ENABLE = 0x0007,         /**< Distributed clocks enable (bool) */
    DL_PARAM_NUM_PORTS = 0x0008          /**< Number of ports (uint8_t) */
} dl_param_id_t;

/**
 * @brief DLL configuration structure
 *
 * Configuration parameters for DLL initialization
 */
typedef struct {
    uint8_t mac_address[6];              /**< Master MAC address */
    uint16_t max_frame_size;             /**< Maximum frame size (bytes) */
    uint16_t tx_queue_size;              /**< Transmission queue size */
    uint16_t rx_queue_size;              /**< Reception queue size */
    uint32_t cycle_time_us;              /**< Cycle time in microseconds */
    uint8_t num_ports;                   /**< Number of Ethernet ports */
    bool enable_redundancy;              /**< Enable redundancy support */
    bool enable_distributed_clocks;      /**< Enable distributed clocks */
} dl_config_t;

/* ========================================================================== */
/* Service Primitives                                                         */
/* ========================================================================== */

/**
 * @brief DL_Send.req - Send request structure
 *
 * Parameters for requesting frame transmission
 */
typedef struct {
    uint8_t* frame_data;                 /**< Pointer to frame data */
    uint16_t frame_length;               /**< Length of frame in bytes */
    uint8_t priority;                    /**< Priority level (0-7) */
    void* user_data;                     /**< User context pointer */
} dl_send_req_t;

/**
 * @brief DL_Send.con - Send confirmation structure
 *
 * Parameters for transmission confirmation callback
 */
typedef struct {
    uint8_t status;                      /**< Transmission status */
    void* user_data;                     /**< User context pointer */
} dl_send_con_t;

/**
 * @brief DL_Receive.ind - Receive indication structure
 *
 * Parameters for frame reception indication callback
 */
typedef struct {
    uint8_t* frame_data;                 /**< Pointer to received frame data */
    uint16_t frame_length;               /**< Length of received frame */
    uint64_t timestamp;                  /**< Reception timestamp (nanoseconds) */
    uint8_t port;                        /**< Reception port number */
} dl_receive_ind_t;

/* ========================================================================== */
/* Queue Management                                                           */
/* ========================================================================== */

/**
 * @brief DLL queue entry structure
 *
 * Structure for queue elements
 */
typedef struct {
    uint8_t* buffer;                     /**< Frame buffer */
    uint16_t length;                     /**< Frame length */
    uint8_t priority;                    /**< Priority level */
    void* user_data;                     /**< User context */
    uint64_t timestamp;                  /**< Enqueue timestamp */
} dl_queue_entry_t;

/* ========================================================================== */
/* Statistics and Diagnostics                                                 */
/* ========================================================================== */

/**
 * @brief DLL statistics structure
 *
 * Counters and measurements for diagnostics
 */
typedef struct {
    uint64_t frames_sent;                /**< Total frames sent */
    uint64_t frames_received;            /**< Total frames received */
    uint64_t send_errors;                /**< Send error count */
    uint64_t receive_errors;             /**< Receive error count */
    uint64_t tx_queue_overflows;         /**< TX queue overflow count */
    uint64_t rx_queue_overflows;         /**< RX queue overflow count */
    uint32_t last_cycle_time_us;         /**< Last cycle time (microseconds) */
    uint32_t max_cycle_time_us;          /**< Maximum cycle time observed */
    uint32_t min_cycle_time_us;          /**< Minimum cycle time observed */
} dl_statistics_t;

/* ========================================================================== */
/* Callback Function Types                                                    */
/* ========================================================================== */

/**
 * @brief Send confirmation callback function type
 *
 * Called by DLL when frame transmission completes
 *
 * @param con Pointer to send confirmation structure
 */
typedef void (*dl_send_con_cb_t)(const dl_send_con_t* con);

/**
 * @brief Receive indication callback function type
 *
 * Called by DLL when frame is received
 *
 * @param ind Pointer to receive indication structure
 */
typedef void (*dl_receive_ind_cb_t)(const dl_receive_ind_t* ind);

/**
 * @brief Error callback function type
 *
 * Called by DLL when error occurs
 *
 * @param error Error code
 * @param context Error context information string
 */
typedef void (*dl_error_cb_t)(dl_error_t error, const char* context);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_DLL_TYPES_H */
