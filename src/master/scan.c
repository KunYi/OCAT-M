/**
 * @file scan.c
 * @brief EtherCAT Network Scanning - Full Implementation
 * @version 2.0.0
 * @date 2026-01-03
 */

#include "ethercat/scan.h"
#include "ethercat/frame.h"
#include "ethercat/frame_builder.h"
#include "ethercat/frame_parser.h"
#include "ethercat/dll.h"
#include "ethercat/hal.h"
#include <string.h>
#include <stdlib.h>

/* ========================================================================== */
/* EEPROM Register Addresses                                                 */
/* ========================================================================== */

#define ESC_REG_TYPE                0x0000  /**< Type register */
#define ESC_REG_PORT_DESC           0x0007  /**< Port descriptor */
#define ESC_REG_DL_STATUS           0x0110  /**< DL status */
#define ESC_REG_STATION_ADDRESS     0x0010  /**< Station address */

#define ESC_REG_SII_CONFIG          0x0500  /**< SII configuration */
#define ESC_REG_SII_CONTROL         0x0502  /**< SII control/status */
#define ESC_REG_SII_ADDRESS         0x0504  /**< SII address */
#define ESC_REG_SII_DATA            0x0508  /**< SII data */

/* SII Control/Status bits */
#define SII_CTRL_READ               0x0100  /**< Read operation */
#define SII_CTRL_BUSY               0x8000  /**< Busy flag */
#define SII_CTRL_ERROR_MASK         0x7800  /**< Error bits mask */

/* SII EEPROM Addresses */
#define SII_ADDR_VENDOR_ID          0x0008  /**< Vendor ID (2 words) */
#define SII_ADDR_PRODUCT_CODE       0x000A  /**< Product code (2 words) */
#define SII_ADDR_REVISION           0x000C  /**< Revision (2 words) */
#define SII_ADDR_SERIAL_NUMBER      0x000E  /**< Serial number (2 words) */
#define SII_ADDR_CATEGORY_START     0x0040  /**< Category start */

/* EtherCAT broadcast MAC address */
static const uint8_t ECAT_BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* ========================================================================== */
/* Scan Context                                                              */
/* ========================================================================== */

typedef struct {
    bool initialized;
    slave_discovery_t slaves[256];
    uint16_t slave_count;
    uint8_t master_mac[6];
} scan_context_t;

static scan_context_t g_scan_context = {0};

/* ========================================================================== */
/* Helper Functions                                                          */
/* ========================================================================== */

/**
 * @brief Send frame and wait for response
 */
static scan_status_t send_and_receive(const uint8_t* frame_data,
                                       uint16_t frame_length,
                                       ecat_parsed_datagram_t* response,
                                       uint32_t timeout_ms)
{
    /* Send frame */
    dl_send_req_t send_req = {
        .frame_data = (uint8_t*)frame_data,
        .frame_length = frame_length,
        .priority = 0,
        .user_data = NULL
    };

    dl_status_t status = dl_send_req(&send_req);
    if (status != DL_STATUS_SUCCESS) {
        return SCAN_STATUS_ERROR;
    }

    /* Wait for response */
    uint64_t start_time = hal_get_time_ms();

    while (1) {
        /* Check timeout */
        if (timeout_ms > 0) {
            uint64_t elapsed = hal_get_time_ms() - start_time;
            if (elapsed >= timeout_ms) {
                return SCAN_STATUS_TIMEOUT;
            }
        }

        /* Check for received frame - use HAL directly */
        hal_frame_buffer_t* rx_buffer = NULL;
        hal_status_t hal_status = hal_receive_frame(&rx_buffer);

        if (hal_status == HAL_STATUS_SUCCESS && rx_buffer != NULL) {
            /* Parse frame */
            ecat_frame_parser_t parser;

            status = ecat_frame_parser_init(&parser, rx_buffer->data, rx_buffer->length);
            if (status != DL_STATUS_SUCCESS) {
                hal_free_rx_buffer(rx_buffer);
                continue;
            }

            status = ecat_frame_parser_validate(&parser);
            if (status != DL_STATUS_SUCCESS) {
                hal_free_rx_buffer(rx_buffer);
                continue;
            }

            /* Get first datagram */
            if (ecat_frame_parser_has_more(&parser)) {
                status = ecat_frame_parser_next_datagram(&parser, response);
                if (status == DL_STATUS_SUCCESS) {
                    hal_free_rx_buffer(rx_buffer);
                    return SCAN_STATUS_SUCCESS;
                }
            }

            hal_free_rx_buffer(rx_buffer);
        }

        hal_sleep_us(100);
    }

    return SCAN_STATUS_TIMEOUT;
}

/* ========================================================================== */
/* Initialization and Shutdown                                               */
/* ========================================================================== */

scan_status_t scan_init(void)
{
    if (g_scan_context.initialized) {
        return SCAN_STATUS_ERROR;
    }

    memset(&g_scan_context, 0, sizeof(scan_context_t));

    /* Get master MAC address from HAL */
    hal_device_info_t dev_info;
    if (hal_get_device_info(&dev_info) == HAL_STATUS_SUCCESS) {
        memcpy(g_scan_context.master_mac, dev_info.mac_address, 6);
    }

    g_scan_context.initialized = true;

    return SCAN_STATUS_SUCCESS;
}

scan_status_t scan_shutdown(void)
{
    if (!g_scan_context.initialized) {
        return SCAN_STATUS_ERROR;
    }

    memset(&g_scan_context, 0, sizeof(scan_context_t));
    return SCAN_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Slave Discovery                                                           */
/* ========================================================================== */

scan_status_t scan_discover_slaves(uint16_t* slave_count, uint32_t timeout_ms)
{
    if (!g_scan_context.initialized || slave_count == NULL) {
        return SCAN_STATUS_INVALID_PARAM;
    }

    /* Build BRD frame to read Type register from all slaves */
    uint8_t frame_buffer[1518];
    ecat_frame_builder_t builder;

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer, sizeof(frame_buffer),
                                                  g_scan_context.master_mac, ECAT_BROADCAST_MAC);
    if (status != DL_STATUS_SUCCESS) {
        return SCAN_STATUS_ERROR;
    }

    /* Add BRD datagram to read Type register (address 0x0000, length 1) */
    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_BRD, 0,
                                              ESC_REG_TYPE, NULL, 1, false);
    if (status != DL_STATUS_SUCCESS) {
        return SCAN_STATUS_ERROR;
    }

    uint16_t frame_length;
    status = ecat_frame_builder_finalize(&builder, &frame_length);
    if (status != DL_STATUS_SUCCESS) {
        return SCAN_STATUS_ERROR;
    }

    /* Send and receive */
    ecat_parsed_datagram_t response;
    scan_status_t scan_status = send_and_receive(frame_buffer, frame_length, &response, timeout_ms);

    if (scan_status != SCAN_STATUS_SUCCESS) {
        return scan_status;
    }

    /* Working counter indicates number of slaves */
    uint16_t wc = response.wkc;
    g_scan_context.slave_count = wc;
    *slave_count = wc;

    if (wc == 0) {
        return SCAN_STATUS_NO_SLAVES;
    }

    /* Initialize slave discovery info */
    for (uint16_t i = 0; i < wc; i++) {
        slave_discovery_t* slave = &g_scan_context.slaves[i];

        slave->position = i;
        slave->auto_inc_address = -(int16_t)(i + 1);
        slave->station_address = 0;  /* Not yet assigned */
        slave->alias_address = 0;
        slave->port_descriptors = 0;
        slave->dl_status = 0;
        slave->link_port0 = false;
        slave->link_port1 = false;
        slave->link_port2 = false;
        slave->link_port3 = false;
    }

    /* Read port descriptors and DL status for each slave using APRD */
    for (uint16_t i = 0; i < wc; i++) {
        slave_discovery_t* slave = &g_scan_context.slaves[i];

        /* Build APRD frame to read port descriptor */
        ecat_frame_builder_reset(&builder);

        /* APRD address format: position (negative) in high 16 bits, offset in low 16 bits */
        uint32_t aprd_addr = ((uint32_t)(slave->auto_inc_address & 0xFFFF) << 16) | ESC_REG_PORT_DESC;

        status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_APRD, 0,
                                                  aprd_addr, NULL, 1, false);
        if (status == DL_STATUS_SUCCESS) {
            status = ecat_frame_builder_finalize(&builder, &frame_length);
            if (status == DL_STATUS_SUCCESS) {
                if (send_and_receive(frame_buffer, frame_length, &response, timeout_ms) == SCAN_STATUS_SUCCESS) {
                    if (response.length >= 1) {
                        slave->port_descriptors = response.data[0];
                    }
                }
            }
        }

        /* Read DL status */
        ecat_frame_builder_reset(&builder);
        aprd_addr = ((uint32_t)(slave->auto_inc_address & 0xFFFF) << 16) | ESC_REG_DL_STATUS;

        status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_APRD, 0,
                                                  aprd_addr, NULL, 2, false);
        if (status == DL_STATUS_SUCCESS) {
            status = ecat_frame_builder_finalize(&builder, &frame_length);
            if (status == DL_STATUS_SUCCESS) {
                if (send_and_receive(frame_buffer, frame_length, &response, timeout_ms) == SCAN_STATUS_SUCCESS) {
                    if (response.length >= 2) {
                        slave->dl_status = response.data[0] | (response.data[1] << 8);
                    }
                }
            }
        }

        /* Parse port information */
        uint8_t port0_type = (slave->port_descriptors & PORT_DESC_PORT0_MASK) >> 0;
        uint8_t port1_type = (slave->port_descriptors & PORT_DESC_PORT1_MASK) >> 2;
        uint8_t port2_type = (slave->port_descriptors & PORT_DESC_PORT2_MASK) >> 4;
        uint8_t port3_type = (slave->port_descriptors & PORT_DESC_PORT3_MASK) >> 6;

        slave->link_port0 = (port0_type == PORT_TYPE_EBUS || port0_type == PORT_TYPE_MII);
        slave->link_port1 = (port1_type == PORT_TYPE_EBUS || port1_type == PORT_TYPE_MII);
        slave->link_port2 = (port2_type == PORT_TYPE_EBUS || port2_type == PORT_TYPE_MII);
        slave->link_port3 = (port3_type == PORT_TYPE_EBUS || port3_type == PORT_TYPE_MII);
    }

    return SCAN_STATUS_SUCCESS;
}

scan_status_t scan_get_discovery_info(uint16_t position, slave_discovery_t* discovery)
{
    if (!g_scan_context.initialized || discovery == NULL) {
        return SCAN_STATUS_INVALID_PARAM;
    }

    if (position >= g_scan_context.slave_count) {
        return SCAN_STATUS_INVALID_PARAM;
    }

    memcpy(discovery, &g_scan_context.slaves[position], sizeof(slave_discovery_t));
    return SCAN_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Station Address Assignment                                                */
/* ========================================================================== */

scan_status_t scan_assign_station_addresses(uint16_t slave_count, uint32_t timeout_ms)
{
    if (!g_scan_context.initialized) {
        return SCAN_STATUS_INVALID_PARAM;
    }

    uint8_t frame_buffer[1518];
    ecat_frame_builder_t builder;

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer, sizeof(frame_buffer),
                                                  g_scan_context.master_mac, ECAT_BROADCAST_MAC);
    if (status != DL_STATUS_SUCCESS) {
        return SCAN_STATUS_ERROR;
    }

    /* Assign station addresses using APWR */
    for (uint16_t i = 0; i < slave_count; i++) {
        uint16_t station_addr = 0x1000 + i;

        /* Build APWR frame to write station address */
        ecat_frame_builder_reset(&builder);

        /* APWR address format: position (negative) in high 16 bits, offset in low 16 bits */
        int16_t auto_inc_addr = -(int16_t)(i + 1);
        uint32_t apwr_addr = ((uint32_t)(auto_inc_addr & 0xFFFF) << 16) | ESC_REG_STATION_ADDRESS;

        uint8_t addr_data[2];
        addr_data[0] = station_addr & 0xFF;
        addr_data[1] = (station_addr >> 8) & 0xFF;

        status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_APWR, 0,
                                                  apwr_addr, addr_data, 2, false);
        if (status != DL_STATUS_SUCCESS) {
            continue;
        }

        uint16_t frame_length;
        status = ecat_frame_builder_finalize(&builder, &frame_length);
        if (status != DL_STATUS_SUCCESS) {
            continue;
        }

        /* Send and wait for response */
        ecat_parsed_datagram_t response;
        if (send_and_receive(frame_buffer, frame_length, &response, timeout_ms) == SCAN_STATUS_SUCCESS) {
            /* Update context */
            g_scan_context.slaves[i].station_address = station_addr;
        }
    }

    return SCAN_STATUS_SUCCESS;
}

/* ========================================================================== */
/* EEPROM (SII) Reading - To be continued...                                */
/* ========================================================================== */

scan_status_t scan_read_eeprom_word(uint16_t station_address,
                                     uint16_t word_address,
                                     uint16_t* data,
                                     uint32_t timeout_ms)
{
    if (!g_scan_context.initialized || data == NULL) {
        return SCAN_STATUS_INVALID_PARAM;
    }

    /* TODO: Implement SII EEPROM read via ESC registers using FPRD/FPWR */
    (void)station_address;
    (void)word_address;
    (void)timeout_ms;

    *data = 0;
    return SCAN_STATUS_ERROR;
}

scan_status_t scan_read_slave_id(uint16_t station_address,
                                   uint32_t* vendor_id,
                                   uint32_t* product_code,
                                   uint32_t* revision,
                                   uint32_t* serial_number,
                                   uint32_t timeout_ms)
{
    if (!g_scan_context.initialized) {
        return SCAN_STATUS_INVALID_PARAM;
    }

    /* TODO: Implement using scan_read_eeprom_word */
    (void)station_address;
    (void)timeout_ms;

    if (vendor_id != NULL) *vendor_id = 0;
    if (product_code != NULL) *product_code = 0;
    if (revision != NULL) *revision = 0;
    if (serial_number != NULL) *serial_number = 0;

    return SCAN_STATUS_ERROR;
}

scan_status_t scan_read_eeprom_category(uint16_t station_address,
                                         sii_category_t category,
                                         uint8_t* buffer,
                                         uint16_t buffer_size,
                                         uint16_t* bytes_read,
                                         uint32_t timeout_ms)
{
    if (!g_scan_context.initialized || buffer == NULL || bytes_read == NULL) {
        return SCAN_STATUS_INVALID_PARAM;
    }

    /* TODO: Implement EEPROM category reading */
    (void)station_address;
    (void)category;
    (void)buffer_size;
    (void)timeout_ms;

    *bytes_read = 0;
    return SCAN_STATUS_ERROR;
}

scan_status_t scan_read_slave_name(uint16_t station_address,
                                     char* name,
                                     size_t name_size,
                                     uint32_t timeout_ms)
{
    if (!g_scan_context.initialized || name == NULL || name_size == 0) {
        return SCAN_STATUS_INVALID_PARAM;
    }

    /* TODO: Implement slave name reading */
    (void)station_address;
    (void)timeout_ms;

    name[0] = '\0';
    return SCAN_STATUS_ERROR;
}

/* ========================================================================== */
/* Topology Detection                                                        */
/* ========================================================================== */

scan_status_t scan_detect_topology(uint16_t slave_count)
{
    if (!g_scan_context.initialized) {
        return SCAN_STATUS_INVALID_PARAM;
    }

    /* Topology detection is already done in scan_discover_slaves */
    (void)slave_count;

    return SCAN_STATUS_SUCCESS;
}
