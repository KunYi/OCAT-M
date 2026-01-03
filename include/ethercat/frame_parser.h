/**
 * @file frame_parser.h
 * @brief EtherCAT Frame Parser Interface
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Based on ETG1000.4 - EtherCAT Data Link Layer Protocol
 *
 * This file contains the frame parser interface for parsing EtherCAT frames.
 */

#ifndef ETHERCAT_FRAME_PARSER_H
#define ETHERCAT_FRAME_PARSER_H

#include "frame.h"
#include "dll_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup FrameParser EtherCAT Frame Parser
 * @{
 */

/* ========================================================================== */
/* Frame Parser Structure                                                     */
/* ========================================================================== */

/**
 * @brief Frame parser context
 */
typedef struct {
    const uint8_t* buffer;       /**< Frame buffer */
    uint16_t buffer_size;        /**< Buffer size */
    uint16_t current_offset;     /**< Current read offset */
    uint8_t datagram_count;      /**< Number of datagrams parsed */
    uint16_t ecat_data_length;   /**< EtherCAT data length from header */
} ecat_frame_parser_t;

/**
 * @brief Parsed datagram information
 */
typedef struct {
    ecat_cmd_t cmd;              /**< Command type */
    uint8_t idx;                 /**< Datagram index */
    uint32_t address;            /**< Address */
    const uint8_t* data;         /**< Pointer to data in frame buffer */
    uint16_t length;             /**< Data length */
    uint16_t wkc;                /**< Working counter */
    bool more;                   /**< More datagrams follow */
    bool circulating;            /**< Circulating frame flag */
    uint16_t irq;                /**< Interrupt request */
} ecat_parsed_datagram_t;

/* ========================================================================== */
/* Frame Parser Functions                                                     */
/* ========================================================================== */

/**
 * @brief Initialize frame parser
 *
 * Initializes the frame parser with a received frame buffer.
 *
 * @param parser Pointer to frame parser
 * @param buffer Frame buffer
 * @param buffer_size Buffer size
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t ecat_frame_parser_init(ecat_frame_parser_t* parser,
                                    const uint8_t* buffer,
                                    uint16_t buffer_size);

/**
 * @brief Validate frame headers
 *
 * Validates Ethernet and EtherCAT headers.
 *
 * @param parser Pointer to frame parser
 * @return DL_STATUS_SUCCESS if valid, error code otherwise
 */
dl_status_t ecat_frame_parser_validate(ecat_frame_parser_t* parser);

/**
 * @brief Get next datagram from frame
 *
 * Parses and returns the next datagram from the frame.
 *
 * @param parser Pointer to frame parser
 * @param datagram Pointer to receive parsed datagram information
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t ecat_frame_parser_next_datagram(ecat_frame_parser_t* parser,
                                              ecat_parsed_datagram_t* datagram);

/**
 * @brief Check if more datagrams are available
 *
 * Checks if there are more datagrams to parse in the frame.
 *
 * @param parser Pointer to frame parser
 * @return true if more datagrams available, false otherwise
 */
bool ecat_frame_parser_has_more(const ecat_frame_parser_t* parser);

/**
 * @brief Reset frame parser
 *
 * Resets the parser to the beginning of the frame.
 *
 * @param parser Pointer to frame parser
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t ecat_frame_parser_reset(ecat_frame_parser_t* parser);

/**
 * @brief Get number of datagrams parsed
 *
 * Returns the number of datagrams parsed so far.
 *
 * @param parser Pointer to frame parser
 * @return Number of datagrams parsed
 */
uint8_t ecat_frame_parser_get_datagram_count(const ecat_frame_parser_t* parser);

/**
 * @brief Get source MAC address
 *
 * Extracts the source MAC address from the frame.
 *
 * @param parser Pointer to frame parser
 * @param mac_address Buffer to receive MAC address (6 bytes)
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t ecat_frame_parser_get_src_mac(const ecat_frame_parser_t* parser,
                                           uint8_t mac_address[6]);

/**
 * @brief Get destination MAC address
 *
 * Extracts the destination MAC address from the frame.
 *
 * @param parser Pointer to frame parser
 * @param mac_address Buffer to receive MAC address (6 bytes)
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t ecat_frame_parser_get_dst_mac(const ecat_frame_parser_t* parser,
                                           uint8_t mac_address[6]);

/**
 * @brief Verify frame FCS
 *
 * Verifies the Frame Check Sequence (CRC32) of the frame.
 *
 * @param parser Pointer to frame parser
 * @return true if FCS is valid, false otherwise
 */
bool ecat_frame_parser_verify_fcs(const ecat_frame_parser_t* parser);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_FRAME_PARSER_H */
