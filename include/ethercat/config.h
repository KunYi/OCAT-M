/**
 * @file config.h
 * @brief EtherCAT Slave Configuration - Public API
 * @version 1.0.0
 * @date 2026-01-03
 *
 * This file contains the API for EtherCAT slave configuration including
 * Sync Manager, PDO mapping, Mailbox, and FMMU configuration.
 */

#ifndef ETHERCAT_CONFIG_H
#define ETHERCAT_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ethercat/al_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Config_API Slave Configuration API
 * @{
 */

/* ========================================================================== */
/* Configuration Status Codes                                                */
/* ========================================================================== */

/**
 * @brief Configuration status codes
 */
typedef enum {
    CONFIG_STATUS_SUCCESS = 0x00,       /**< Operation successful */
    CONFIG_STATUS_ERROR = 0x01,         /**< General error */
    CONFIG_STATUS_TIMEOUT = 0x02,       /**< Operation timeout */
    CONFIG_STATUS_INVALID_PARAM = 0x03, /**< Invalid parameter */
    CONFIG_STATUS_NOT_SUPPORTED = 0x04, /**< Operation not supported */
    CONFIG_STATUS_EEPROM_ERROR = 0x05   /**< EEPROM read error */
} config_status_t;

/* ========================================================================== */
/* FMMU (Fieldbus Memory Management Unit) Configuration                      */
/* ========================================================================== */

/**
 * @brief FMMU configuration structure
 */
typedef struct __attribute__((packed)) {
    uint32_t logical_start_address;     /**< Logical start address */
    uint16_t length;                    /**< Length in bytes */
    uint8_t logical_start_bit;          /**< Logical start bit */
    uint8_t logical_end_bit;            /**< Logical end bit */
    uint16_t physical_start_address;    /**< Physical start address */
    uint8_t physical_start_bit;         /**< Physical start bit */
    uint8_t read_enable;                /**< Read enable */
    uint8_t write_enable;               /**< Write enable */
    uint8_t enable;                     /**< FMMU enable */
} fmmu_config_t;

#define FMMU_CONFIG_BASE_ADDR 0x0600
#define FMMU_CONFIG_SIZE 16
#define FMMU_MAX_COUNT 16

/**
 * @brief SII FMMU category structure
 */
typedef struct __attribute__((packed)) {
    uint8_t fmmu_usage;                 /**< FMMU usage */
    uint8_t reserved[15];
} sii_fmmu_t;

/* ========================================================================== */
/* Sync Manager Configuration from EEPROM                                    */
/* ========================================================================== */

/**
 * @brief SII Sync Manager category structure
 */
typedef struct __attribute__((packed)) {
    uint16_t physical_start_address;    /**< Physical start address */
    uint16_t length;                    /**< Length in bytes */
    uint8_t control_register;           /**< Control register */
    uint8_t status_register;            /**< Status register (reserved) */
    uint8_t enable;                     /**< Enable flags */
    uint8_t sm_type;                    /**< SM type */
} sii_sync_manager_t;

/* ========================================================================== */
/* PDO (Process Data Object) Configuration                                   */
/* ========================================================================== */

/**
 * @brief PDO entry structure
 */
typedef struct __attribute__((packed)) {
    uint16_t index;                     /**< Object dictionary index */
    uint8_t subindex;                   /**< Object dictionary subindex */
    uint8_t name_idx;                   /**< Name string index */
    uint8_t data_type;                  /**< Data type */
    uint8_t bit_length;                 /**< Bit length */
    uint16_t flags;                     /**< Flags */
} pdo_entry_t;

/**
 * @brief PDO structure
 */
typedef struct __attribute__((packed)) {
    uint16_t index;                     /**< PDO index */
    uint8_t entry_count;                /**< Number of entries */
    uint8_t sync_manager;               /**< Sync Manager index */
    uint8_t dc_sync;                    /**< DC sync */
    uint8_t name_idx;                   /**< Name string index */
    uint16_t flags;                     /**< Flags */
} pdo_t;

/**
 * @brief SII PDO category header
 */
typedef struct __attribute__((packed)) {
    uint16_t pdo_index;                 /**< PDO index */
    uint8_t entry_count;                /**< Number of entries */
    uint8_t sync_manager;               /**< Sync Manager index */
    uint8_t dc_sync;                    /**< DC sync */
    uint8_t name_idx;                   /**< Name string index */
    uint16_t flags;                     /**< Flags */
} sii_pdo_t;

/**
 * @brief SII PDO entry
 */
typedef struct __attribute__((packed)) {
    uint16_t index;                     /**< Object dictionary index */
    uint8_t subindex;                   /**< Object dictionary subindex */
    uint8_t name_idx;                   /**< Name string index */
    uint8_t data_type;                  /**< Data type */
    uint8_t bit_length;                 /**< Bit length */
    uint16_t flags;                     /**< Flags */
} sii_pdo_entry_t;

/* ========================================================================== */
/* Mailbox Configuration                                                     */
/* ========================================================================== */

/**
 * @brief Mailbox configuration structure
 */
typedef struct {
    uint16_t write_address;             /**< Mailbox write address (Master->Slave) */
    uint16_t write_size;                /**< Mailbox write size */
    uint16_t read_address;              /**< Mailbox read address (Slave->Master) */
    uint16_t read_size;                 /**< Mailbox read size */
    uint8_t write_sm;                   /**< Write Sync Manager index */
    uint8_t read_sm;                    /**< Read Sync Manager index */
    bool supports_coe;                  /**< Supports CoE */
    bool supports_foe;                  /**< Supports FoE */
    bool supports_soe;                  /**< Supports SoE */
    bool supports_eoe;                  /**< Supports EoE */
} mailbox_config_t;

/* ========================================================================== */
/* Slave Configuration Structure                                             */
/* ========================================================================== */

/**
 * @brief Complete slave configuration
 */
typedef struct {
    uint16_t station_address;           /**< Station address */

    /* Sync Manager configuration */
    sm_config_t sync_managers[SM_MAX_COUNT];
    uint8_t sm_count;

    /* FMMU configuration */
    fmmu_config_t fmmus[FMMU_MAX_COUNT];
    uint8_t fmmu_count;

    /* Mailbox configuration */
    mailbox_config_t mailbox;
    bool has_mailbox;

    /* PDO configuration */
    pdo_t* rxpdos;                      /**< RxPDO array (Master->Slave) */
    uint16_t rxpdo_count;
    pdo_t* txpdos;                      /**< TxPDO array (Slave->Master) */
    uint16_t txpdo_count;

    /* Process data mapping */
    uint32_t input_offset;              /**< Input data offset in process image */
    uint32_t input_size;                /**< Input data size in bytes */
    uint32_t output_offset;             /**< Output data offset in process image */
    uint32_t output_size;               /**< Output data size in bytes */

    bool configured;                    /**< Configuration complete flag */
} slave_config_t;

/* ========================================================================== */
/* Configuration API Functions                                               */
/* ========================================================================== */

/**
 * @brief Initialize configuration module
 *
 * @return CONFIG_STATUS_SUCCESS on success, error code otherwise
 */
config_status_t config_init(void);

/**
 * @brief Shutdown configuration module
 *
 * @return CONFIG_STATUS_SUCCESS on success, error code otherwise
 */
config_status_t config_shutdown(void);

/**
 * @brief Read Sync Manager configuration from EEPROM
 *
 * @param station_address Slave station address
 * @param sm_configs Array to receive SM configurations
 * @param max_count Maximum number of SMs to read
 * @param sm_count Pointer to receive actual SM count
 * @param timeout_ms Timeout in milliseconds
 * @return CONFIG_STATUS_SUCCESS on success, error code otherwise
 */
config_status_t config_read_sync_managers(uint16_t station_address,
                                           sii_sync_manager_t* sm_configs,
                                           uint8_t max_count,
                                           uint8_t* sm_count,
                                           uint32_t timeout_ms);

/**
 * @brief Write Sync Manager configuration to slave
 *
 * @param station_address Slave station address
 * @param sm_index Sync Manager index (0-15)
 * @param sm_config Sync Manager configuration
 * @param timeout_ms Timeout in milliseconds
 * @return CONFIG_STATUS_SUCCESS on success, error code otherwise
 */
config_status_t config_write_sync_manager(uint16_t station_address,
                                           uint8_t sm_index,
                                           const sm_config_t* sm_config,
                                           uint32_t timeout_ms);

/**
 * @brief Read FMMU configuration from EEPROM
 *
 * @param station_address Slave station address
 * @param fmmu_configs Array to receive FMMU configurations
 * @param max_count Maximum number of FMMUs to read
 * @param fmmu_count Pointer to receive actual FMMU count
 * @param timeout_ms Timeout in milliseconds
 * @return CONFIG_STATUS_SUCCESS on success, error code otherwise
 */
config_status_t config_read_fmmus(uint16_t station_address,
                                   sii_fmmu_t* fmmu_configs,
                                   uint8_t max_count,
                                   uint8_t* fmmu_count,
                                   uint32_t timeout_ms);

/**
 * @brief Write FMMU configuration to slave
 *
 * @param station_address Slave station address
 * @param fmmu_index FMMU index (0-15)
 * @param fmmu_config FMMU configuration
 * @param timeout_ms Timeout in milliseconds
 * @return CONFIG_STATUS_SUCCESS on success, error code otherwise
 */
config_status_t config_write_fmmu(uint16_t station_address,
                                   uint8_t fmmu_index,
                                   const fmmu_config_t* fmmu_config,
                                   uint32_t timeout_ms);

/**
 * @brief Read PDO configuration from EEPROM
 *
 * @param station_address Slave station address
 * @param is_txpdo True for TxPDO (inputs), false for RxPDO (outputs)
 * @param pdos Array to receive PDO configurations
 * @param max_count Maximum number of PDOs to read
 * @param pdo_count Pointer to receive actual PDO count
 * @param timeout_ms Timeout in milliseconds
 * @return CONFIG_STATUS_SUCCESS on success, error code otherwise
 */
config_status_t config_read_pdos(uint16_t station_address,
                                  bool is_txpdo,
                                  sii_pdo_t* pdos,
                                  uint16_t max_count,
                                  uint16_t* pdo_count,
                                  uint32_t timeout_ms);

/**
 * @brief Configure mailbox communication
 *
 * Reads mailbox configuration from EEPROM and configures the
 * corresponding Sync Managers.
 *
 * @param station_address Slave station address
 * @param mailbox_config Pointer to receive mailbox configuration
 * @param timeout_ms Timeout in milliseconds
 * @return CONFIG_STATUS_SUCCESS on success, error code otherwise
 */
config_status_t config_setup_mailbox(uint16_t station_address,
                                      mailbox_config_t* mailbox_config,
                                      uint32_t timeout_ms);

/**
 * @brief Configure slave for process data exchange
 *
 * This function performs complete slave configuration including:
 * - Reading configuration from EEPROM
 * - Configuring Sync Managers
 * - Configuring FMMUs
 * - Setting up mailbox (if supported)
 * - Configuring PDO mappings
 *
 * @param station_address Slave station address
 * @param slave_config Pointer to receive complete slave configuration
 * @param timeout_ms Timeout in milliseconds
 * @return CONFIG_STATUS_SUCCESS on success, error code otherwise
 */
config_status_t config_configure_slave(uint16_t station_address,
                                        slave_config_t* slave_config,
                                        uint32_t timeout_ms);

/**
 * @brief Calculate process data offsets for all slaves
 *
 * This function calculates the logical memory layout for process data
 * exchange, assigning offsets to each slave's inputs and outputs.
 *
 * @param slave_configs Array of slave configurations
 * @param slave_count Number of slaves
 * @param total_input_size Pointer to receive total input size
 * @param total_output_size Pointer to receive total output size
 * @return CONFIG_STATUS_SUCCESS on success, error code otherwise
 */
config_status_t config_calculate_process_data(slave_config_t* slave_configs,
                                               uint16_t slave_count,
                                               uint32_t* total_input_size,
                                               uint32_t* total_output_size);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_CONFIG_H */
