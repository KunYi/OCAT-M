/**
 * @file coe.h
 * @brief CANopen over EtherCAT (CoE) - Public API
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Based on ETG1000.6 - EtherCAT Application Layer Protocols
 *
 * This file contains the public API for CoE including SDO access,
 * object dictionary operations, and PDO mapping.
 */

#ifndef ETHERCAT_COE_H
#define ETHERCAT_COE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CoE_API CANopen over EtherCAT API
 * @{
 */

/* ========================================================================== */
/* CoE Status Codes                                                          */
/* ========================================================================== */

/**
 * @brief CoE status codes
 */
typedef enum {
    COE_STATUS_SUCCESS = 0x00,          /**< Operation successful */
    COE_STATUS_ERROR = 0x01,            /**< General error */
    COE_STATUS_TIMEOUT = 0x02,          /**< Operation timeout */
    COE_STATUS_ABORT = 0x03,            /**< SDO abort */
    COE_STATUS_INVALID_PARAM = 0x04,    /**< Invalid parameter */
    COE_STATUS_NOT_SUPPORTED = 0x05,    /**< Operation not supported */
    COE_STATUS_BUSY = 0x06              /**< Resource busy */
} coe_status_t;

/* ========================================================================== */
/* CoE Service Types                                                         */
/* ========================================================================== */

/**
 * @brief CoE service types
 */
typedef enum {
    COE_SERVICE_SDO_REQUEST = 0x02,     /**< SDO request */
    COE_SERVICE_SDO_RESPONSE = 0x03,    /**< SDO response */
    COE_SERVICE_TXPDO = 0x04,           /**< TxPDO (slave to master) */
    COE_SERVICE_RXPDO = 0x05,           /**< RxPDO (master to slave) */
    COE_SERVICE_TXPDO_REMOTE = 0x06,    /**< TxPDO remote request */
    COE_SERVICE_RXPDO_REMOTE = 0x07,    /**< RxPDO remote request */
    COE_SERVICE_SDO_INFO = 0x08         /**< SDO information */
} coe_service_t;

/* ========================================================================== */
/* CoE Header                                                                */
/* ========================================================================== */

/**
 * @brief CoE header structure
 */
typedef struct __attribute__((packed)) {
    uint16_t number : 9;                /**< SDO number (bits 0-8) */
    uint16_t reserved : 3;              /**< Reserved (bits 9-11) */
    uint16_t service : 4;               /**< CoE service type (bits 12-15) */
} coe_header_t;

#define COE_HEADER_SIZE 2

/* ========================================================================== */
/* SDO Command Specifiers                                                    */
/* ========================================================================== */

/**
 * @brief SDO command specifiers (Client Command Specifier - CCS)
 */
typedef enum {
    SDO_CCS_DOWNLOAD_SEGMENT = 0x00,    /**< Download segment */
    SDO_CCS_DOWNLOAD_INIT = 0x01,       /**< Initiate download */
    SDO_CCS_UPLOAD_INIT = 0x02,         /**< Initiate upload */
    SDO_CCS_UPLOAD_SEGMENT = 0x03,      /**< Upload segment */
    SDO_CCS_ABORT = 0x04                /**< Abort transfer */
} sdo_ccs_t;

/**
 * @brief SDO command specifiers (Server Command Specifier - SCS)
 */
typedef enum {
    SDO_SCS_UPLOAD_SEGMENT = 0x00,      /**< Upload segment response */
    SDO_SCS_DOWNLOAD_SEGMENT = 0x01,    /**< Download segment response */
    SDO_SCS_UPLOAD_INIT = 0x02,         /**< Initiate upload response */
    SDO_SCS_DOWNLOAD_INIT = 0x03,       /**< Initiate download response */
    SDO_SCS_ABORT = 0x04                /**< Abort transfer */
} sdo_scs_t;

/* ========================================================================== */
/* SDO Structures                                                            */
/* ========================================================================== */

/**
 * @brief SDO command byte structure
 */
typedef struct {
    uint8_t ccs : 3;                    /**< Client/Server command specifier (bits 0-2) */
    uint8_t reserved : 1;               /**< Reserved (bit 3) */
    uint8_t n : 2;                      /**< Number of bytes (bits 4-5) */
    uint8_t e : 1;                      /**< Expedited transfer (bit 6) */
    uint8_t s : 1;                      /**< Size indicator (bit 7) */
} sdo_command_byte_t;

/**
 * @brief SDO Download request structure (Initiate)
 */
typedef struct __attribute__((packed)) {
    uint8_t command;                    /**< Command specifier */
    uint16_t index;                     /**< Object Dictionary index */
    uint8_t subindex;                   /**< Object Dictionary subindex */
    uint32_t data;                      /**< Data (expedited) or size (normal) */
} sdo_download_init_req_t;

/**
 * @brief SDO Download response structure (Initiate)
 */
typedef struct __attribute__((packed)) {
    uint8_t command;                    /**< Command specifier */
    uint16_t index;                     /**< Object Dictionary index */
    uint8_t subindex;                   /**< Object Dictionary subindex */
} sdo_download_init_res_t;

/**
 * @brief SDO Upload request structure (Initiate)
 */
typedef struct __attribute__((packed)) {
    uint8_t command;                    /**< Command specifier */
    uint16_t index;                     /**< Object Dictionary index */
    uint8_t subindex;                   /**< Object Dictionary subindex */
    uint32_t reserved;                  /**< Reserved */
} sdo_upload_init_req_t;

/**
 * @brief SDO Upload response structure (Initiate)
 */
typedef struct __attribute__((packed)) {
    uint8_t command;                    /**< Command specifier */
    uint16_t index;                     /**< Object Dictionary index */
    uint8_t subindex;                   /**< Object Dictionary subindex */
    uint32_t data;                      /**< Data (expedited) or size (normal) */
} sdo_upload_init_res_t;

/**
 * @brief SDO Abort structure
 */
typedef struct __attribute__((packed)) {
    uint8_t command;                    /**< Command specifier (0x80) */
    uint16_t index;                     /**< Object Dictionary index */
    uint8_t subindex;                   /**< Object Dictionary subindex */
    uint32_t abort_code;                /**< Abort code */
} sdo_abort_t;

/**
 * @brief SDO Download segment request structure
 */
typedef struct __attribute__((packed)) {
    uint8_t command;                    /**< Command specifier */
    uint8_t data[7];                    /**< Segment data (up to 7 bytes) */
} sdo_download_segment_req_t;

/**
 * @brief SDO Download segment response structure
 */
typedef struct __attribute__((packed)) {
    uint8_t command;                    /**< Command specifier */
} sdo_download_segment_res_t;

/**
 * @brief SDO Upload segment request structure
 */
typedef struct __attribute__((packed)) {
    uint8_t command;                    /**< Command specifier */
} sdo_upload_segment_req_t;

/**
 * @brief SDO Upload segment response structure
 */
typedef struct __attribute__((packed)) {
    uint8_t command;                    /**< Command specifier */
    uint8_t data[7];                    /**< Segment data (up to 7 bytes) */
} sdo_upload_segment_res_t;

/**
 * @brief SDO segment command byte structure
 */
typedef struct {
    uint8_t toggle : 1;                 /**< Toggle bit (bit 0) */
    uint8_t n : 3;                      /**< Number of bytes (bits 1-3) */
    uint8_t c : 1;                      /**< Last segment (bit 4) */
    uint8_t ccs : 3;                    /**< Command specifier (bits 5-7) */
} sdo_segment_command_byte_t;

/* ========================================================================== */
/* SDO Abort Codes                                                           */
/* ========================================================================== */

/**
 * @brief SDO abort codes
 */
typedef enum {
    SDO_ABORT_TOGGLE_BIT = 0x05030000,
    SDO_ABORT_TIMEOUT = 0x05040000,
    SDO_ABORT_INVALID_COMMAND = 0x05040001,
    SDO_ABORT_INVALID_BLOCK_SIZE = 0x05040002,
    SDO_ABORT_INVALID_SEQUENCE = 0x05040003,
    SDO_ABORT_CRC_ERROR = 0x05040004,
    SDO_ABORT_OUT_OF_MEMORY = 0x05040005,
    SDO_ABORT_UNSUPPORTED_ACCESS = 0x06010000,
    SDO_ABORT_WRITE_ONLY = 0x06010001,
    SDO_ABORT_READ_ONLY = 0x06010002,
    SDO_ABORT_OBJECT_NOT_EXIST = 0x06020000,
    SDO_ABORT_OBJECT_CANNOT_MAP = 0x06040041,
    SDO_ABORT_PDO_LENGTH_EXCEEDED = 0x06040042,
    SDO_ABORT_PARAMETER_INCOMPATIBLE = 0x06040043,
    SDO_ABORT_INTERNAL_INCOMPATIBILITY = 0x06040047,
    SDO_ABORT_HARDWARE_ERROR = 0x06060000,
    SDO_ABORT_DATA_TYPE_MISMATCH = 0x06070010,
    SDO_ABORT_DATA_TYPE_LENGTH_HIGH = 0x06070012,
    SDO_ABORT_DATA_TYPE_LENGTH_LOW = 0x06070013,
    SDO_ABORT_SUBINDEX_NOT_EXIST = 0x06090011,
    SDO_ABORT_VALUE_RANGE_EXCEEDED = 0x06090030,
    SDO_ABORT_VALUE_TOO_HIGH = 0x06090031,
    SDO_ABORT_VALUE_TOO_LOW = 0x06090032,
    SDO_ABORT_MAX_LESS_MIN = 0x06090036,
    SDO_ABORT_GENERAL_ERROR = 0x08000000,
    SDO_ABORT_DATA_CANNOT_TRANSFER = 0x08000020,
    SDO_ABORT_DATA_CANNOT_TRANSFER_LOCAL = 0x08000021,
    SDO_ABORT_DATA_CANNOT_TRANSFER_STATE = 0x08000022,
    SDO_ABORT_NO_OBJECT_DICTIONARY = 0x08000023,
    SDO_ABORT_NO_DATA_AVAILABLE = 0x08000024
} sdo_abort_code_t;

/* ========================================================================== */
/* Object Dictionary Standard Indices                                        */
/* ========================================================================== */

/**
 * @brief Standard CANopen object dictionary indices
 */
#define OD_DEVICE_TYPE              0x1000  /**< Device type */
#define OD_ERROR_REGISTER           0x1001  /**< Error register */
#define OD_MANUFACTURER_STATUS      0x1002  /**< Manufacturer status */
#define OD_IDENTITY_OBJECT          0x1018  /**< Identity object */
#define OD_SYNC_MANAGER_TYPE        0x1C00  /**< Sync manager type */
#define OD_RXPDO_MAPPING            0x1600  /**< RxPDO mapping (outputs) */
#define OD_TXPDO_MAPPING            0x1A00  /**< TxPDO mapping (inputs) */
#define OD_RXPDO_ASSIGN             0x1C12  /**< RxPDO assignment */
#define OD_TXPDO_ASSIGN             0x1C13  /**< TxPDO assignment */

/**
 * @brief Identity object subindices
 */
#define OD_IDENTITY_VENDOR_ID       0x01    /**< Vendor ID */
#define OD_IDENTITY_PRODUCT_CODE    0x02    /**< Product code */
#define OD_IDENTITY_REVISION        0x03    /**< Revision number */
#define OD_IDENTITY_SERIAL_NUMBER   0x04    /**< Serial number */

/* ========================================================================== */
/* CoE API Functions                                                         */
/* ========================================================================== */

/**
 * @brief Initialize CoE module
 *
 * @return COE_STATUS_SUCCESS on success, error code otherwise
 */
coe_status_t coe_init(void);

/**
 * @brief Shutdown CoE module
 *
 * @return COE_STATUS_SUCCESS on success, error code otherwise
 */
coe_status_t coe_shutdown(void);

/* ========================================================================== */
/* SDO Download (Write to Object Dictionary)                                */
/* ========================================================================== */

/**
 * @brief SDO Download (write to object dictionary)
 *
 * @param slave_address Slave station address
 * @param index Object dictionary index
 * @param subindex Object dictionary subindex
 * @param data Data to write
 * @param size Data size in bytes
 * @param timeout_ms Timeout in milliseconds
 * @return COE_STATUS_SUCCESS on success, error code otherwise
 */
coe_status_t coe_sdo_download(uint16_t slave_address,
                               uint16_t index,
                               uint8_t subindex,
                               const uint8_t* data,
                               uint32_t size,
                               uint32_t timeout_ms);

/**
 * @brief SDO Download expedited (1-4 bytes)
 *
 * @param slave_address Slave station address
 * @param index Object dictionary index
 * @param subindex Object dictionary subindex
 * @param value Value to write (up to 4 bytes)
 * @param size Value size (1-4 bytes)
 * @param timeout_ms Timeout in milliseconds
 * @return COE_STATUS_SUCCESS on success, error code otherwise
 */
coe_status_t coe_sdo_download_expedited(uint16_t slave_address,
                                         uint16_t index,
                                         uint8_t subindex,
                                         uint32_t value,
                                         uint8_t size,
                                         uint32_t timeout_ms);

/* ========================================================================== */
/* SDO Upload (Read from Object Dictionary)                                 */
/* ========================================================================== */

/**
 * @brief SDO Upload (read from object dictionary)
 *
 * @param slave_address Slave station address
 * @param index Object dictionary index
 * @param subindex Object dictionary subindex
 * @param data Buffer for read data
 * @param size Pointer to buffer size (in: max, out: actual)
 * @param timeout_ms Timeout in milliseconds
 * @return COE_STATUS_SUCCESS on success, error code otherwise
 */
coe_status_t coe_sdo_upload(uint16_t slave_address,
                             uint16_t index,
                             uint8_t subindex,
                             uint8_t* data,
                             uint32_t* size,
                             uint32_t timeout_ms);

/**
 * @brief SDO Upload expedited (1-4 bytes)
 *
 * @param slave_address Slave station address
 * @param index Object dictionary index
 * @param subindex Object dictionary subindex
 * @param value Pointer to receive value
 * @param size Pointer to receive size
 * @param timeout_ms Timeout in milliseconds
 * @return COE_STATUS_SUCCESS on success, error code otherwise
 */
coe_status_t coe_sdo_upload_expedited(uint16_t slave_address,
                                       uint16_t index,
                                       uint8_t subindex,
                                       uint32_t* value,
                                       uint8_t* size,
                                       uint32_t timeout_ms);

/* ========================================================================== */
/* Utility Functions                                                         */
/* ========================================================================== */

/**
 * @brief Get SDO abort code description string
 *
 * @param abort_code SDO abort code
 * @return Pointer to description string
 */
const char* coe_get_abort_code_string(sdo_abort_code_t abort_code);

/**
 * @brief Get CoE version string
 *
 * @return Pointer to version string
 */
const char* coe_get_version(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_COE_H */
