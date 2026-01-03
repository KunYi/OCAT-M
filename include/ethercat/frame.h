/**
 * @file frame.h
 * @brief EtherCAT Frame Structure Definitions
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Based on ETG1000.4 - EtherCAT Data Link Layer Protocol
 *
 * This file contains frame and datagram structure definitions for EtherCAT.
 */

#ifndef ETHERCAT_FRAME_H
#define ETHERCAT_FRAME_H

#include "dll_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Frame EtherCAT Frame Structures
 * @{
 */

/* ========================================================================== */
/* Constants                                                                  */
/* ========================================================================== */

/** EtherCAT EtherType */
#define ETHERCAT_ETHERTYPE 0x88A4

/** EtherCAT protocol type */
#define ECAT_TYPE_DLPDU 0x1

/** Frame size limits */
#define ECAT_MAX_DATA_SIZE 1486
#define ECAT_MIN_FRAME_SIZE 64
#define ECAT_MAX_FRAME_SIZE 1518

/** Datagram size limits */
#define ECAT_DATAGRAM_HEADER_SIZE 10
#define ECAT_MAX_DATAGRAM_DATA_SIZE 1486

/** Timing constraints (nanoseconds) */
#define ECAT_FRAME_PROCESSING_TIME_NS 1000
#define ECAT_MIN_FRAME_GAP_NS 960
#define ECAT_MAX_FRAME_RATE_HZ 100000

/* ========================================================================== */
/* Ethernet Frame Structures                                                 */
/* ========================================================================== */

/**
 * @brief Ethernet II frame header
 */
typedef struct __attribute__((packed)) {
    uint8_t destination[6];      /**< Destination MAC address */
    uint8_t source[6];           /**< Source MAC address */
    uint16_t ethertype;          /**< EtherType (0x88A4 for EtherCAT) */
} eth_header_t;

/**
 * @brief EtherCAT frame header
 */
typedef struct __attribute__((packed)) {
    uint16_t length : 11;        /**< Length of EtherCAT data (bits 0-10) */
    uint16_t reserved : 1;       /**< Reserved (bit 11) */
    uint16_t type : 4;           /**< Protocol type (bits 12-15) */
} ecat_header_t;

/**
 * @brief Complete EtherCAT frame
 */
typedef struct __attribute__((packed)) {
    eth_header_t eth_header;     /**< Ethernet header */
    ecat_header_t ecat_header;   /**< EtherCAT header */
    uint8_t data[ECAT_MAX_DATA_SIZE];  /**< EtherCAT datagrams */
    uint32_t fcs;                /**< Frame Check Sequence (CRC32) */
} ecat_frame_t;

/* ========================================================================== */
/* EtherCAT Datagram Structures                                              */
/* ========================================================================== */

/**
 * @brief EtherCAT datagram header
 */
typedef struct __attribute__((packed)) {
    uint8_t cmd;                 /**< Command type */
    uint8_t idx;                 /**< Index (for identification) */
    uint32_t address;            /**< Address (interpretation depends on cmd) */
    uint16_t length : 11;        /**< Data length in bytes (bits 0-10) */
    uint16_t reserved : 3;       /**< Reserved (bits 11-13) */
    uint16_t circulating : 1;    /**< Circulating frame (bit 14) */
    uint16_t more : 1;           /**< More datagrams follow (bit 15) */
    uint16_t irq;                /**< Interrupt request */
} ecat_datagram_header_t;

/**
 * @brief Complete EtherCAT datagram
 */
typedef struct __attribute__((packed)) {
    ecat_datagram_header_t header;  /**< Datagram header */
    uint8_t data[ECAT_MAX_DATAGRAM_DATA_SIZE];  /**< Data payload */
    uint16_t wkc;                   /**< Working Counter */
} ecat_datagram_t;

/* ========================================================================== */
/* Command Types                                                              */
/* ========================================================================== */

/**
 * @brief EtherCAT command types
 */
typedef enum {
    /* Physical addressing - Auto-increment */
    ECAT_CMD_NOP = 0x00,         /**< No Operation */
    ECAT_CMD_APRD = 0x01,        /**< Auto-increment Physical Read */
    ECAT_CMD_APWR = 0x02,        /**< Auto-increment Physical Write */
    ECAT_CMD_APRW = 0x03,        /**< Auto-increment Physical Read/Write */

    /* Physical addressing - Configured */
    ECAT_CMD_FPRD = 0x04,        /**< Configured Physical Read */
    ECAT_CMD_FPWR = 0x05,        /**< Configured Physical Write */
    ECAT_CMD_FPRW = 0x06,        /**< Configured Physical Read/Write */

    /* Broadcast */
    ECAT_CMD_BRD = 0x07,         /**< Broadcast Read */
    ECAT_CMD_BWR = 0x08,         /**< Broadcast Write */
    ECAT_CMD_BRW = 0x09,         /**< Broadcast Read/Write */

    /* Logical addressing */
    ECAT_CMD_LRD = 0x0A,         /**< Logical Read */
    ECAT_CMD_LWR = 0x0B,         /**< Logical Write */
    ECAT_CMD_LRW = 0x0C,         /**< Logical Read/Write */

    /* Configured addressing with multiple slaves */
    ECAT_CMD_ARMW = 0x0D,        /**< Auto-increment Read Multiple Write */
    ECAT_CMD_FRMW = 0x0E         /**< Configured Read Multiple Write */
} ecat_cmd_t;

/* ========================================================================== */
/* Addressing Structures                                                      */
/* ========================================================================== */

/**
 * @brief Auto-increment address structure
 */
typedef struct {
    int16_t position;            /**< Slave position (negative, -1 to -65535) */
    uint16_t offset;             /**< Memory offset within slave */
} ecat_addr_autoincrement_t;

/**
 * @brief Configured address structure
 */
typedef struct {
    uint16_t station_address;    /**< Configured station address */
    uint16_t offset;             /**< Memory offset within slave */
} ecat_addr_configured_t;

/**
 * @brief Logical address type
 */
typedef uint32_t ecat_addr_logical_t;

/* ========================================================================== */
/* Working Counter                                                            */
/* ========================================================================== */

/**
 * @brief Working counter increment rules
 */
typedef enum {
    WKC_INCREMENT_READ = 1,      /**< Increment by 1 for read operations */
    WKC_INCREMENT_WRITE = 1,     /**< Increment by 1 for write operations */
    WKC_INCREMENT_READWRITE = 2  /**< Increment by 2 for read/write operations */
} wkc_increment_t;

/* ========================================================================== */
/* Address Building Functions                                                 */
/* ========================================================================== */

/**
 * @brief Build auto-increment address
 *
 * @param position Slave position (1-based, will be negated)
 * @param offset Memory offset
 * @return 32-bit address value
 */
static inline uint32_t ecat_addr_autoincrement(uint16_t position, uint16_t offset)
{
    return ((uint32_t)(-(int16_t)position) << 16) | offset;
}

/**
 * @brief Build configured address
 *
 * @param station_address Configured station address
 * @param offset Memory offset
 * @return 32-bit address value
 */
static inline uint32_t ecat_addr_configured(uint16_t station_address, uint16_t offset)
{
    return ((uint32_t)station_address << 16) | offset;
}

/**
 * @brief Build logical address
 *
 * @param address Logical address (0x00000000 to 0xFFFFFFFF)
 * @return 32-bit address value
 */
static inline uint32_t ecat_addr_logical(uint32_t address)
{
    return address;
}

/* ========================================================================== */
/* Command Type Functions                                                     */
/* ========================================================================== */

/**
 * @brief Get command type name
 *
 * @param cmd Command type
 * @return Pointer to command name string (static, do not free)
 */
const char* ecat_cmd_get_name(ecat_cmd_t cmd);

/**
 * @brief Check if command is read operation
 *
 * @param cmd Command type
 * @return true if read operation, false otherwise
 */
bool ecat_cmd_is_read(ecat_cmd_t cmd);

/**
 * @brief Check if command is write operation
 *
 * @param cmd Command type
 * @return true if write operation, false otherwise
 */
bool ecat_cmd_is_write(ecat_cmd_t cmd);

/**
 * @brief Check if command is read/write operation
 *
 * @param cmd Command type
 * @return true if read/write operation, false otherwise
 */
bool ecat_cmd_is_readwrite(ecat_cmd_t cmd);

/* ========================================================================== */
/* Working Counter Functions                                                  */
/* ========================================================================== */

/**
 * @brief Validate working counter
 *
 * Checks if the working counter matches the expected value
 *
 * @param expected Expected working counter value
 * @param actual Actual working counter value
 * @return true if valid, false otherwise
 */
bool ecat_wkc_validate(uint16_t expected, uint16_t actual);

/**
 * @brief Calculate expected working counter
 *
 * @param cmd Command type
 * @param num_slaves Number of slaves expected to process datagram
 * @return Expected working counter value
 */
uint16_t ecat_wkc_expected(ecat_cmd_t cmd, uint16_t num_slaves);

/* ========================================================================== */
/* CRC Functions                                                              */
/* ========================================================================== */

/**
 * @brief Calculate Ethernet FCS (CRC32)
 *
 * @param data Pointer to data
 * @param length Data length
 * @return CRC32 value
 */
uint32_t ecat_calculate_fcs(const uint8_t* data, uint16_t length);

/**
 * @brief Verify Ethernet FCS
 *
 * @param frame Pointer to frame
 * @param frame_length Frame length
 * @return true if FCS is valid, false otherwise
 */
bool ecat_verify_fcs(const uint8_t* frame, uint16_t frame_length);

/* ========================================================================== */
/* Timing Functions                                                           */
/* ========================================================================== */

/**
 * @brief Calculate expected round-trip time
 *
 * @param num_slaves Number of slaves in network
 * @param cable_length_m Total cable length in meters
 * @return Expected RTT in nanoseconds
 */
uint32_t ecat_calculate_rtt(uint16_t num_slaves, uint32_t cable_length_m);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_FRAME_H */
