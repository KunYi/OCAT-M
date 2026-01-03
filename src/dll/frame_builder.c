/**
 * @file frame_builder.c
 * @brief EtherCAT Frame Builder Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "ethercat/frame_builder.h"
#include "ethercat/endian.h"
#include "ethercat/dll_errors.h"
#include <string.h>

/* ========================================================================== */
/* Frame Builder Functions                                                    */
/* ========================================================================== */

dl_status_t ecat_frame_builder_init(ecat_frame_builder_t* builder,
                                     uint8_t* buffer,
                                     uint16_t buffer_size,
                                     const uint8_t src_mac[6],
                                     const uint8_t dst_mac[6])
{
    if (builder == NULL || buffer == NULL || src_mac == NULL || dst_mac == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "ecat_frame_builder_init: invalid parameter");
        return DL_STATUS_INVALID_PARAM;
    }

    if (buffer_size < ECAT_MIN_FRAME_SIZE) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "ecat_frame_builder_init: buffer too small");
        return DL_STATUS_INVALID_PARAM;
    }

    /* Initialize builder */
    builder->buffer = buffer;
    builder->buffer_size = buffer_size;
    builder->current_offset = 0;
    builder->datagram_count = 0;
    memcpy(builder->src_mac, src_mac, 6);
    memcpy(builder->dst_mac, dst_mac, 6);

    /* Write Ethernet header */
    memcpy(&buffer[0], dst_mac, 6);
    memcpy(&buffer[6], src_mac, 6);
    ecat_write_u16_le(&buffer[12], ETHERCAT_ETHERTYPE);

    /* Write EtherCAT header (length will be updated in finalize) */
    ecat_write_u16_le(&buffer[14], 0x1000);  /* Type = 0x1, Length = 0 (will be updated) */

    /* Set offset to start of datagram area */
    builder->current_offset = 16;

    return DL_STATUS_SUCCESS;
}

dl_status_t ecat_frame_builder_add_datagram(ecat_frame_builder_t* builder,
                                              ecat_cmd_t cmd,
                                              uint8_t idx,
                                              uint32_t address,
                                              const uint8_t* data,
                                              uint16_t length,
                                              bool more)
{
    if (builder == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "ecat_frame_builder_add_datagram: invalid parameter");
        return DL_STATUS_INVALID_PARAM;
    }

    /* Calculate datagram size */
    uint16_t datagram_size = ECAT_DATAGRAM_HEADER_SIZE + length + 2;  /* +2 for WKC */

    /* Check if there's enough space */
    if (builder->current_offset + datagram_size + 4 > builder->buffer_size) {  /* +4 for FCS */
        dl_set_error(DL_ERROR_INVALID_PARAM, "ecat_frame_builder_add_datagram: buffer full");
        return DL_STATUS_ERROR;
    }

    uint8_t* dgram = &builder->buffer[builder->current_offset];

    /* Write datagram header */
    dgram[0] = cmd;
    dgram[1] = idx;
    ecat_write_u32_le(&dgram[2], address);

    /* Write length field with flags */
    uint16_t length_field = length & 0x7FF;  /* 11 bits for length */
    if (more) {
        length_field |= 0x8000;  /* Set 'more' bit */
    }
    ecat_write_u16_le(&dgram[6], length_field);

    /* Write IRQ field (0 for now) */
    ecat_write_u16_le(&dgram[8], 0);

    /* Write data */
    if (data != NULL && length > 0) {
        memcpy(&dgram[10], data, length);
    } else {
        /* For read operations, fill with zeros */
        memset(&dgram[10], 0, length);
    }

    /* Write WKC (initialized to 0) */
    ecat_write_u16_le(&dgram[10 + length], 0);

    /* Update offset and count */
    builder->current_offset += datagram_size;
    builder->datagram_count++;

    return DL_STATUS_SUCCESS;
}

dl_status_t ecat_frame_builder_finalize(ecat_frame_builder_t* builder,
                                         uint16_t* frame_length)
{
    if (builder == NULL || frame_length == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "ecat_frame_builder_finalize: invalid parameter");
        return DL_STATUS_INVALID_PARAM;
    }

    if (builder->datagram_count == 0) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "ecat_frame_builder_finalize: no datagrams");
        return DL_STATUS_ERROR;
    }

    /* Calculate EtherCAT data length (excluding Ethernet and EtherCAT headers) */
    uint16_t ecat_data_length = builder->current_offset - 16;

    /* Update EtherCAT header length field */
    uint16_t ecat_header = (ECAT_TYPE_DLPDU << 12) | (ecat_data_length & 0x7FF);
    ecat_write_u16_le(&builder->buffer[14], ecat_header);

    /* Calculate and append FCS */
    uint32_t fcs = ecat_calculate_fcs(builder->buffer, builder->current_offset);
    ecat_write_u32_le(&builder->buffer[builder->current_offset], fcs);

    /* Update frame length */
    *frame_length = builder->current_offset + 4;

    /* Pad to minimum frame size if necessary */
    if (*frame_length < ECAT_MIN_FRAME_SIZE) {
        memset(&builder->buffer[*frame_length], 0, ECAT_MIN_FRAME_SIZE - *frame_length);
        *frame_length = ECAT_MIN_FRAME_SIZE;
    }

    return DL_STATUS_SUCCESS;
}

dl_status_t ecat_frame_builder_reset(ecat_frame_builder_t* builder)
{
    if (builder == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "ecat_frame_builder_reset: invalid parameter");
        return DL_STATUS_INVALID_PARAM;
    }

    /* Re-initialize with same MAC addresses */
    return ecat_frame_builder_init(builder, builder->buffer, builder->buffer_size,
                                    builder->src_mac, builder->dst_mac);
}

uint16_t ecat_frame_builder_get_size(const ecat_frame_builder_t* builder)
{
    if (builder == NULL) {
        return 0;
    }
    return builder->current_offset;
}

uint8_t ecat_frame_builder_get_datagram_count(const ecat_frame_builder_t* builder)
{
    if (builder == NULL) {
        return 0;
    }
    return builder->datagram_count;
}

bool ecat_frame_builder_can_add(const ecat_frame_builder_t* builder,
                                 uint16_t datagram_size)
{
    if (builder == NULL) {
        return false;
    }

    uint16_t required_size = builder->current_offset + datagram_size + 4;  /* +4 for FCS */
    return (required_size <= builder->buffer_size);
}
