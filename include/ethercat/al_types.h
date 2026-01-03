/**
 * @file al_types.h
 * @brief EtherCAT Application Layer - Type Definitions
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Based on ETG1000.5 - EtherCAT Application Layer Services
 *
 * This file contains all type definitions for the Application Layer including
 * states, registers, sync managers, and mailbox structures.
 */

#ifndef ETHERCAT_AL_TYPES_H
#define ETHERCAT_AL_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup AL_Types Application Layer Types
 * @{
 */

/* ========================================================================== */
/* AL Status and Error Codes                                                 */
/* ========================================================================== */

/**
 * @brief AL status codes
 */
typedef enum {
    AL_STATUS_SUCCESS = 0x00,           /**< Operation successful */
    AL_STATUS_ERROR = 0x01,             /**< General error */
    AL_STATUS_BUSY = 0x02,              /**< Resource busy */
    AL_STATUS_TIMEOUT = 0x03,           /**< Operation timeout */
    AL_STATUS_INVALID_PARAM = 0x04,     /**< Invalid parameter */
    AL_STATUS_INVALID_STATE = 0x05,     /**< Invalid state for operation */
    AL_STATUS_NOT_INITIALIZED = 0x06,   /**< AL not initialized */
    AL_STATUS_NO_MEMORY = 0x07,         /**< Memory allocation failed */
    AL_STATUS_NOT_SUPPORTED = 0x08      /**< Operation not supported */
} al_status_t;

/* ========================================================================== */
/* AL States                                                                  */
/* ========================================================================== */

/**
 * @brief Application Layer states
 */
typedef enum {
    AL_STATE_INIT = 0x01,               /**< Init state */
    AL_STATE_PREOP = 0x02,              /**< Pre-Operational state */
    AL_STATE_BOOT = 0x03,               /**< Bootstrap state */
    AL_STATE_SAFEOP = 0x04,             /**< Safe-Operational state */
    AL_STATE_OP = 0x08                  /**< Operational state */
} al_state_t;

/**
 * @brief AL state transition requests
 */
typedef enum {
    AL_TRANS_INIT_TO_PREOP,             /**< Init -> Pre-Op */
    AL_TRANS_PREOP_TO_INIT,             /**< Pre-Op -> Init */
    AL_TRANS_PREOP_TO_SAFEOP,           /**< Pre-Op -> Safe-Op */
    AL_TRANS_SAFEOP_TO_PREOP,           /**< Safe-Op -> Pre-Op */
    AL_TRANS_SAFEOP_TO_OP,              /**< Safe-Op -> Op */
    AL_TRANS_OP_TO_SAFEOP,              /**< Op -> Safe-Op */
    AL_TRANS_PREOP_TO_BOOT,             /**< Pre-Op -> Bootstrap */
    AL_TRANS_BOOT_TO_INIT               /**< Bootstrap -> Init */
} al_state_transition_t;

/* ========================================================================== */
/* AL Control and Status Registers                                           */
/* ========================================================================== */

/**
 * @brief AL Control register structure (Master -> Slave)
 */
typedef struct __attribute__((packed)) {
    uint16_t state : 4;                 /**< Requested state (bits 0-3) */
    uint16_t ack : 1;                   /**< Acknowledge error (bit 4) */
    uint16_t request_id : 1;            /**< Request ID toggle (bit 5) */
    uint16_t reserved : 10;             /**< Reserved (bits 6-15) */
} al_control_t;

#define AL_CONTROL_REG_ADDR 0x0120

/**
 * @brief AL Status register structure (Slave -> Master)
 */
typedef struct __attribute__((packed)) {
    uint16_t state : 4;                 /**< Current state (bits 0-3) */
    uint16_t error : 1;                 /**< Error flag (bit 4) */
    uint16_t id : 1;                    /**< ID toggle (bit 5) */
    uint16_t reserved : 10;             /**< Reserved (bits 6-15) */
} al_status_reg_t;

#define AL_STATUS_REG_ADDR 0x0130

/**
 * @brief AL Status Code register (error codes)
 */
typedef enum {
    AL_STATUS_CODE_NO_ERROR = 0x0000,
    AL_STATUS_CODE_UNSPECIFIED = 0x0001,
    AL_STATUS_CODE_NO_MEMORY = 0x0002,
    AL_STATUS_CODE_INVALID_DEVICE_SETUP = 0x0003,
    AL_STATUS_CODE_INVALID_MAILBOX_CONFIG = 0x0004,
    AL_STATUS_CODE_INVALID_MAILBOX_CONFIG_PREOP = 0x0005,
    AL_STATUS_CODE_INVALID_SM_CONFIG = 0x0006,
    AL_STATUS_CODE_INVALID_SM_CONFIG_PREOP = 0x0007,
    AL_STATUS_CODE_INVALID_OUTPUT_CONFIG = 0x0008,
    AL_STATUS_CODE_INVALID_INPUT_CONFIG = 0x0009,
    AL_STATUS_CODE_INVALID_WATCHDOG_CONFIG = 0x000A,
    AL_STATUS_CODE_SLAVE_NEEDS_COLD_START = 0x000B,
    AL_STATUS_CODE_SLAVE_NEEDS_INIT = 0x000C,
    AL_STATUS_CODE_SLAVE_NEEDS_PREOP = 0x000D,
    AL_STATUS_CODE_SLAVE_NEEDS_SAFEOP = 0x000E,
    AL_STATUS_CODE_INVALID_INPUT_MAPPING = 0x000F,
    AL_STATUS_CODE_INVALID_OUTPUT_MAPPING = 0x0010,
    AL_STATUS_CODE_INCONSISTENT_SETTINGS = 0x0011,
    AL_STATUS_CODE_FREERUN_NOT_SUPPORTED = 0x0012,
    AL_STATUS_CODE_SYNC_NOT_SUPPORTED = 0x0013,
    AL_STATUS_CODE_FREERUN_NEEDS_3BUFFER = 0x0014,
    AL_STATUS_CODE_BACKGROUND_WATCHDOG = 0x0015,
    AL_STATUS_CODE_NO_VALID_INPUTS = 0x0016,
    AL_STATUS_CODE_NO_VALID_OUTPUTS = 0x0017,
    AL_STATUS_CODE_SYNC_ERROR = 0x0018,
    AL_STATUS_CODE_SYNC_WATCHDOG = 0x0019,
    AL_STATUS_CODE_INVALID_SYNC_TYPES = 0x001A,
    AL_STATUS_CODE_INVALID_OUTPUT_CONFIG_SAFEOP = 0x001B,
    AL_STATUS_CODE_INVALID_INPUT_CONFIG_SAFEOP = 0x001C,
    AL_STATUS_CODE_INVALID_WATCHDOG_CONFIG_SAFEOP = 0x001D,
    AL_STATUS_CODE_SLAVE_NEEDS_BOOT = 0x001E
} al_status_code_t;

#define AL_STATUS_CODE_REG_ADDR 0x0134

/* ========================================================================== */
/* Sync Manager (SM)                                                         */
/* ========================================================================== */

/**
 * @brief Sync Manager types
 */
typedef enum {
    SM_TYPE_MAILBOX_WRITE = 0,          /**< Mailbox write (Master -> Slave) */
    SM_TYPE_MAILBOX_READ = 1,           /**< Mailbox read (Slave -> Master) */
    SM_TYPE_PROCESS_DATA_WRITE = 2,     /**< Process data outputs (Master -> Slave) */
    SM_TYPE_PROCESS_DATA_READ = 3       /**< Process data inputs (Slave -> Master) */
} sm_type_t;

/**
 * @brief Sync Manager configuration structure
 */
typedef struct __attribute__((packed)) {
    uint16_t physical_start_address;    /**< Physical start address */
    uint16_t length;                    /**< Length in bytes */
    uint8_t control;                    /**< Control register */
    uint8_t status;                     /**< Status register */
    uint8_t enable;                     /**< Enable register */
    uint8_t pdi_control;                /**< PDI control register */
} sm_config_t;

#define SM_CONFIG_BASE_ADDR 0x0800
#define SM_CONFIG_SIZE 8
#define SM_MAX_COUNT 16

/**
 * @brief SM Control register bits
 */
typedef struct {
    uint8_t operation_mode : 2;         /**< Operation mode (bits 0-1) */
    uint8_t direction : 1;              /**< Direction: 0=read, 1=write (bit 2) */
    uint8_t ecat_event : 1;             /**< EtherCAT event enable (bit 3) */
    uint8_t dls_user_event : 1;         /**< DLS user event enable (bit 4) */
    uint8_t reserved : 1;               /**< Reserved (bit 5) */
    uint8_t watchdog : 1;               /**< Watchdog trigger (bit 6) */
    uint8_t reserved2 : 1;              /**< Reserved (bit 7) */
} sm_control_bits_t;

/**
 * @brief SM operation modes
 */
typedef enum {
    SM_OP_MODE_3BUFFER = 0x00,          /**< 3-buffer mode */
    SM_OP_MODE_1BUFFER = 0x02           /**< 1-buffer mode (mailbox) */
} sm_operation_mode_t;

/* ========================================================================== */
/* Mailbox Protocol                                                          */
/* ========================================================================== */

/**
 * @brief Mailbox protocol types
 */
typedef enum {
    MBOX_TYPE_ERROR = 0x00,             /**< Error response */
    MBOX_TYPE_AOE = 0x01,               /**< ADS over EtherCAT */
    MBOX_TYPE_EOE = 0x02,               /**< Ethernet over EtherCAT */
    MBOX_TYPE_COE = 0x03,               /**< CANopen over EtherCAT */
    MBOX_TYPE_FOE = 0x04,               /**< File over EtherCAT */
    MBOX_TYPE_SOE = 0x05,               /**< Servo over EtherCAT */
    MBOX_TYPE_VOE = 0x0F                /**< Vendor specific over EtherCAT */
} mailbox_type_t;

/**
 * @brief Mailbox header structure
 */
typedef struct __attribute__((packed)) {
    uint16_t length;                    /**< Data length (bits 0-15) */
    uint16_t address;                   /**< Slave address */
    uint8_t channel : 6;                /**< Channel (bits 0-5) */
    uint8_t priority : 2;               /**< Priority (bits 6-7) */
    uint8_t type;                       /**< Mailbox protocol type */
} mailbox_header_t;

#define MAILBOX_HEADER_SIZE 6

/**
 * @brief Mailbox states
 */
typedef enum {
    MBOX_STATE_IDLE,                    /**< Idle, ready for new request */
    MBOX_STATE_WRITE_REQUESTED,         /**< Write request pending */
    MBOX_STATE_WRITE_IN_PROGRESS,       /**< Writing to slave */
    MBOX_STATE_READ_REQUESTED,          /**< Read request pending */
    MBOX_STATE_READ_IN_PROGRESS,        /**< Reading from slave */
    MBOX_STATE_ERROR                    /**< Error state */
} mailbox_state_t;

/* ========================================================================== */
/* AL Service Primitives                                                     */
/* ========================================================================== */

/**
 * @brief AL_Control.req - Request state change
 */
typedef struct {
    uint16_t slave_address;             /**< Slave station address */
    al_state_t requested_state;         /**< Requested AL state */
    uint32_t timeout_ms;                /**< Timeout in milliseconds */
    void* user_data;                    /**< User context */
} al_control_req_t;

/**
 * @brief AL_Control.con - State change confirmation
 */
typedef struct {
    uint16_t slave_address;             /**< Slave station address */
    al_state_t current_state;           /**< Current AL state */
    al_status_code_t status_code;       /**< Status code (0 = success) */
    void* user_data;                    /**< User context */
} al_control_con_t;

/**
 * @brief AL_Control.ind - State change indication
 */
typedef struct {
    uint16_t slave_address;             /**< Slave station address */
    al_state_t old_state;               /**< Previous AL state */
    al_state_t new_state;               /**< New AL state */
    al_status_code_t status_code;       /**< Status code */
} al_control_ind_t;

/**
 * @brief MBX_Send.req - Send mailbox message
 */
typedef struct {
    uint16_t slave_address;             /**< Slave station address */
    mailbox_type_t type;                /**< Mailbox protocol type */
    uint8_t* data;                      /**< Message data */
    uint16_t length;                    /**< Data length */
    uint8_t priority;                   /**< Priority (0-3) */
    void* user_data;                    /**< User context */
} mbx_send_req_t;

/**
 * @brief MBX_Send.con - Mailbox send confirmation
 */
typedef struct {
    uint16_t slave_address;             /**< Slave station address */
    uint8_t status;                     /**< Send status */
    void* user_data;                    /**< User context */
} mbx_send_con_t;

/**
 * @brief MBX_Receive.ind - Mailbox receive indication
 */
typedef struct {
    uint16_t slave_address;             /**< Slave station address */
    mailbox_type_t type;                /**< Mailbox protocol type */
    uint8_t* data;                      /**< Received data */
    uint16_t length;                    /**< Data length */
} mbx_receive_ind_t;

/* ========================================================================== */
/* AL Configuration                                                          */
/* ========================================================================== */

/**
 * @brief Application Layer configuration
 */
typedef struct {
    uint32_t state_transition_timeout_ms;  /**< State transition timeout */
    uint32_t mailbox_timeout_ms;           /**< Mailbox operation timeout */
    uint16_t max_slaves;                   /**< Maximum number of slaves */
    bool enable_distributed_clocks;        /**< Enable DC support */
    void* user_data;                       /**< User context */
} al_config_t;

/* ========================================================================== */
/* AL Callback Function Types                                                */
/* ========================================================================== */

/**
 * @brief State change indication callback
 *
 * @param ind Pointer to state change indication
 */
typedef void (*al_state_change_cb_t)(const al_control_ind_t* ind);

/**
 * @brief Mailbox receive indication callback
 *
 * @param ind Pointer to mailbox receive indication
 */
typedef void (*al_mailbox_receive_cb_t)(const mbx_receive_ind_t* ind);

/**
 * @brief AL error callback
 *
 * @param slave_address Slave station address
 * @param error_code AL status code
 * @param context Error context string
 */
typedef void (*al_error_cb_t)(uint16_t slave_address,
                               al_status_code_t error_code,
                               const char* context);

/* ========================================================================== */
/* AL Timing Requirements                                                    */
/* ========================================================================== */

/**
 * @brief AL timing constraints
 */
#define AL_STATE_TRANSITION_TIMEOUT_MS  1000    /**< Default state transition timeout */
#define AL_MAILBOX_TIMEOUT_MS           100     /**< Default mailbox timeout */
#define AL_MAILBOX_POLL_INTERVAL_MS     1       /**< Mailbox polling interval */
#define AL_STATUS_CHECK_INTERVAL_MS     10      /**< Status check interval */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_AL_TYPES_H */
