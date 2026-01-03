/**
 * @file frame_builder.h
 * @brief EtherCAT Frame Builder Interface
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Based on ETG1000.4 - EtherCAT Data Link Layer Protocol
 *
 * This file contains the frame builder interface for constructing EtherCAT frames.
 */

#ifndef ETHERCAT_FRAME_BUILDER_H
#define ETHERCAT_FRAME_BUILDER_H

#include "frame.h"
#include "dll_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup FrameBuilder EtherCAT Frame Builder
 * @{
 */

/* ========================================================================== */
/* Frame Builder Structure                                                    */
/* ========================================================================== */

/**
 * @brief Frame builder context
 */
typedef struct {
    uint8_t* buffer;             /**< Frame buffer */
    uint16_t buffer_size;        /**< Buffer size */
    uint16_t current_offset;     /**< Current write offset */
    uint8_t datagram_count;      /**< Number of datagrams added */
    uint8_t src_mac[6];          /**< Source MAC address */
    uint8_t dst_mac[6];          /**< Destination MAC address */
} ecat_frame_builder_t;

/* ========================================================================== */
/* Frame Builder Functions                                                    */
/* ========================================================================== */

/**
 * @brief Initialize frame builder
 *
 * Initializes the frame builder and writes Ethernet and EtherCAT headers.
 *
 * @param builder Pointer to frame builder
 * @param buffer Frame buffer
 * @param buffer_size Buffer size
 * @param src_mac Source MAC address
 * @param dst_mac Destination MAC address
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t ecat_frame_builder_init(ecat_frame_builder_t* builder,
                                     uint8_t* buffer,
                                     uint16_t buffer_size,
                                     const uint8_t src_mac[6],
                                     const uint8_t dst_mac[6]);

/**
 * @brief Add datagram to frame
 *
 * Adds a datagram to the frame being built.
 *
 * @param builder Pointer to frame builder
 * @param cmd Command type
 * @param idx Datagram index
 * @param address Address (interpretation depends on cmd)
 * @param data Data to write (NULL for read operations)
 * @param length Data length
 * @param more More datagrams will follow
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t ecat_frame_builder_add_datagram(ecat_frame_builder_t* builder,
                                              ecat_cmd_t cmd,
                                              uint8_t idx,
                                              uint32_t address,
                                              const uint8_t* data,
                                              uint16_t length,
                                              bool more);

/**
 * @brief Finalize frame
 *
 * Finalizes the frame by updating the EtherCAT header length field
 * and calculating the FCS (CRC32).
 *
 * @param builder Pointer to frame builder
 * @param frame_length Pointer to receive final frame length
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t ecat_frame_builder_finalize(ecat_frame_builder_t* builder,
                                         uint16_t* frame_length);

/**
 * @brief Reset frame builder
 *
 * Resets the frame builder to initial state for building a new frame.
 *
 * @param builder Pointer to frame builder
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t ecat_frame_builder_reset(ecat_frame_builder_t* builder);

/**
 * @brief Get current frame size
 *
 * Returns the current size of the frame being built.
 *
 * @param builder Pointer to frame builder
 * @return Current frame size in bytes
 */
uint16_t ecat_frame_builder_get_size(const ecat_frame_builder_t* builder);

/**
 * @brief Get number of datagrams
 *
 * Returns the number of datagrams added to the frame.
 *
 * @param builder Pointer to frame builder
 * @return Number of datagrams
 */
uint8_t ecat_frame_builder_get_datagram_count(const ecat_frame_builder_t* builder);

/**
 * @brief Check if more datagrams can be added
 *
 * Checks if there is enough space to add another datagram.
 *
 * @param builder Pointer to frame builder
 * @param datagram_size Size of datagram to add
 * @return true if space available, false otherwise
 */
bool ecat_frame_builder_can_add(const ecat_frame_builder_t* builder,
                                 uint16_t datagram_size);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_FRAME_BUILDER_H */
