/**
 * @file process_data.h
 * @brief EtherCAT Process Data - Public API
 * @version 1.0.0
 * @date 2026-01-03
 *
 * This file contains the API for EtherCAT process data exchange including
 * cyclic operation, LRW command, and working counter validation.
 */

#ifndef ETHERCAT_PROCESS_DATA_H
#define ETHERCAT_PROCESS_DATA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ProcessData_API Process Data API
 * @{
 */

/* ========================================================================== */
/* Process Data Status Codes                                                 */
/* ========================================================================== */

/**
 * @brief Process data status codes
 */
typedef enum {
    PD_STATUS_SUCCESS = 0x00,           /**< Operation successful */
    PD_STATUS_ERROR = 0x01,             /**< General error */
    PD_STATUS_TIMEOUT = 0x02,           /**< Operation timeout */
    PD_STATUS_INVALID_PARAM = 0x03,     /**< Invalid parameter */
    PD_STATUS_NOT_INITIALIZED = 0x04,   /**< Not initialized */
    PD_STATUS_WKC_ERROR = 0x05,         /**< Working counter error */
    PD_STATUS_BUFFER_OVERFLOW = 0x06,   /**< Buffer overflow */
    PD_STATUS_REDUNDANCY_LOST = 0x07,   /**< Redundancy lost */
    PD_STATUS_PRIMARY_FAILED = 0x08,    /**< Primary port failed */
    PD_STATUS_SECONDARY_FAILED = 0x09   /**< Secondary port failed */
} pd_status_t;

/* ========================================================================== */
/* Redundancy Support (Phase 5.2 - Optional)                                */
/* ========================================================================== */

/**
 * @brief Redundancy mode
 */
typedef enum {
    PD_REDUNDANCY_NONE = 0,             /**< No redundancy */
    PD_REDUNDANCY_CABLE,                /**< Cable redundancy (ring topology) */
    PD_REDUNDANCY_FRAME,                /**< Frame redundancy (dual send) */
    PD_REDUNDANCY_HOT_CONNECT           /**< Hot connect support */
} pd_redundancy_mode_t;

/**
 * @brief Port selection for redundancy
 */
typedef enum {
    PD_PORT_PRIMARY = 0,                /**< Primary port */
    PD_PORT_SECONDARY = 1,              /**< Secondary port */
    PD_PORT_AUTO = 2                    /**< Automatic selection */
} pd_port_select_t;

/**
 * @brief Redundancy configuration
 */
typedef struct {
    pd_redundancy_mode_t mode;          /**< Redundancy mode */
    pd_port_select_t active_port;       /**< Active port selection */
    bool auto_switch;                   /**< Automatic port switching */
    uint32_t switch_threshold_ms;       /**< Switch threshold (milliseconds) */
    uint32_t health_check_interval_ms;  /**< Health check interval */
} pd_redundancy_config_t;

/**
 * @brief Port status information
 */
typedef struct {
    bool link_up;                       /**< Link status */
    bool active;                        /**< Port is active */
    uint64_t frames_sent;               /**< Frames sent on this port */
    uint64_t frames_received;           /**< Frames received on this port */
    uint64_t errors;                    /**< Error count */
    uint32_t last_wkc;                  /**< Last working counter */
    uint64_t last_success_time_ns;      /**< Last successful exchange time */
} pd_port_status_t;

/* ========================================================================== */
/* Process Data Image                                                        */
/* ========================================================================== */

/**
 * @brief Process data image structure
 */
typedef struct {
    /* Data buffers */
    uint8_t* input_data;                /**< Input process data (from slaves) */
    uint32_t input_size;                /**< Input data size in bytes */
    uint8_t* output_data;               /**< Output process data (to slaves) */
    uint32_t output_size;               /**< Output data size in bytes */
    uint32_t logical_address;           /**< Logical memory start address */

    /* Redundancy support (Phase 5.2) */
    pd_redundancy_config_t redundancy;  /**< Redundancy configuration */
    pd_port_status_t port_status[2];    /**< Status for primary/secondary ports */
    pd_port_select_t current_port;      /**< Currently active port */

    /* Frame management */
    uint8_t frame_index;                /**< Frame index for identification */
    bool frame_pending;                 /**< Frame transmission pending */
} pd_image_t;

/**
 * @brief Slave process data mapping
 */
typedef struct {
    uint16_t station_address;           /**< Slave station address */
    uint32_t input_offset;              /**< Input data offset in image */
    uint32_t input_size;                /**< Input data size in bytes */
    uint32_t output_offset;             /**< Output data offset in image */
    uint32_t output_size;               /**< Output data size in bytes */
    uint32_t logical_input_address;     /**< Logical input address */
    uint32_t logical_output_address;    /**< Logical output address */
} pd_slave_mapping_t;

/* ========================================================================== */
/* Cyclic Operation Statistics                                               */
/* ========================================================================== */

/**
 * @brief Cyclic operation statistics
 */
typedef struct {
    /* Cycle statistics */
    uint64_t cycle_count;               /**< Total cycles executed */
    uint64_t wkc_error_count;           /**< Working counter errors */
    uint64_t timeout_count;             /**< Timeout errors */
    uint32_t min_cycle_time_us;         /**< Minimum cycle time (microseconds) */
    uint32_t max_cycle_time_us;         /**< Maximum cycle time (microseconds) */
    uint32_t avg_cycle_time_us;         /**< Average cycle time (microseconds) */
    uint16_t last_working_counter;      /**< Last working counter value */
    uint16_t expected_working_counter;  /**< Expected working counter */

    /* Redundancy statistics (Phase 5.2) */
    uint64_t port_switch_count;         /**< Number of port switches */
    uint64_t redundancy_loss_count;     /**< Redundancy loss events */
    uint64_t primary_error_count;       /**< Primary port errors */
    uint64_t secondary_error_count;     /**< Secondary port errors */
    pd_port_select_t active_port;       /**< Currently active port */
    bool redundancy_available;          /**< Redundancy is available */
} pd_statistics_t;

/* ========================================================================== */
/* API Functions                                                             */
/* ========================================================================== */

/**
 * @brief Initialize process data module
 *
 * @return PD_STATUS_SUCCESS on success, error code otherwise
 */
pd_status_t pd_init(void);

/**
 * @brief Shutdown process data module
 *
 * @return PD_STATUS_SUCCESS on success, error code otherwise
 */
pd_status_t pd_shutdown(void);

/**
 * @brief Allocate process data image
 *
 * @param image Pointer to image structure
 * @param input_size Input data size in bytes
 * @param output_size Output data size in bytes
 * @param redundancy Redundancy configuration (NULL for no redundancy)
 * @return PD_STATUS_SUCCESS on success, error code otherwise
 */
pd_status_t pd_allocate_image(pd_image_t* image,
                               uint32_t input_size,
                               uint32_t output_size,
                               const pd_redundancy_config_t* redundancy);

/**
 * @brief Free process data image
 *
 * @param image Pointer to image structure
 * @return PD_STATUS_SUCCESS on success, error code otherwise
 */
pd_status_t pd_free_image(pd_image_t* image);

/**
 * @brief Map slave process data to image
 *
 * @param slave_count Number of slaves
 * @param mappings Array of slave mappings
 * @param image Pointer to image structure
 * @return PD_STATUS_SUCCESS on success, error code otherwise
 */
pd_status_t pd_map_slave(uint16_t slave_count,
                          const pd_slave_mapping_t* mappings,
                          pd_image_t* image);

/**
 * @brief Exchange process data (LRW command)
 *
 * This function sends output data and receives input data in a single frame
 * using the LRW (Logical Read/Write) command.
 *
 * @param image Pointer to process data image
 * @param working_counter Pointer to receive working counter
 * @param timeout_ms Timeout in milliseconds
 * @return PD_STATUS_SUCCESS on success, error code otherwise
 */
pd_status_t pd_exchange(pd_image_t* image,
                         uint16_t* working_counter,
                         uint32_t timeout_ms);

/**
 * @brief Exchange process data on specific port (Phase 5.2)
 *
 * @param image Pointer to process data image
 * @param port Port selection
 * @param working_counter Pointer to receive working counter
 * @param timeout_ms Timeout in milliseconds
 * @return PD_STATUS_SUCCESS on success, error code otherwise
 */
pd_status_t pd_exchange_port(pd_image_t* image,
                              pd_port_select_t port,
                              uint16_t* working_counter,
                              uint32_t timeout_ms);

/**
 * @brief Validate working counter
 *
 * @param expected Expected working counter value
 * @param actual Actual working counter value
 * @return true if valid, false otherwise
 */
bool pd_validate_wkc(uint16_t expected, uint16_t actual);

/**
 * @brief Switch active port (Phase 5.2)
 *
 * @param image Pointer to process data image
 * @param new_port New port selection
 * @return PD_STATUS_SUCCESS on success, error code otherwise
 */
pd_status_t pd_switch_port(pd_image_t* image, pd_port_select_t new_port);

/**
 * @brief Check port health (Phase 5.2)
 *
 * @param image Pointer to process data image
 * @param port Port to check
 * @param healthy Pointer to receive health status
 * @return PD_STATUS_SUCCESS on success, error code otherwise
 */
pd_status_t pd_check_port_health(pd_image_t* image,
                                  pd_port_select_t port,
                                  bool* healthy);

/**
 * @brief Get port status (Phase 5.2)
 *
 * @param image Pointer to process data image
 * @param port Port selection
 * @param status Pointer to receive port status
 * @return PD_STATUS_SUCCESS on success, error code otherwise
 */
pd_status_t pd_get_port_status(pd_image_t* image,
                                pd_port_select_t port,
                                pd_port_status_t* status);

/**
 * @brief Get process data statistics
 *
 * @param stats Pointer to receive statistics
 * @return PD_STATUS_SUCCESS on success, error code otherwise
 */
pd_status_t pd_get_statistics(pd_statistics_t* stats);

/**
 * @brief Reset process data statistics
 *
 * @return PD_STATUS_SUCCESS on success, error code otherwise
 */
pd_status_t pd_reset_statistics(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_PROCESS_DATA_H */
