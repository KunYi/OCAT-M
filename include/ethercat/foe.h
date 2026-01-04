/**
 * @file foe.h
 * @brief File over EtherCAT (FoE) Protocol Implementation
 *
 * This file implements the FoE protocol as defined in ETG1000.6 specification.
 * FoE provides file transfer capabilities for firmware updates and data exchange.
 *
 * @author EtherCAT Master Stack
 * @date 2026-01-04
 * @version 1.0.0
 */

#ifndef ETHERCAT_FOE_H
#define ETHERCAT_FOE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup foe File over EtherCAT (FoE)
 * @brief FoE protocol implementation for file transfer
 * @{
 */

/* ========================================================================== */
/*                             Type Definitions                               */
/* ========================================================================== */

/**
 * @brief FoE operation codes
 */
typedef enum {
    FOE_OPCODE_READ = 0x01,             /**< Read file from slave */
    FOE_OPCODE_WRITE = 0x02,            /**< Write file to slave */
    FOE_OPCODE_DATA = 0x03,             /**< Data packet */
    FOE_OPCODE_ACK = 0x04,              /**< Acknowledge */
    FOE_OPCODE_ERROR = 0x05,            /**< Error response */
    FOE_OPCODE_BUSY = 0x06              /**< Busy response */
} foe_opcode_t;

/**
 * @brief FoE error codes (as defined in ETG1000.6)
 */
typedef enum {
    FOE_ERROR_NOT_DEFINED = 0x8000,         /**< Not defined */
    FOE_ERROR_NOT_FOUND = 0x8001,           /**< File not found */
    FOE_ERROR_ACCESS_DENIED = 0x8002,       /**< Access denied */
    FOE_ERROR_DISK_FULL = 0x8003,           /**< Disk full */
    FOE_ERROR_ILLEGAL = 0x8004,             /**< Illegal operation */
    FOE_ERROR_PACKET_NUMBER = 0x8005,       /**< Packet number error */
    FOE_ERROR_ALREADY_EXISTS = 0x8006,      /**< File already exists */
    FOE_ERROR_NO_USER = 0x8007,             /**< No user */
    FOE_ERROR_BOOTSTRAP_ONLY = 0x8008,      /**< Bootstrap mode only */
    FOE_ERROR_NOT_BOOTSTRAP = 0x8009,       /**< Not in bootstrap mode */
    FOE_ERROR_NO_RIGHTS = 0x800A,           /**< No rights */
    FOE_ERROR_PROGRAM_ERROR = 0x800B        /**< Program error */
} foe_error_code_t;

/**
 * @brief FoE status codes
 */
typedef enum {
    FOE_STATUS_SUCCESS = 0x00,          /**< Operation successful */
    FOE_STATUS_ERROR = 0x01,            /**< General error */
    FOE_STATUS_TIMEOUT = 0x02,          /**< Operation timeout */
    FOE_STATUS_BUSY = 0x03,             /**< Slave is busy */
    FOE_STATUS_INVALID_PARAM = 0x04,    /**< Invalid parameter */
    FOE_STATUS_NOT_SUPPORTED = 0x05,    /**< FoE not supported by slave */
    FOE_STATUS_ABORTED = 0x06,          /**< Transfer aborted */
    FOE_STATUS_MAILBOX_ERROR = 0x07     /**< Mailbox communication error */
} foe_status_t;

/**
 * @brief FoE header structure (6 bytes)
 *
 * This structure represents the FoE protocol header as defined in ETG1000.6.
 */
typedef struct __attribute__((packed)) {
    uint8_t opcode;                     /**< FoE operation code */
    uint8_t reserved;                   /**< Reserved (must be 0) */
    union {
        uint32_t packet_number;         /**< Packet number (for DATA/ACK) */
        uint32_t error_code;            /**< Error code (for ERROR) */
        uint32_t password;              /**< Password (for READ/WRITE) */
    };
} foe_header_t;

/**
 * @brief FoE transfer progress callback
 *
 * @param bytes_transferred Number of bytes transferred so far
 * @param total_bytes Total number of bytes to transfer
 * @param user_data User-provided context pointer
 */
typedef void (*foe_progress_callback_t)(uint32_t bytes_transferred,
                                        uint32_t total_bytes,
                                        void* user_data);

/* ========================================================================== */
/*                             Constants                                      */
/* ========================================================================== */

#define FOE_HEADER_SIZE             6       /**< FoE header size in bytes */
#define FOE_MAX_DATA_SIZE           512     /**< Maximum data per packet */
#define FOE_DEFAULT_TIMEOUT_MS      5000    /**< Default transfer timeout */
#define FOE_PACKET_TIMEOUT_MS       1000    /**< Per-packet timeout */
#define FOE_BUSY_RETRY_MS           100     /**< Retry delay when busy */
#define FOE_MAX_BUSY_RETRIES        50      /**< Maximum busy retries */
#define FOE_MAX_FILENAME_LENGTH     256     /**< Maximum filename length */

/* ========================================================================== */
/*                          Public API Functions                              */
/* ========================================================================== */

/**
 * @brief Read file from slave
 *
 * This function reads a file from the slave device using the FoE protocol.
 * The slave must be in Pre-Operational or Bootstrap state.
 *
 * @param slave_address Slave station address (0x1000+)
 * @param filename Filename to read (null-terminated string)
 * @param data Buffer to receive file data
 * @param size Pointer to buffer size (in: max size, out: actual size)
 * @param timeout_ms Timeout in milliseconds (0 = use default)
 * @param progress_callback Optional progress callback (can be NULL)
 * @param user_data User data passed to progress callback
 * @return FOE_STATUS_SUCCESS on success, error code otherwise
 *
 * @note The buffer must be large enough to hold the entire file.
 * @note If the file is larger than the buffer, FOE_STATUS_INVALID_PARAM is returned.
 *
 * @example
 * @code
 * uint8_t buffer[65536];
 * uint32_t size = sizeof(buffer);
 * foe_status_t status = foe_read(0x1001, "config.bin", buffer, &size, 5000, NULL, NULL);
 * if (status == FOE_STATUS_SUCCESS) {
 *     printf("Read %u bytes\n", size);
 * }
 * @endcode
 */
foe_status_t foe_read(uint16_t slave_address,
                      const char* filename,
                      uint8_t* data,
                      uint32_t* size,
                      uint32_t timeout_ms,
                      foe_progress_callback_t progress_callback,
                      void* user_data);

/**
 * @brief Write file to slave
 *
 * This function writes a file to the slave device using the FoE protocol.
 * The slave must be in Pre-Operational or Bootstrap state.
 *
 * @param slave_address Slave station address (0x1000+)
 * @param filename Filename to write (null-terminated string)
 * @param data File data to write
 * @param size File size in bytes
 * @param timeout_ms Timeout in milliseconds (0 = use default)
 * @param progress_callback Optional progress callback (can be NULL)
 * @param user_data User data passed to progress callback
 * @return FOE_STATUS_SUCCESS on success, error code otherwise
 *
 * @note The file is transferred in chunks of up to FOE_MAX_DATA_SIZE bytes.
 * @note The slave may respond with BUSY, in which case the transfer is retried.
 *
 * @example
 * @code
 * uint8_t firmware[] = { ... };
 * foe_status_t status = foe_write(0x1001, "firmware.bin", firmware,
 *                                 sizeof(firmware), 10000, progress_cb, NULL);
 * @endcode
 */
foe_status_t foe_write(uint16_t slave_address,
                       const char* filename,
                       const uint8_t* data,
                       uint32_t size,
                       uint32_t timeout_ms,
                       foe_progress_callback_t progress_callback,
                       void* user_data);

/**
 * @brief Perform firmware update on slave
 *
 * This is a convenience function that performs a complete firmware update:
 * 1. Transitions slave to Bootstrap state
 * 2. Writes firmware file using FoE
 * 3. Transitions slave back to Init state
 * 4. Waits for slave to restart
 *
 * @param slave_address Slave station address (0x1000+)
 * @param firmware_data Firmware binary data
 * @param firmware_size Firmware size in bytes
 * @param progress_callback Optional progress callback (can be NULL)
 * @param user_data User data passed to progress callback
 * @param timeout_ms Total timeout in milliseconds (0 = use default)
 * @return FOE_STATUS_SUCCESS on success, error code otherwise
 *
 * @note This function requires the slave to support Bootstrap mode.
 * @note The slave will restart after the firmware update.
 * @note The default firmware filename is "ECAT.BIN" (can be customized).
 *
 * @example
 * @code
 * uint8_t firmware[131072];
 * // Load firmware into buffer...
 * foe_status_t status = foe_firmware_update(0x1001, firmware, sizeof(firmware),
 *                                           progress_cb, NULL, 30000);
 * @endcode
 */
foe_status_t foe_firmware_update(uint16_t slave_address,
                                  const uint8_t* firmware_data,
                                  uint32_t firmware_size,
                                  foe_progress_callback_t progress_callback,
                                  void* user_data,
                                  uint32_t timeout_ms);

/**
 * @brief Perform firmware update with custom filename
 *
 * Same as foe_firmware_update() but allows specifying a custom filename.
 *
 * @param slave_address Slave station address (0x1000+)
 * @param filename Firmware filename (null-terminated string)
 * @param firmware_data Firmware binary data
 * @param firmware_size Firmware size in bytes
 * @param progress_callback Optional progress callback (can be NULL)
 * @param user_data User data passed to progress callback
 * @param timeout_ms Total timeout in milliseconds (0 = use default)
 * @return FOE_STATUS_SUCCESS on success, error code otherwise
 */
foe_status_t foe_firmware_update_ex(uint16_t slave_address,
                                     const char* filename,
                                     const uint8_t* firmware_data,
                                     uint32_t firmware_size,
                                     foe_progress_callback_t progress_callback,
                                     void* user_data,
                                     uint32_t timeout_ms);

/* ========================================================================== */
/*                          Utility Functions                                 */
/* ========================================================================== */

/**
 * @brief Get FoE error code description
 *
 * @param error_code FoE error code
 * @return Pointer to error description string (static storage)
 */
const char* foe_get_error_string(foe_error_code_t error_code);

/**
 * @brief Get FoE status description
 *
 * @param status FoE status code
 * @return Pointer to status description string (static storage)
 */
const char* foe_get_status_string(foe_status_t status);

/**
 * @brief Get FoE opcode name
 *
 * @param opcode FoE operation code
 * @return Pointer to opcode name string (static storage)
 */
const char* foe_get_opcode_name(foe_opcode_t opcode);

/**
 * @brief Check if slave supports FoE protocol
 *
 * This function checks the slave's mailbox protocol support to determine
 * if FoE is available.
 *
 * @param slave_address Slave station address (0x1000+)
 * @param supported Pointer to receive support flag
 * @return FOE_STATUS_SUCCESS on success, error code otherwise
 */
foe_status_t foe_check_support(uint16_t slave_address, bool* supported);

/** @} */ /* end of foe group */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_FOE_H */
