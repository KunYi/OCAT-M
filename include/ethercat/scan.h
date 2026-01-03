/**
 * @file scan.h
 * @brief EtherCAT Network Scanning - Public API
 * @version 1.0.0
 * @date 2026-01-03
 *
 * This file contains the API for EtherCAT network scanning including
 * slave discovery, topology detection, and EEPROM reading.
 */

#ifndef ETHERCAT_SCAN_H
#define ETHERCAT_SCAN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Scan_API Network Scanning API
 * @{
 */

/* ========================================================================== */
/* EEPROM (SII) Definitions                                                  */
/* ========================================================================== */

/**
 * @brief SII (Slave Information Interface) categories
 */
typedef enum {
    SII_CAT_NOP = 0x00,                 /**< No operation */
    SII_CAT_STRINGS = 0x0A,             /**< String category */
    SII_CAT_DATATYPES = 0x20,           /**< Data types */
    SII_CAT_GENERAL = 0x1E,             /**< General information */
    SII_CAT_FMMU = 0x28,                /**< FMMU configuration */
    SII_CAT_SYNC_MANAGER = 0x29,        /**< Sync Manager configuration */
    SII_CAT_TXPDO = 0x32,               /**< TxPDO (inputs) */
    SII_CAT_RXPDO = 0x33,               /**< RxPDO (outputs) */
    SII_CAT_DC = 0x3C,                  /**< Distributed Clocks */
    SII_CAT_END = 0xFFFF                /**< End of categories */
} sii_category_t;

/**
 * @brief SII General Information structure
 */
typedef struct __attribute__((packed)) {
    uint8_t group_idx;                  /**< Group index */
    uint8_t img_idx;                    /**< Image index */
    uint8_t order_idx;                  /**< Order index */
    uint8_t name_idx;                   /**< Name string index */
    uint8_t reserved1;
    uint8_t coe_details;                /**< CoE details */
    uint8_t foe_details;                /**< FoE details */
    uint8_t eoe_details;                /**< EoE details */
    uint8_t soe_channels;               /**< SoE channels */
    uint8_t ds402_channels;             /**< DS402 channels */
    uint8_t sysman_class;               /**< SysMan class */
    uint8_t flags;                      /**< Flags */
    int16_t current_on_ebus;            /**< Current consumption on E-Bus (mA) */
    uint8_t group_idx_2;                /**< Group index 2 */
    uint8_t reserved2;
    uint16_t physical_port;             /**< Physical port */
    uint16_t physical_memory_addr;      /**< Physical memory address */
    uint8_t reserved3[12];
} sii_general_info_t;

/**
 * @brief SII category header
 */
typedef struct __attribute__((packed)) {
    uint16_t category_type;             /**< Category type */
    uint16_t word_size;                 /**< Size in words */
} sii_category_header_t;

/* ========================================================================== */
/* Scan Status Codes                                                         */
/* ========================================================================== */

/**
 * @brief Scan status codes
 */
typedef enum {
    SCAN_STATUS_SUCCESS = 0x00,         /**< Operation successful */
    SCAN_STATUS_ERROR = 0x01,           /**< General error */
    SCAN_STATUS_TIMEOUT = 0x02,         /**< Operation timeout */
    SCAN_STATUS_INVALID_PARAM = 0x03,   /**< Invalid parameter */
    SCAN_STATUS_NO_SLAVES = 0x04,       /**< No slaves found */
    SCAN_STATUS_EEPROM_ERROR = 0x05     /**< EEPROM read error */
} scan_status_t;

/* ========================================================================== */
/* Slave Discovery                                                           */
/* ========================================================================== */

/**
 * @brief Slave port descriptor bits
 */
#define PORT_DESC_PORT0_MASK    0x03    /**< Port 0 type mask */
#define PORT_DESC_PORT1_MASK    0x0C    /**< Port 1 type mask */
#define PORT_DESC_PORT2_MASK    0x30    /**< Port 2 type mask */
#define PORT_DESC_PORT3_MASK    0xC0    /**< Port 3 type mask */

#define PORT_TYPE_NOT_IMPLEMENTED   0x00 /**< Port not implemented */
#define PORT_TYPE_NOT_CONFIGURED    0x01 /**< Port not configured */
#define PORT_TYPE_EBUS              0x02 /**< E-Bus port */
#define PORT_TYPE_MII               0x03 /**< MII port */

/**
 * @brief Slave discovery information
 */
typedef struct {
    uint16_t position;                  /**< Position in network (0-based) */
    uint16_t auto_inc_address;          /**< Auto-increment address (-position - 1) */
    uint16_t station_address;           /**< Configured station address */
    uint16_t alias_address;             /**< Alias address from EEPROM */
    uint8_t port_descriptors;           /**< Port descriptor byte */
    uint8_t dl_status;                  /**< DL status register */
    bool link_port0;                    /**< Port 0 link status */
    bool link_port1;                    /**< Port 1 link status */
    bool link_port2;                    /**< Port 2 link status */
    bool link_port3;                    /**< Port 3 link status */
} slave_discovery_t;

/* ========================================================================== */
/* Scan API Functions                                                        */
/* ========================================================================== */

/**
 * @brief Initialize network scanning module
 *
 * @return SCAN_STATUS_SUCCESS on success, error code otherwise
 */
scan_status_t scan_init(void);

/**
 * @brief Shutdown network scanning module
 *
 * @return SCAN_STATUS_SUCCESS on success, error code otherwise
 */
scan_status_t scan_shutdown(void);

/**
 * @brief Discover all slaves on the network
 *
 * This function uses BRD (Broadcast Read) commands to count slaves
 * and assign station addresses.
 *
 * @param slave_count Pointer to receive number of slaves found
 * @param timeout_ms Timeout in milliseconds
 * @return SCAN_STATUS_SUCCESS on success, error code otherwise
 */
scan_status_t scan_discover_slaves(uint16_t* slave_count, uint32_t timeout_ms);

/**
 * @brief Get slave discovery information
 *
 * @param position Slave position (0-based)
 * @param discovery Pointer to receive discovery information
 * @return SCAN_STATUS_SUCCESS on success, error code otherwise
 */
scan_status_t scan_get_discovery_info(uint16_t position, slave_discovery_t* discovery);

/**
 * @brief Read slave EEPROM (SII) data
 *
 * @param station_address Slave station address
 * @param word_address EEPROM word address
 * @param data Pointer to receive data (16-bit word)
 * @param timeout_ms Timeout in milliseconds
 * @return SCAN_STATUS_SUCCESS on success, error code otherwise
 */
scan_status_t scan_read_eeprom_word(uint16_t station_address,
                                     uint16_t word_address,
                                     uint16_t* data,
                                     uint32_t timeout_ms);

/**
 * @brief Read slave EEPROM category
 *
 * @param station_address Slave station address
 * @param category Category type to read
 * @param buffer Buffer to receive category data
 * @param buffer_size Size of buffer in bytes
 * @param bytes_read Pointer to receive actual bytes read
 * @param timeout_ms Timeout in milliseconds
 * @return SCAN_STATUS_SUCCESS on success, error code otherwise
 */
scan_status_t scan_read_eeprom_category(uint16_t station_address,
                                         sii_category_t category,
                                         uint8_t* buffer,
                                         uint16_t buffer_size,
                                         uint16_t* bytes_read,
                                         uint32_t timeout_ms);

/**
 * @brief Read slave identification from EEPROM
 *
 * Reads vendor ID, product code, revision, and serial number.
 *
 * @param station_address Slave station address
 * @param vendor_id Pointer to receive vendor ID
 * @param product_code Pointer to receive product code
 * @param revision Pointer to receive revision number
 * @param serial_number Pointer to receive serial number
 * @param timeout_ms Timeout in milliseconds
 * @return SCAN_STATUS_SUCCESS on success, error code otherwise
 */
scan_status_t scan_read_slave_id(uint16_t station_address,
                                   uint32_t* vendor_id,
                                   uint32_t* product_code,
                                   uint32_t* revision,
                                   uint32_t* serial_number,
                                   uint32_t timeout_ms);

/**
 * @brief Read slave name string from EEPROM
 *
 * @param station_address Slave station address
 * @param name Buffer to receive name string
 * @param name_size Size of name buffer
 * @param timeout_ms Timeout in milliseconds
 * @return SCAN_STATUS_SUCCESS on success, error code otherwise
 */
scan_status_t scan_read_slave_name(uint16_t station_address,
                                     char* name,
                                     size_t name_size,
                                     uint32_t timeout_ms);

/**
 * @brief Detect network topology
 *
 * Analyzes port descriptors and link status to determine network topology.
 *
 * @param slave_count Number of slaves
 * @return SCAN_STATUS_SUCCESS on success, error code otherwise
 */
scan_status_t scan_detect_topology(uint16_t slave_count);

/**
 * @brief Assign station addresses to all slaves
 *
 * Assigns sequential station addresses starting from 0x1000.
 *
 * @param slave_count Number of slaves
 * @param timeout_ms Timeout in milliseconds
 * @return SCAN_STATUS_SUCCESS on success, error code otherwise
 */
scan_status_t scan_assign_station_addresses(uint16_t slave_count, uint32_t timeout_ms);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_SCAN_H */
