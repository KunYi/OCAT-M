/**
 * @file config.c
 * @brief EtherCAT Slave Configuration - Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "ethercat/config.h"
#include "ethercat/scan.h"
#include "ethercat/frame.h"
#include "ethercat/frame_builder.h"
#include "ethercat/frame_parser.h"
#include "ethercat/dll.h"
#include "ethercat/hal.h"
#include <string.h>
#include <stdlib.h>

/* ========================================================================== */
/* ESC Register Addresses                                                    */
/* ========================================================================== */

#define ESC_REG_SM_BASE         0x0800  /**< Sync Manager base address */
#define ESC_REG_FMMU_BASE       0x0600  /**< FMMU base address */
#define ESC_REG_AL_CONTROL      0x0120  /**< AL Control register */
#define ESC_REG_AL_STATUS       0x0130  /**< AL Status register */

/* EtherCAT broadcast MAC address */
static const uint8_t ECAT_BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* ========================================================================== */
/* Configuration Context                                                     */
/* ========================================================================== */

typedef struct {
    bool initialized;
    uint8_t master_mac[6];
} config_context_t;

static config_context_t g_config_context = {0};

/* ========================================================================== */
/* Helper Functions                                                          */
/* ========================================================================== */

/**
 * @brief Send frame and wait for response
 */
static config_status_t send_and_receive(const uint8_t* frame_data,
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
        return CONFIG_STATUS_ERROR;
    }

    /* Wait for response */
    uint64_t start_time = hal_get_time_ms();

    while (1) {
        /* Check timeout */
        if (timeout_ms > 0) {
            uint64_t elapsed = hal_get_time_ms() - start_time;
            if (elapsed >= timeout_ms) {
                return CONFIG_STATUS_TIMEOUT;
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
                    return CONFIG_STATUS_SUCCESS;
                }
            }

            hal_free_rx_buffer(rx_buffer);
        }

        hal_sleep_us(100);
    }

    return CONFIG_STATUS_TIMEOUT;
}

/**
 * @brief Read ESC register using FPRD
 */
__attribute__((unused))
static config_status_t fprd_register(uint16_t station_address,
                                      uint16_t reg_address,
                                      uint8_t* data,
                                      uint16_t length,
                                      uint32_t timeout_ms)
{
    uint8_t frame_buffer[1518];
    ecat_frame_builder_t builder;

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer, sizeof(frame_buffer),
                                                  g_config_context.master_mac, ECAT_BROADCAST_MAC);
    if (status != DL_STATUS_SUCCESS) {
        return CONFIG_STATUS_ERROR;
    }

    /* FPRD address format: station_address in high 16 bits, offset in low 16 bits */
    uint32_t fprd_addr = ((uint32_t)station_address << 16) | reg_address;

    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_FPRD, 0,
                                              fprd_addr, NULL, length, false);
    if (status != DL_STATUS_SUCCESS) {
        return CONFIG_STATUS_ERROR;
    }

    uint16_t frame_length;
    status = ecat_frame_builder_finalize(&builder, &frame_length);
    if (status != DL_STATUS_SUCCESS) {
        return CONFIG_STATUS_ERROR;
    }

    /* Send and receive */
    ecat_parsed_datagram_t response;
    config_status_t config_status = send_and_receive(frame_buffer, frame_length, &response, timeout_ms);

    if (config_status != CONFIG_STATUS_SUCCESS) {
        return config_status;
    }

    /* Copy data */
    if (response.length >= length) {
        memcpy(data, response.data, length);
        return CONFIG_STATUS_SUCCESS;
    }

    return CONFIG_STATUS_ERROR;
}

/**
 * @brief Write ESC register using FPWR
 */
static config_status_t fpwr_register(uint16_t station_address,
                                      uint16_t reg_address,
                                      const uint8_t* data,
                                      uint16_t length,
                                      uint32_t timeout_ms)
{
    uint8_t frame_buffer[1518];
    ecat_frame_builder_t builder;

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer, sizeof(frame_buffer),
                                                  g_config_context.master_mac, ECAT_BROADCAST_MAC);
    if (status != DL_STATUS_SUCCESS) {
        return CONFIG_STATUS_ERROR;
    }

    /* FPWR address format: station_address in high 16 bits, offset in low 16 bits */
    uint32_t fpwr_addr = ((uint32_t)station_address << 16) | reg_address;

    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_FPWR, 0,
                                              fpwr_addr, data, length, false);
    if (status != DL_STATUS_SUCCESS) {
        return CONFIG_STATUS_ERROR;
    }

    uint16_t frame_length;
    status = ecat_frame_builder_finalize(&builder, &frame_length);
    if (status != DL_STATUS_SUCCESS) {
        return CONFIG_STATUS_ERROR;
    }

    /* Send and receive */
    ecat_parsed_datagram_t response;
    config_status_t config_status = send_and_receive(frame_buffer, frame_length, &response, timeout_ms);

    return config_status;
}

/* ========================================================================== */
/* Initialization and Shutdown                                               */
/* ========================================================================== */

config_status_t config_init(void)
{
    if (g_config_context.initialized) {
        return CONFIG_STATUS_ERROR;
    }

    memset(&g_config_context, 0, sizeof(config_context_t));

    /* Get master MAC address from HAL */
    hal_device_info_t dev_info;
    if (hal_get_device_info(&dev_info) == HAL_STATUS_SUCCESS) {
        memcpy(g_config_context.master_mac, dev_info.mac_address, 6);
    }

    g_config_context.initialized = true;

    return CONFIG_STATUS_SUCCESS;
}

config_status_t config_shutdown(void)
{
    if (!g_config_context.initialized) {
        return CONFIG_STATUS_ERROR;
    }

    memset(&g_config_context, 0, sizeof(config_context_t));
    return CONFIG_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Sync Manager Configuration                                                */
/* ========================================================================== */

config_status_t config_read_sync_managers(uint16_t station_address,
                                           sii_sync_manager_t* sm_configs,
                                           uint8_t max_count,
                                           uint8_t* sm_count,
                                           uint32_t timeout_ms)
{
    if (!g_config_context.initialized || sm_configs == NULL || sm_count == NULL) {
        return CONFIG_STATUS_INVALID_PARAM;
    }

    /* Read SYNC_MANAGER category from EEPROM */
    uint8_t category_data[256];
    uint16_t bytes_read;

    scan_status_t status = scan_read_eeprom_category(station_address,
                                                      SII_CAT_SYNC_MANAGER,
                                                      category_data,
                                                      sizeof(category_data),
                                                      &bytes_read,
                                                      timeout_ms);

    if (status != SCAN_STATUS_SUCCESS) {
        *sm_count = 0;
        return CONFIG_STATUS_EEPROM_ERROR;
    }

    /* Parse Sync Manager configurations */
    uint8_t count = bytes_read / sizeof(sii_sync_manager_t);
    if (count > max_count) {
        count = max_count;
    }

    memcpy(sm_configs, category_data, count * sizeof(sii_sync_manager_t));
    *sm_count = count;

    return CONFIG_STATUS_SUCCESS;
}

config_status_t config_write_sync_manager(uint16_t station_address,
                                           uint8_t sm_index,
                                           const sm_config_t* sm_config,
                                           uint32_t timeout_ms)
{
    if (!g_config_context.initialized || sm_config == NULL) {
        return CONFIG_STATUS_INVALID_PARAM;
    }

    if (sm_index >= SM_MAX_COUNT) {
        return CONFIG_STATUS_INVALID_PARAM;
    }

    /* Calculate SM register address */
    uint16_t sm_addr = ESC_REG_SM_BASE + (sm_index * SM_CONFIG_SIZE);

    /* Write SM configuration */
    uint8_t sm_data[SM_CONFIG_SIZE];
    sm_data[0] = sm_config->physical_start_address & 0xFF;
    sm_data[1] = (sm_config->physical_start_address >> 8) & 0xFF;
    sm_data[2] = sm_config->length & 0xFF;
    sm_data[3] = (sm_config->length >> 8) & 0xFF;
    sm_data[4] = sm_config->control;
    sm_data[5] = sm_config->status;
    sm_data[6] = sm_config->enable;
    sm_data[7] = sm_config->pdi_control;

    return fpwr_register(station_address, sm_addr, sm_data, SM_CONFIG_SIZE, timeout_ms);
}

/* ========================================================================== */
/* FMMU Configuration                                                        */
/* ========================================================================== */

config_status_t config_read_fmmus(uint16_t station_address,
                                   sii_fmmu_t* fmmu_configs,
                                   uint8_t max_count,
                                   uint8_t* fmmu_count,
                                   uint32_t timeout_ms)
{
    if (!g_config_context.initialized || fmmu_configs == NULL || fmmu_count == NULL) {
        return CONFIG_STATUS_INVALID_PARAM;
    }

    /* Read FMMU category from EEPROM */
    uint8_t category_data[256];
    uint16_t bytes_read;

    scan_status_t status = scan_read_eeprom_category(station_address,
                                                      SII_CAT_FMMU,
                                                      category_data,
                                                      sizeof(category_data),
                                                      &bytes_read,
                                                      timeout_ms);

    if (status != SCAN_STATUS_SUCCESS) {
        *fmmu_count = 0;
        return CONFIG_STATUS_EEPROM_ERROR;
    }

    /* Parse FMMU configurations */
    uint8_t count = bytes_read / sizeof(sii_fmmu_t);
    if (count > max_count) {
        count = max_count;
    }

    memcpy(fmmu_configs, category_data, count * sizeof(sii_fmmu_t));
    *fmmu_count = count;

    return CONFIG_STATUS_SUCCESS;
}

config_status_t config_write_fmmu(uint16_t station_address,
                                   uint8_t fmmu_index,
                                   const fmmu_config_t* fmmu_config,
                                   uint32_t timeout_ms)
{
    if (!g_config_context.initialized || fmmu_config == NULL) {
        return CONFIG_STATUS_INVALID_PARAM;
    }

    if (fmmu_index >= FMMU_MAX_COUNT) {
        return CONFIG_STATUS_INVALID_PARAM;
    }

    /* Calculate FMMU register address */
    uint16_t fmmu_addr = ESC_REG_FMMU_BASE + (fmmu_index * FMMU_CONFIG_SIZE);

    /* Write FMMU configuration */
    uint8_t fmmu_data[FMMU_CONFIG_SIZE];
    fmmu_data[0] = fmmu_config->logical_start_address & 0xFF;
    fmmu_data[1] = (fmmu_config->logical_start_address >> 8) & 0xFF;
    fmmu_data[2] = (fmmu_config->logical_start_address >> 16) & 0xFF;
    fmmu_data[3] = (fmmu_config->logical_start_address >> 24) & 0xFF;
    fmmu_data[4] = fmmu_config->length & 0xFF;
    fmmu_data[5] = (fmmu_config->length >> 8) & 0xFF;
    fmmu_data[6] = fmmu_config->logical_start_bit;
    fmmu_data[7] = fmmu_config->logical_end_bit;
    fmmu_data[8] = fmmu_config->physical_start_address & 0xFF;
    fmmu_data[9] = (fmmu_config->physical_start_address >> 8) & 0xFF;
    fmmu_data[10] = fmmu_config->physical_start_bit;
    fmmu_data[11] = fmmu_config->read_enable;
    fmmu_data[12] = fmmu_config->write_enable;
    fmmu_data[13] = fmmu_config->enable;
    fmmu_data[14] = 0;  /* Reserved */
    fmmu_data[15] = 0;  /* Reserved */

    return fpwr_register(station_address, fmmu_addr, fmmu_data, FMMU_CONFIG_SIZE, timeout_ms);
}

/* ========================================================================== */
/* PDO Configuration                                                         */
/* ========================================================================== */

config_status_t config_read_pdos(uint16_t station_address,
                                  bool is_txpdo,
                                  sii_pdo_t* pdos,
                                  uint16_t max_count,
                                  uint16_t* pdo_count,
                                  uint32_t timeout_ms)
{
    if (!g_config_context.initialized || pdos == NULL || pdo_count == NULL) {
        return CONFIG_STATUS_INVALID_PARAM;
    }

    /* Read PDO category from EEPROM */
    sii_category_t category = is_txpdo ? SII_CAT_TXPDO : SII_CAT_RXPDO;
    uint8_t category_data[512];
    uint16_t bytes_read;

    scan_status_t status = scan_read_eeprom_category(station_address,
                                                      category,
                                                      category_data,
                                                      sizeof(category_data),
                                                      &bytes_read,
                                                      timeout_ms);

    if (status != SCAN_STATUS_SUCCESS) {
        *pdo_count = 0;
        return CONFIG_STATUS_EEPROM_ERROR;
    }

    /* Parse PDO configurations */
    uint16_t offset = 0;
    uint16_t count = 0;

    while (offset < bytes_read && count < max_count) {
        if (offset + sizeof(sii_pdo_t) > bytes_read) {
            break;
        }

        /* Copy PDO header */
        memcpy(&pdos[count], &category_data[offset], sizeof(sii_pdo_t));
        offset += sizeof(sii_pdo_t);

        /* Skip PDO entries (we'll read them separately if needed) */
        uint8_t entry_count = pdos[count].entry_count;
        offset += entry_count * sizeof(sii_pdo_entry_t);

        count++;
    }

    *pdo_count = count;

    return CONFIG_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Mailbox Configuration                                                     */
/* ========================================================================== */

config_status_t config_setup_mailbox(uint16_t station_address,
                                      mailbox_config_t* mailbox_config,
                                      uint32_t timeout_ms)
{
    if (!g_config_context.initialized || mailbox_config == NULL) {
        return CONFIG_STATUS_INVALID_PARAM;
    }

    /* Read GENERAL category to get mailbox configuration */
    uint8_t general_data[128];
    uint16_t bytes_read;

    scan_status_t status = scan_read_eeprom_category(station_address,
                                                      SII_CAT_GENERAL,
                                                      general_data,
                                                      sizeof(general_data),
                                                      &bytes_read,
                                                      timeout_ms);

    if (status != SCAN_STATUS_SUCCESS) {
        return CONFIG_STATUS_EEPROM_ERROR;
    }

    if (bytes_read < sizeof(sii_general_info_t)) {
        return CONFIG_STATUS_ERROR;
    }

    sii_general_info_t* general = (sii_general_info_t*)general_data;

    /* Parse mailbox protocol support */
    mailbox_config->supports_coe = (general->coe_details & 0x01) != 0;
    mailbox_config->supports_foe = (general->foe_details & 0x01) != 0;
    mailbox_config->supports_soe = (general->soe_channels > 0);
    mailbox_config->supports_eoe = (general->eoe_details & 0x01) != 0;

    /* Read Sync Manager configuration for mailbox */
    sii_sync_manager_t sm_configs[SM_MAX_COUNT];
    uint8_t sm_count;

    config_status_t config_status = config_read_sync_managers(station_address,
                                                               sm_configs,
                                                               SM_MAX_COUNT,
                                                               &sm_count,
                                                               timeout_ms);

    if (config_status != CONFIG_STATUS_SUCCESS) {
        return config_status;
    }

    /* Find mailbox Sync Managers (typically SM0 and SM1) */
    bool found_write = false;
    bool found_read = false;

    for (uint8_t i = 0; i < sm_count && i < 4; i++) {
        uint8_t sm_type = sm_configs[i].sm_type;

        if (sm_type == SM_TYPE_MAILBOX_WRITE && !found_write) {
            mailbox_config->write_address = sm_configs[i].physical_start_address;
            mailbox_config->write_size = sm_configs[i].length;
            mailbox_config->write_sm = i;
            found_write = true;
        } else if (sm_type == SM_TYPE_MAILBOX_READ && !found_read) {
            mailbox_config->read_address = sm_configs[i].physical_start_address;
            mailbox_config->read_size = sm_configs[i].length;
            mailbox_config->read_sm = i;
            found_read = true;
        }
    }

    if (!found_write || !found_read) {
        return CONFIG_STATUS_ERROR;
    }

    /* Configure mailbox Sync Managers */
    for (uint8_t i = 0; i < 2; i++) {
        sm_config_t sm_config;
        sm_config.physical_start_address = sm_configs[i].physical_start_address;
        sm_config.length = sm_configs[i].length;
        sm_config.control = sm_configs[i].control_register;
        sm_config.status = 0;
        sm_config.enable = 0x01;  /* Enable SM */
        sm_config.pdi_control = 0;

        config_status = config_write_sync_manager(station_address, i, &sm_config, timeout_ms);
        if (config_status != CONFIG_STATUS_SUCCESS) {
            return config_status;
        }
    }

    return CONFIG_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Complete Slave Configuration                                              */
/* ========================================================================== */

config_status_t config_configure_slave(uint16_t station_address,
                                        slave_config_t* slave_config,
                                        uint32_t timeout_ms)
{
    if (!g_config_context.initialized || slave_config == NULL) {
        return CONFIG_STATUS_INVALID_PARAM;
    }

    memset(slave_config, 0, sizeof(slave_config_t));
    slave_config->station_address = station_address;

    config_status_t status;

    /* Read Sync Manager configuration */
    sii_sync_manager_t sii_sms[SM_MAX_COUNT];
    status = config_read_sync_managers(station_address, sii_sms, SM_MAX_COUNT,
                                        &slave_config->sm_count, timeout_ms);

    if (status == CONFIG_STATUS_SUCCESS && slave_config->sm_count > 0) {
        /* Convert SII SM configs to runtime SM configs */
        for (uint8_t i = 0; i < slave_config->sm_count; i++) {
            slave_config->sync_managers[i].physical_start_address = sii_sms[i].physical_start_address;
            slave_config->sync_managers[i].length = sii_sms[i].length;
            slave_config->sync_managers[i].control = sii_sms[i].control_register;
            slave_config->sync_managers[i].status = 0;
            slave_config->sync_managers[i].enable = sii_sms[i].enable;
            slave_config->sync_managers[i].pdi_control = 0;
        }
    }

    /* Setup mailbox if supported */
    status = config_setup_mailbox(station_address, &slave_config->mailbox, timeout_ms);
    if (status == CONFIG_STATUS_SUCCESS) {
        slave_config->has_mailbox = true;
    } else {
        slave_config->has_mailbox = false;
    }

    /* Read FMMU configuration */
    sii_fmmu_t sii_fmmus[FMMU_MAX_COUNT];
    status = config_read_fmmus(station_address, sii_fmmus, FMMU_MAX_COUNT,
                                &slave_config->fmmu_count, timeout_ms);

    /* Read PDO configuration */
    sii_pdo_t txpdos[64];
    sii_pdo_t rxpdos[64];
    uint16_t txpdo_count = 0;
    uint16_t rxpdo_count = 0;

    config_read_pdos(station_address, true, txpdos, 64, &txpdo_count, timeout_ms);
    config_read_pdos(station_address, false, rxpdos, 64, &rxpdo_count, timeout_ms);

    /* Calculate process data sizes */
    slave_config->input_size = 0;
    slave_config->output_size = 0;

    for (uint16_t i = 0; i < txpdo_count; i++) {
        /* TxPDO = inputs (Slave -> Master) */
        /* Size calculation would require reading PDO entries */
        /* For now, use SM configuration */
    }

    for (uint16_t i = 0; i < rxpdo_count; i++) {
        /* RxPDO = outputs (Master -> Slave) */
        /* Size calculation would require reading PDO entries */
    }

    /* Use Sync Manager sizes as approximation */
    for (uint8_t i = 0; i < slave_config->sm_count; i++) {
        uint8_t sm_type = sii_sms[i].sm_type;
        if (sm_type == SM_TYPE_PROCESS_DATA_READ) {
            slave_config->input_size += sii_sms[i].length;
        } else if (sm_type == SM_TYPE_PROCESS_DATA_WRITE) {
            slave_config->output_size += sii_sms[i].length;
        }
    }

    slave_config->configured = true;

    return CONFIG_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Process Data Calculation                                                  */
/* ========================================================================== */

config_status_t config_calculate_process_data(slave_config_t* slave_configs,
                                               uint16_t slave_count,
                                               uint32_t* total_input_size,
                                               uint32_t* total_output_size)
{
    if (!g_config_context.initialized || slave_configs == NULL) {
        return CONFIG_STATUS_INVALID_PARAM;
    }

    if (total_input_size == NULL || total_output_size == NULL) {
        return CONFIG_STATUS_INVALID_PARAM;
    }

    uint32_t input_offset = 0;
    uint32_t output_offset = 0;

    /* Calculate offsets for each slave */
    for (uint16_t i = 0; i < slave_count; i++) {
        slave_config_t* config = &slave_configs[i];

        /* Assign input offset */
        config->input_offset = input_offset;
        input_offset += config->input_size;

        /* Assign output offset */
        config->output_offset = output_offset;
        output_offset += config->output_size;
    }

    *total_input_size = input_offset;
    *total_output_size = output_offset;

    return CONFIG_STATUS_SUCCESS;
}
