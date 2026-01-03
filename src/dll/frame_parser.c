/**
 * @file frame_parser.c
 * @brief EtherCAT Frame Parser Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "ethercat/frame_parser.h"
#include "ethercat/endian.h"
#include "ethercat/dll_errors.h"
#include <string.h>

/* ========================================================================== */
/* Frame Parser Functions                                                     */
/* ========================================================================== */

dl_status_t ecat_frame_parser_init(ecat_frame_parser_t* parser,
                                    const uint8_t* buffer,
                                    uint16_t buffer_size)
{
    if (parser == NULL || buffer == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "ecat_frame_parser_init: invalid parameter");
        return DL_STATUS_INVALID_PARAM;
    }

    if (buffer_size < ECAT_MIN_FRAME_SIZE) {
        dl_set_error(DL_ERROR_INVALID_FRAME, "ecat_frame_parser_init: frame too small");
        return DL_STATUS_INVALID_PARAM;
    }

    /* Initialize parser */
    parser->buffer = buffer;
    parser->buffer_size = buffer_size;
    parser->current_offset = 16;  /* Start after Ethernet and EtherCAT headers */
    parser->datagram_count = 0;
    parser->ecat_data_length = 0;

    return DL_STATUS_SUCCESS;
}

dl_status_t ecat_frame_parser_validate(ecat_frame_parser_t* parser)
{
    if (parser == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "ecat_frame_parser_validate: invalid parameter");
        return DL_STATUS_INVALID_PARAM;
    }

    const uint8_t* buffer = parser->buffer;

    /* Check EtherType */
    uint16_t ethertype = ecat_read_u16_le(&buffer[12]);
    if (ethertype != ETHERCAT_ETHERTYPE) {
        dl_set_error(DL_ERROR_INVALID_FRAME, "ecat_frame_parser_validate: invalid EtherType");
        return DL_STATUS_ERROR;
    }

    /* Parse EtherCAT header */
    uint16_t ecat_header = ecat_read_u16_le(&buffer[14]);
    uint16_t ecat_type = (ecat_header >> 12) & 0xF;
    uint16_t ecat_length = ecat_header & 0x7FF;

    /* Check protocol type */
    if (ecat_type != ECAT_TYPE_DLPDU) {
        dl_set_error(DL_ERROR_INVALID_FRAME, "ecat_frame_parser_validate: invalid protocol type");
        return DL_STATUS_ERROR;
    }

    /* Store EtherCAT data length */
    parser->ecat_data_length = ecat_length;

    /* Verify frame length */
    if (16 + ecat_length + 4 > parser->buffer_size) {
        dl_set_error(DL_ERROR_INVALID_FRAME, "ecat_frame_parser_validate: frame length mismatch");
        return DL_STATUS_ERROR;
    }

    return DL_STATUS_SUCCESS;
}

dl_status_t ecat_frame_parser_next_datagram(ecat_frame_parser_t* parser,
                                              ecat_parsed_datagram_t* datagram)
{
    if (parser == NULL || datagram == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "ecat_frame_parser_next_datagram: invalid parameter");
        return DL_STATUS_INVALID_PARAM;
    }

    /* Check if we've reached the end of EtherCAT data */
    if (parser->current_offset >= 16 + parser->ecat_data_length) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "ecat_frame_parser_next_datagram: no more datagrams");
        return DL_STATUS_ERROR;
    }

    const uint8_t* dgram = &parser->buffer[parser->current_offset];

    /* Parse datagram header */
    datagram->cmd = (ecat_cmd_t)dgram[0];
    datagram->idx = dgram[1];
    datagram->address = ecat_read_u32_le(&dgram[2]);

    /* Parse length field with flags */
    uint16_t length_field = ecat_read_u16_le(&dgram[6]);
    datagram->length = length_field & 0x7FF;
    datagram->circulating = (length_field & 0x4000) ? true : false;
    datagram->more = (length_field & 0x8000) ? true : false;

    /* Parse IRQ field */
    datagram->irq = ecat_read_u16_le(&dgram[8]);

    /* Set data pointer */
    datagram->data = &dgram[10];

    /* Parse WKC */
    datagram->wkc = ecat_read_u16_le(&dgram[10 + datagram->length]);

    /* Update offset */
    uint16_t datagram_size = ECAT_DATAGRAM_HEADER_SIZE + datagram->length + 2;
    parser->current_offset += datagram_size;
    parser->datagram_count++;

    return DL_STATUS_SUCCESS;
}

bool ecat_frame_parser_has_more(const ecat_frame_parser_t* parser)
{
    if (parser == NULL) {
        return false;
    }

    return (parser->current_offset < 16 + parser->ecat_data_length);
}

dl_status_t ecat_frame_parser_reset(ecat_frame_parser_t* parser)
{
    if (parser == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "ecat_frame_parser_reset: invalid parameter");
        return DL_STATUS_INVALID_PARAM;
    }

    parser->current_offset = 16;
    parser->datagram_count = 0;

    return DL_STATUS_SUCCESS;
}

uint8_t ecat_frame_parser_get_datagram_count(const ecat_frame_parser_t* parser)
{
    if (parser == NULL) {
        return 0;
    }
    return parser->datagram_count;
}

dl_status_t ecat_frame_parser_get_src_mac(const ecat_frame_parser_t* parser,
                                           uint8_t mac_address[6])
{
    if (parser == NULL || mac_address == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "ecat_frame_parser_get_src_mac: invalid parameter");
        return DL_STATUS_INVALID_PARAM;
    }

    memcpy(mac_address, &parser->buffer[6], 6);
    return DL_STATUS_SUCCESS;
}

dl_status_t ecat_frame_parser_get_dst_mac(const ecat_frame_parser_t* parser,
                                           uint8_t mac_address[6])
{
    if (parser == NULL || mac_address == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "ecat_frame_parser_get_dst_mac: invalid parameter");
        return DL_STATUS_INVALID_PARAM;
    }

    memcpy(mac_address, &parser->buffer[0], 6);
    return DL_STATUS_SUCCESS;
}

bool ecat_frame_parser_verify_fcs(const ecat_frame_parser_t* parser)
{
    if (parser == NULL) {
        return false;
    }

    return ecat_verify_fcs(parser->buffer, parser->buffer_size);
}
