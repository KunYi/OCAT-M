/**
 * @file dc.c
 * @brief EtherCAT Distributed Clocks - Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "ethercat/dc.h"
#include "ethercat/frame.h"
#include "ethercat/frame_builder.h"
#include "ethercat/frame_parser.h"
#include "ethercat/dll.h"
#include "ethercat/hal.h"
#include <string.h>
#include <stdlib.h>

/* ========================================================================== */
/* Constants                                                                 */
/* ========================================================================== */

#define DC_DEFAULT_FILTER_DEPTH_TIME    12      /**< Default time filter depth */
#define DC_DEFAULT_FILTER_DEPTH_SPEED   12      /**< Default speed filter depth */
#define DC_DEFAULT_SYNC_IMPULSE_NS      100     /**< Default sync impulse length (ns) */
#define DC_PROPAGATION_DELAY_SAMPLES    10      /**< Number of samples for delay measurement */

/* EtherCAT broadcast MAC address */
static const uint8_t ECAT_BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* ========================================================================== */
/* DC Context                                                                */
/* ========================================================================== */

typedef struct {
    bool initialized;
    uint8_t master_mac[6];
    dc_statistics_t stats[256];         /**< Statistics per slave */
} dc_context_t;

static dc_context_t g_dc_context = {0};

/* ========================================================================== */
/* Helper Functions                                                          */
/* ========================================================================== */

/**
 * @brief Send frame and wait for response
 */
static dc_status_t send_and_receive(const uint8_t* frame_data,
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
        return DC_STATUS_ERROR;
    }

    /* Wait for response */
    uint64_t start_time = hal_get_time_ms();

    while (1) {
        /* Check timeout */
        if (timeout_ms > 0) {
            uint64_t elapsed = hal_get_time_ms() - start_time;
            if (elapsed >= timeout_ms) {
                return DC_STATUS_TIMEOUT;
            }
        }

        /* Check for received frame */
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
                    return DC_STATUS_SUCCESS;
                }
            }

            hal_free_rx_buffer(rx_buffer);
        }

        hal_sleep_us(100);
    }

    return DC_STATUS_TIMEOUT;
}

/**
 * @brief Read ESC register using FPRD
 */
static dc_status_t fprd_register(uint16_t station_address,
                                  uint16_t reg_address,
                                  uint8_t* data,
                                  uint16_t length,
                                  uint32_t timeout_ms)
{
    uint8_t frame_buffer[1518];
    ecat_frame_builder_t builder;

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer, sizeof(frame_buffer),
                                                  g_dc_context.master_mac, ECAT_BROADCAST_MAC);
    if (status != DL_STATUS_SUCCESS) {
        return DC_STATUS_ERROR;
    }

    /* FPRD address format: station_address in high 16 bits, offset in low 16 bits */
    uint32_t fprd_addr = ((uint32_t)station_address << 16) | reg_address;

    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_FPRD, 0,
                                              fprd_addr, NULL, length, false);
    if (status != DL_STATUS_SUCCESS) {
        return DC_STATUS_ERROR;
    }

    uint16_t frame_length;
    status = ecat_frame_builder_finalize(&builder, &frame_length);
    if (status != DL_STATUS_SUCCESS) {
        return DC_STATUS_ERROR;
    }

    /* Send and receive */
    ecat_parsed_datagram_t response;
    dc_status_t dc_status = send_and_receive(frame_buffer, frame_length, &response, timeout_ms);

    if (dc_status != DC_STATUS_SUCCESS) {
        return dc_status;
    }

    /* Copy data */
    if (response.length >= length) {
        memcpy(data, response.data, length);
        return DC_STATUS_SUCCESS;
    }

    return DC_STATUS_ERROR;
}

/**
 * @brief Write ESC register using FPWR
 */
static dc_status_t fpwr_register(uint16_t station_address,
                                  uint16_t reg_address,
                                  const uint8_t* data,
                                  uint16_t length,
                                  uint32_t timeout_ms)
{
    uint8_t frame_buffer[1518];
    ecat_frame_builder_t builder;

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer, sizeof(frame_buffer),
                                                  g_dc_context.master_mac, ECAT_BROADCAST_MAC);
    if (status != DL_STATUS_SUCCESS) {
        return DC_STATUS_ERROR;
    }

    /* FPWR address format: station_address in high 16 bits, offset in low 16 bits */
    uint32_t fpwr_addr = ((uint32_t)station_address << 16) | reg_address;

    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_FPWR, 0,
                                              fpwr_addr, data, length, false);
    if (status != DL_STATUS_SUCCESS) {
        return DC_STATUS_ERROR;
    }

    uint16_t frame_length;
    status = ecat_frame_builder_finalize(&builder, &frame_length);
    if (status != DL_STATUS_SUCCESS) {
        return DC_STATUS_ERROR;
    }

    /* Send and receive */
    ecat_parsed_datagram_t response;
    dc_status_t dc_status = send_and_receive(frame_buffer, frame_length, &response, timeout_ms);

    return dc_status;
}

/* ========================================================================== */
/* Initialization and Shutdown                                               */
/* ========================================================================== */

dc_status_t dc_init(void)
{
    if (g_dc_context.initialized) {
        return DC_STATUS_ERROR;
    }

    memset(&g_dc_context, 0, sizeof(dc_context_t));

    /* Get master MAC address from HAL */
    hal_device_info_t dev_info;
    if (hal_get_device_info(&dev_info) == HAL_STATUS_SUCCESS) {
        memcpy(g_dc_context.master_mac, dev_info.mac_address, 6);
    }

    g_dc_context.initialized = true;

    return DC_STATUS_SUCCESS;
}

dc_status_t dc_shutdown(void)
{
    if (!g_dc_context.initialized) {
        return DC_STATUS_NOT_INITIALIZED;
    }

    memset(&g_dc_context, 0, sizeof(dc_context_t));
    return DC_STATUS_SUCCESS;
}

/* ========================================================================== */
/* DC Support Check                                                          */
/* ========================================================================== */

dc_status_t dc_check_support(uint16_t station_address,
                              bool* supported,
                              uint32_t timeout_ms)
{
    if (!g_dc_context.initialized || supported == NULL) {
        return DC_STATUS_INVALID_PARAM;
    }

    /* Try to read DC system time register */
    uint8_t time_data[8];
    dc_status_t status = fprd_register(station_address, DC_REG_SYSTEM_TIME,
                                        time_data, 8, timeout_ms);

    if (status == DC_STATUS_SUCCESS) {
        /* Check if time is non-zero (indicates DC support) */
        bool has_time = false;
        for (int i = 0; i < 8; i++) {
            if (time_data[i] != 0) {
                has_time = true;
                break;
            }
        }
        *supported = has_time;
    } else {
        *supported = false;
    }

    return DC_STATUS_SUCCESS;
}

/* ========================================================================== */
/* System Time Operations                                                    */
/* ========================================================================== */

dc_status_t dc_read_system_time(uint16_t station_address,
                                 uint64_t* system_time,
                                 uint32_t timeout_ms)
{
    if (!g_dc_context.initialized || system_time == NULL) {
        return DC_STATUS_INVALID_PARAM;
    }

    uint8_t time_data[8];
    dc_status_t status = fprd_register(station_address, DC_REG_SYSTEM_TIME,
                                        time_data, 8, timeout_ms);

    if (status != DC_STATUS_SUCCESS) {
        return status;
    }

    /* Convert to 64-bit time (little-endian) */
    *system_time = 0;
    for (int i = 0; i < 8; i++) {
        *system_time |= ((uint64_t)time_data[i]) << (i * 8);
    }

    return DC_STATUS_SUCCESS;
}

dc_status_t dc_write_system_time_offset(uint16_t station_address,
                                         int64_t time_offset,
                                         uint32_t timeout_ms)
{
    if (!g_dc_context.initialized) {
        return DC_STATUS_INVALID_PARAM;
    }

    /* Convert to bytes (little-endian) */
    uint8_t offset_data[8];
    for (int i = 0; i < 8; i++) {
        offset_data[i] = (time_offset >> (i * 8)) & 0xFF;
    }

    return fpwr_register(station_address, DC_REG_SYSTEM_TIME_OFFSET,
                         offset_data, 8, timeout_ms);
}

/* ========================================================================== */
/* Propagation Delay Measurement                                             */
/* ========================================================================== */

dc_status_t dc_measure_propagation_delay(uint16_t station_address,
                                          int32_t* delay,
                                          uint32_t timeout_ms)
{
    if (!g_dc_context.initialized || delay == NULL) {
        return DC_STATUS_INVALID_PARAM;
    }

    /* Read receive times for all ports */
    uint32_t receive_times[4] = {0};

    for (int port = 0; port < 4; port++) {
        uint16_t reg_addr = DC_REG_RECEIVE_TIME_PORT0 + (port * 4);
        uint8_t time_data[4];

        dc_status_t status = fprd_register(station_address, reg_addr,
                                            time_data, 4, timeout_ms);

        if (status == DC_STATUS_SUCCESS) {
            receive_times[port] = time_data[0] | (time_data[1] << 8) |
                                  (time_data[2] << 16) | (time_data[3] << 24);
        }
    }

    /* Calculate propagation delay as average of non-zero receive times */
    uint32_t sum = 0;
    uint32_t count = 0;

    for (int i = 0; i < 4; i++) {
        if (receive_times[i] != 0) {
            sum += receive_times[i];
            count++;
        }
    }

    if (count > 0) {
        *delay = (int32_t)(sum / count);
    } else {
        *delay = 0;
    }

    return DC_STATUS_SUCCESS;
}

/* ========================================================================== */
/* SYNC Configuration                                                        */
/* ========================================================================== */

dc_status_t dc_configure_sync0(uint16_t station_address,
                                const dc_sync0_config_t* sync0_config,
                                uint32_t timeout_ms)
{
    if (!g_dc_context.initialized || sync0_config == NULL) {
        return DC_STATUS_INVALID_PARAM;
    }

    dc_status_t status;

    /* Write SYNC0 cycle time */
    uint8_t cycle_data[4];
    cycle_data[0] = sync0_config->cycle_time_ns & 0xFF;
    cycle_data[1] = (sync0_config->cycle_time_ns >> 8) & 0xFF;
    cycle_data[2] = (sync0_config->cycle_time_ns >> 16) & 0xFF;
    cycle_data[3] = (sync0_config->cycle_time_ns >> 24) & 0xFF;

    status = fpwr_register(station_address, DC_REG_SYNC0_CYCLE_TIME,
                           cycle_data, 4, timeout_ms);
    if (status != DC_STATUS_SUCCESS) {
        return status;
    }

    /* Write sync impulse length */
    uint8_t impulse_data[2];
    impulse_data[0] = sync0_config->sync_impulse_length_ns & 0xFF;
    impulse_data[1] = (sync0_config->sync_impulse_length_ns >> 8) & 0xFF;

    status = fpwr_register(station_address, DC_REG_SYNC_IMPULSE_LENGTH,
                           impulse_data, 2, timeout_ms);
    if (status != DC_STATUS_SUCCESS) {
        return status;
    }

    /* Enable/disable SYNC0 */
    uint8_t activation = 0;
    if (sync0_config->enabled) {
        activation |= DC_ACTIVATION_SYNC0;
    }

    status = fpwr_register(station_address, DC_REG_ACTIVATION,
                           &activation, 1, timeout_ms);

    return status;
}

dc_status_t dc_configure_sync1(uint16_t station_address,
                                const dc_sync1_config_t* sync1_config,
                                uint32_t timeout_ms)
{
    if (!g_dc_context.initialized || sync1_config == NULL) {
        return DC_STATUS_INVALID_PARAM;
    }

    dc_status_t status;

    /* Write SYNC1 cycle time */
    uint8_t cycle_data[4];
    cycle_data[0] = sync1_config->cycle_time_ns & 0xFF;
    cycle_data[1] = (sync1_config->cycle_time_ns >> 8) & 0xFF;
    cycle_data[2] = (sync1_config->cycle_time_ns >> 16) & 0xFF;
    cycle_data[3] = (sync1_config->cycle_time_ns >> 24) & 0xFF;

    status = fpwr_register(station_address, DC_REG_SYNC1_CYCLE_TIME,
                           cycle_data, 4, timeout_ms);
    if (status != DC_STATUS_SUCCESS) {
        return status;
    }

    /* Enable/disable SYNC1 */
    uint8_t activation = 0;
    if (sync1_config->enabled) {
        activation |= DC_ACTIVATION_SYNC1;
    }

    status = fpwr_register(station_address, DC_REG_ACTIVATION,
                           &activation, 1, timeout_ms);

    return status;
}

dc_status_t dc_configure_slave(const dc_slave_config_t* config,
                                uint32_t timeout_ms)
{
    if (!g_dc_context.initialized || config == NULL) {
        return DC_STATUS_INVALID_PARAM;
    }

    dc_status_t status;

    /* Configure SYNC0 if enabled */
    if (config->sync0.enabled) {
        status = dc_configure_sync0(config->station_address, &config->sync0, timeout_ms);
        if (status != DC_STATUS_SUCCESS) {
            return status;
        }
    }

    /* Configure SYNC1 if enabled */
    if (config->sync1.enabled) {
        status = dc_configure_sync1(config->station_address, &config->sync1, timeout_ms);
        if (status != DC_STATUS_SUCCESS) {
            return status;
        }
    }

    return DC_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Reference Clock Selection                                                 */
/* ========================================================================== */

dc_status_t dc_select_reference_clock(uint16_t slave_count,
                                       const uint16_t* station_addresses,
                                       uint16_t* reference_address,
                                       uint32_t timeout_ms)
{
    if (!g_dc_context.initialized || station_addresses == NULL || reference_address == NULL) {
        return DC_STATUS_INVALID_PARAM;
    }

    if (slave_count == 0) {
        return DC_STATUS_INVALID_PARAM;
    }

    /* Select first slave with DC support as reference */
    for (uint16_t i = 0; i < slave_count; i++) {
        bool supported = false;
        dc_status_t status = dc_check_support(station_addresses[i], &supported, timeout_ms);

        if (status == DC_STATUS_SUCCESS && supported) {
            *reference_address = station_addresses[i];
            return DC_STATUS_SUCCESS;
        }
    }

    return DC_STATUS_NOT_SUPPORTED;
}

/* ========================================================================== */
/* Synchronization                                                           */
/* ========================================================================== */

dc_status_t dc_synchronize_slaves(uint16_t slave_count,
                                   const uint16_t* station_addresses,
                                   uint16_t reference_address,
                                   uint32_t timeout_ms)
{
    if (!g_dc_context.initialized || station_addresses == NULL) {
        return DC_STATUS_INVALID_PARAM;
    }

    /* Read reference clock time */
    uint64_t reference_time = 0;
    dc_status_t status = dc_read_system_time(reference_address, &reference_time, timeout_ms);
    if (status != DC_STATUS_SUCCESS) {
        return status;
    }

    /* Synchronize each slave to reference */
    for (uint16_t i = 0; i < slave_count; i++) {
        uint16_t addr = station_addresses[i];

        if (addr == reference_address) {
            continue;  /* Skip reference clock */
        }

        /* Read slave time */
        uint64_t slave_time = 0;
        status = dc_read_system_time(addr, &slave_time, timeout_ms);
        if (status != DC_STATUS_SUCCESS) {
            continue;  /* Skip this slave */
        }

        /* Calculate offset */
        int64_t offset = (int64_t)reference_time - (int64_t)slave_time;

        /* Write offset */
        status = dc_write_system_time_offset(addr, offset, timeout_ms);
        if (status != DC_STATUS_SUCCESS) {
            continue;  /* Skip this slave */
        }
    }

    return DC_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Drift Compensation                                                        */
/* ========================================================================== */

dc_status_t dc_compensate_drift(uint16_t station_address,
                                 uint32_t timeout_ms)
{
    if (!g_dc_context.initialized) {
        return DC_STATUS_INVALID_PARAM;
    }

    /* Read system time difference */
    uint8_t diff_data[4];
    dc_status_t status = fprd_register(station_address, DC_REG_SYSTEM_TIME_DIFF,
                                        diff_data, 4, timeout_ms);

    if (status != DC_STATUS_SUCCESS) {
        return status;
    }

    int32_t time_diff = (int32_t)(diff_data[0] | (diff_data[1] << 8) |
                                   (diff_data[2] << 16) | (diff_data[3] << 24));

    /* If difference is significant, apply correction */
    if (time_diff > 1000 || time_diff < -1000) {  /* > 1 microsecond */
        status = dc_write_system_time_offset(station_address, -time_diff, timeout_ms);
    }

    return status;
}

/* ========================================================================== */
/* Information and Statistics                                                */
/* ========================================================================== */

dc_status_t dc_get_slave_info(uint16_t station_address,
                               dc_slave_info_t* info,
                               uint32_t timeout_ms)
{
    if (!g_dc_context.initialized || info == NULL) {
        return DC_STATUS_INVALID_PARAM;
    }

    memset(info, 0, sizeof(dc_slave_info_t));
    info->station_address = station_address;

    /* Check DC support */
    dc_status_t status = dc_check_support(station_address, &info->dc_supported, timeout_ms);
    if (status != DC_STATUS_SUCCESS || !info->dc_supported) {
        return DC_STATUS_NOT_SUPPORTED;
    }

    /* Read system time */
    status = dc_read_system_time(station_address, &info->system_time, timeout_ms);
    if (status != DC_STATUS_SUCCESS) {
        return status;
    }

    /* Read time offset */
    uint8_t offset_data[8];
    status = fprd_register(station_address, DC_REG_SYSTEM_TIME_OFFSET,
                           offset_data, 8, timeout_ms);
    if (status == DC_STATUS_SUCCESS) {
        info->time_offset = 0;
        for (int i = 0; i < 8; i++) {
            info->time_offset |= ((int64_t)offset_data[i]) << (i * 8);
        }
    }

    /* Read propagation delay */
    int32_t delay;
    status = dc_measure_propagation_delay(station_address, &delay, timeout_ms);
    if (status == DC_STATUS_SUCCESS) {
        info->time_delay = delay;
    }

    return DC_STATUS_SUCCESS;
}

dc_status_t dc_get_statistics(uint16_t station_address,
                               dc_statistics_t* stats)
{
    if (!g_dc_context.initialized || stats == NULL) {
        return DC_STATUS_INVALID_PARAM;
    }

    if (station_address >= 256) {
        return DC_STATUS_INVALID_PARAM;
    }

    memcpy(stats, &g_dc_context.stats[station_address], sizeof(dc_statistics_t));
    return DC_STATUS_SUCCESS;
}

dc_status_t dc_reset_statistics(uint16_t station_address)
{
    if (!g_dc_context.initialized) {
        return DC_STATUS_INVALID_PARAM;
    }

    if (station_address >= 256) {
        return DC_STATUS_INVALID_PARAM;
    }

    memset(&g_dc_context.stats[station_address], 0, sizeof(dc_statistics_t));
    return DC_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Enable/Disable                                                            */
/* ========================================================================== */

dc_status_t dc_enable(uint16_t station_address, uint32_t timeout_ms)
{
    if (!g_dc_context.initialized) {
        return DC_STATUS_INVALID_PARAM;
    }

    /* Enable cyclic operation */
    uint8_t control = DC_CYCLIC_OPERATION;
    return fpwr_register(station_address, DC_REG_CYCLIC_UNIT_CONTROL,
                         &control, 1, timeout_ms);
}

dc_status_t dc_disable(uint16_t station_address, uint32_t timeout_ms)
{
    if (!g_dc_context.initialized) {
        return DC_STATUS_INVALID_PARAM;
    }

    /* Disable cyclic operation */
    uint8_t control = 0;
    return fpwr_register(station_address, DC_REG_CYCLIC_UNIT_CONTROL,
                         &control, 1, timeout_ms);
}
