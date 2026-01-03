/**
 * @file master_internal.h
 * @brief EtherCAT Master - Internal Definitions
 * @version 1.0.0
 * @date 2026-01-03
 */

#ifndef ETHERCAT_MASTER_INTERNAL_H
#define ETHERCAT_MASTER_INTERNAL_H

#include "ethercat/master.h"
#include "ethercat/scan.h"
#include "ethercat/al.h"
#include "ethercat/dll.h"
#include "ethercat/hal.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Constants                                                                 */
/* ========================================================================== */

#define MASTER_MAX_SLAVES           256     /**< Maximum number of slaves */
#define MASTER_STATION_ADDRESS_BASE 0x1000  /**< Base station address */
#define MASTER_DEFAULT_TIMEOUT_MS   1000    /**< Default timeout (ms) */

/* ========================================================================== */
/* Internal Slave Structure                                                  */
/* ========================================================================== */

/**
 * @brief Internal slave structure with full information
 */
typedef struct {
    /* Discovery information */
    uint16_t position;                  /**< Position in network (0-based) */
    uint16_t auto_inc_address;          /**< Auto-increment address */
    uint16_t station_address;           /**< Configured station address */
    uint16_t alias_address;             /**< Alias address from EEPROM */

    /* Port information */
    uint8_t port_descriptors;           /**< Port descriptor byte */
    uint8_t num_ports;                  /**< Number of ports */
    bool link_port[4];                  /**< Link status for each port */

    /* Identification */
    uint32_t vendor_id;                 /**< Vendor ID */
    uint32_t product_code;              /**< Product code */
    uint32_t revision_number;           /**< Revision number */
    uint32_t serial_number;             /**< Serial number */
    char name[64];                      /**< Device name */

    /* Capabilities */
    bool has_mailbox;                   /**< Mailbox support */
    bool has_coe;                       /**< CoE support */
    bool has_foe;                       /**< FoE support */
    bool has_soe;                       /**< SoE support */
    bool has_eoe;                       /**< EoE support */

    /* State */
    uint8_t al_state;                   /**< Current AL state */
    uint16_t al_status_code;            /**< AL status code */

    /* Configuration */
    bool configured;                    /**< Slave is configured */
    bool operational;                   /**< Slave is operational */
} master_slave_t;

/* ========================================================================== */
/* Master Context                                                            */
/* ========================================================================== */

/**
 * @brief Master context structure
 */
typedef struct {
    bool initialized;                   /**< Master initialized */
    master_state_t state;               /**< Current master state */
    master_config_t config;             /**< Master configuration */

    /* Slave information */
    master_slave_t slaves[MASTER_MAX_SLAVES]; /**< Slave array */
    uint16_t slave_count;               /**< Number of slaves */

    /* Topology */
    network_topology_t topology;        /**< Network topology */

    /* Cyclic operation */
    bool cyclic_active;                 /**< Cyclic operation active */
    uint64_t cycle_start_time;          /**< Cycle start time (ns) */
    uint32_t cycle_counter;             /**< Cycle counter */

    /* Statistics */
    uint32_t frames_sent;               /**< Total frames sent */
    uint32_t frames_received;           /**< Total frames received */
    uint32_t errors;                    /**< Total errors */
} master_context_t;

/* ========================================================================== */
/* Internal Functions                                                        */
/* ========================================================================== */

/**
 * @brief Get master context
 *
 * @return Pointer to master context
 */
master_context_t* master_get_context(void);

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_MASTER_INTERNAL_H */
