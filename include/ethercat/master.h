/**
 * @file master.h
 * @brief EtherCAT Master - Public API
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Based on ETG1000 Series - EtherCAT Master Implementation
 *
 * This file contains the public API for the EtherCAT Master including
 * initialization, network scanning, slave configuration, and cyclic operation.
 */

#ifndef ETHERCAT_MASTER_H
#define ETHERCAT_MASTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "process_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Master_API EtherCAT Master API
 * @{
 */

/* ========================================================================== */
/* Master Status Codes                                                       */
/* ========================================================================== */

/**
 * @brief Master status codes
 */
typedef enum {
    MASTER_STATUS_SUCCESS = 0x00,       /**< Operation successful */
    MASTER_STATUS_ERROR = 0x01,         /**< General error */
    MASTER_STATUS_TIMEOUT = 0x02,       /**< Operation timeout */
    MASTER_STATUS_INVALID_PARAM = 0x03, /**< Invalid parameter */
    MASTER_STATUS_NOT_INITIALIZED = 0x04, /**< Master not initialized */
    MASTER_STATUS_BUSY = 0x05,          /**< Master busy */
    MASTER_STATUS_NO_SLAVES = 0x06,     /**< No slaves found */
    MASTER_STATUS_SLAVE_ERROR = 0x07    /**< Slave error */
} master_status_t;

/* ========================================================================== */
/* Master States                                                             */
/* ========================================================================== */

/**
 * @brief Master operational states
 */
typedef enum {
    MASTER_STATE_IDLE = 0x00,           /**< Idle state */
    MASTER_STATE_INIT = 0x01,           /**< Initializing */
    MASTER_STATE_SCANNING = 0x02,       /**< Scanning network */
    MASTER_STATE_CONFIGURING = 0x03,    /**< Configuring slaves */
    MASTER_STATE_PREOP = 0x04,          /**< Pre-Operational */
    MASTER_STATE_SAFEOP = 0x05,         /**< Safe-Operational */
    MASTER_STATE_OP = 0x06,             /**< Operational */
    MASTER_STATE_ERROR = 0xFF           /**< Error state */
} master_state_t;

/* ========================================================================== */
/* Slave Information                                                         */
/* ========================================================================== */

/**
 * @brief Slave device information
 */
typedef struct {
    uint16_t station_address;           /**< Configured station address */
    uint16_t alias_address;             /**< Alias address from EEPROM */
    uint32_t vendor_id;                 /**< Vendor ID */
    uint32_t product_code;              /**< Product code */
    uint32_t revision_number;           /**< Revision number */
    uint32_t serial_number;             /**< Serial number */
    uint16_t position;                  /**< Position in network (0-based) */
    uint8_t port_descriptors;           /**< Port descriptor byte */
    uint8_t num_ports;                  /**< Number of ports */
    bool has_mailbox;                   /**< Mailbox support */
    bool has_coe;                       /**< CoE support */
    bool has_foe;                       /**< FoE support */
    bool has_soe;                       /**< SoE support */
    bool has_eoe;                       /**< EoE support */
    char name[64];                      /**< Device name from EEPROM */
} slave_info_t;

/**
 * @brief Network topology information
 */
typedef struct {
    uint16_t slave_count;               /**< Total number of slaves */
    uint16_t working_counter_expected;  /**< Expected working counter */
    uint32_t cycle_time_us;             /**< Cycle time in microseconds */
    bool topology_valid;                /**< Topology is valid */
    bool dc_available;                  /**< Distributed Clocks available */
} network_topology_t;

/* ========================================================================== */
/* Master Configuration                                                      */
/* ========================================================================== */

/**
 * @brief Master configuration structure
 */
typedef struct {
    const char* interface_name;         /**< Network interface name */
    uint8_t mac_address[6];             /**< Master MAC address */
    uint32_t cycle_time_us;             /**< Desired cycle time (microseconds) */
    uint32_t scan_timeout_ms;           /**< Network scan timeout (milliseconds) */
    uint32_t state_change_timeout_ms;   /**< State change timeout (milliseconds) */
    bool enable_dc;                     /**< Enable Distributed Clocks */
    bool auto_configure;                /**< Auto-configure slaves */
} master_config_t;

/* ========================================================================== */
/* Master API Functions                                                      */
/* ========================================================================== */

/**
 * @brief Initialize EtherCAT Master
 *
 * @param config Pointer to master configuration
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_init(const master_config_t* config);

/**
 * @brief Shutdown EtherCAT Master
 *
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_shutdown(void);

/**
 * @brief Scan EtherCAT network for slaves
 *
 * This function scans the network to discover all connected slaves,
 * reads their EEPROM information, and builds the network topology.
 *
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_scan_network(void);

/**
 * @brief Get number of slaves found
 *
 * @param count Pointer to receive slave count
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_get_slave_count(uint16_t* count);

/**
 * @brief Get slave information by position
 *
 * @param position Slave position (0-based)
 * @param info Pointer to receive slave information
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_get_slave_info(uint16_t position, slave_info_t* info);

/**
 * @brief Get network topology information
 *
 * @param topology Pointer to receive topology information
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_get_topology(network_topology_t* topology);

/**
 * @brief Configure all slaves
 *
 * This function configures all discovered slaves with their
 * sync managers, mailboxes, and PDO mappings.
 *
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_configure_slaves(void);

/**
 * @brief Request state change for all slaves
 *
 * @param requested_state Target AL state
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_request_state(uint8_t requested_state);

/**
 * @brief Get current master state
 *
 * @param state Pointer to receive master state
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_get_state(master_state_t* state);

/**
 * @brief Start cyclic operation
 *
 * This function starts the cyclic process data exchange.
 * Master must be in SAFEOP or OP state.
 *
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_start_cyclic(void);

/**
 * @brief Stop cyclic operation
 *
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_stop_cyclic(void);

/**
 * @brief Process one cycle
 *
 * This function sends process data to slaves and receives responses.
 * Should be called periodically at the configured cycle time.
 *
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_process_cycle(void);

/**
 * @brief Allocate process data buffers
 *
 * This function allocates input/output buffers based on slave configuration.
 *
 * @param redundancy Redundancy configuration (NULL for no redundancy)
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_allocate_process_data(const pd_redundancy_config_t* redundancy);

/**
 * @brief Free process data buffers
 *
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_free_process_data(void);

/**
 * @brief Get process data image
 *
 * @param image Pointer to receive process data image pointer
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_get_process_data_image(pd_image_t** image);

/**
 * @brief Write output process data for a slave
 *
 * @param position Slave position (0-based)
 * @param data Pointer to output data
 * @param length Data length in bytes
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_write_slave_output(uint16_t position,
                                            const uint8_t* data,
                                            uint32_t length);

/**
 * @brief Read input process data from a slave
 *
 * @param position Slave position (0-based)
 * @param data Pointer to receive input data
 * @param length Data length in bytes
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_read_slave_input(uint16_t position,
                                         uint8_t* data,
                                         uint32_t length);

/**
 * @brief Get cyclic operation statistics
 *
 * @param stats Pointer to receive statistics
 * @return MASTER_STATUS_SUCCESS on success, error code otherwise
 */
master_status_t master_get_cyclic_statistics(pd_statistics_t* stats);

/**
 * @brief Get master version string
 *
 * @return Pointer to version string
 */
const char* master_get_version(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_MASTER_H */
