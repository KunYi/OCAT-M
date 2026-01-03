/**
 * @file al.c
 * @brief Application Layer Core Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "al_internal.h"
#include "ethercat/hal.h"
#include "ethercat/frame.h"
#include "ethercat/frame_builder.h"
#include "ethercat/frame_parser.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ========================================================================== */
/* Global AL Context                                                         */
/* ========================================================================== */

/* Global AL context (singleton) */
static al_context_t g_al_context = {0};

/* ========================================================================== */
/* Initialization and Configuration                                          */
/* ========================================================================== */

al_status_t al_init(const al_config_t* config)
{
    if (config == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    if (g_al_context.initialized) {
        return AL_STATUS_ERROR;
    }

    /* Initialize context */
    memset(&g_al_context, 0, sizeof(al_context_t));
    memcpy(&g_al_context.config, config, sizeof(al_config_t));

    /* Allocate slave contexts array */
    g_al_context.slaves = (al_slave_context_t*)calloc(config->max_slaves, sizeof(al_slave_context_t));
    if (g_al_context.slaves == NULL) {
        return AL_STATUS_NO_MEMORY;
    }

    g_al_context.slave_count = 0;
    g_al_context.initialized = true;

    return AL_STATUS_SUCCESS;
}

al_status_t al_shutdown(void)
{
    if (!g_al_context.initialized) {
        return AL_STATUS_NOT_INITIALIZED;
    }

    /* Free all slave contexts */
    for (uint16_t i = 0; i < g_al_context.slave_count; i++) {
        al_slave_context_t* slave = &g_al_context.slaves[i];
        al_mailbox_shutdown(slave);
    }

    /* Free slave array */
    if (g_al_context.slaves != NULL) {
        free(g_al_context.slaves);
        g_al_context.slaves = NULL;
    }

    /* Clear context */
    memset(&g_al_context, 0, sizeof(al_context_t));

    return AL_STATUS_SUCCESS;
}

bool al_is_initialized(void)
{
    return g_al_context.initialized;
}

/* ========================================================================== */
/* Slave Context Management                                                  */
/* ========================================================================== */

al_slave_context_t* al_get_slave_context(uint16_t slave_address)
{
    if (!g_al_context.initialized) {
        return NULL;
    }

    for (uint16_t i = 0; i < g_al_context.slave_count; i++) {
        if (g_al_context.slaves[i].station_address == slave_address) {
            return &g_al_context.slaves[i];
        }
    }

    return NULL;
}

al_slave_context_t* al_alloc_slave_context(uint16_t slave_address)
{
    if (!g_al_context.initialized) {
        return NULL;
    }

    /* Check if already exists */
    al_slave_context_t* existing = al_get_slave_context(slave_address);
    if (existing != NULL) {
        return existing;
    }

    /* Check if we have space */
    if (g_al_context.slave_count >= g_al_context.config.max_slaves) {
        return NULL;
    }

    /* Allocate new context */
    al_slave_context_t* slave = &g_al_context.slaves[g_al_context.slave_count];
    memset(slave, 0, sizeof(al_slave_context_t));

    slave->station_address = slave_address;
    slave->current_state = AL_STATE_INIT;
    slave->requested_state = AL_STATE_INIT;
    slave->status_code = AL_STATUS_CODE_NO_ERROR;
    slave->mbx_state = MBOX_STATE_IDLE;

    /* Initialize mailbox */
    al_status_t status = al_mailbox_init(slave);
    if (status != AL_STATUS_SUCCESS) {
        return NULL;
    }

    g_al_context.slave_count++;

    return slave;
}

void al_free_slave_context(uint16_t slave_address)
{
    if (!g_al_context.initialized) {
        return;
    }

    for (uint16_t i = 0; i < g_al_context.slave_count; i++) {
        if (g_al_context.slaves[i].station_address == slave_address) {
            al_slave_context_t* slave = &g_al_context.slaves[i];

            /* Shutdown mailbox */
            al_mailbox_shutdown(slave);

            /* Shift remaining slaves */
            for (uint16_t j = i; j < g_al_context.slave_count - 1; j++) {
                memcpy(&g_al_context.slaves[j], &g_al_context.slaves[j + 1], sizeof(al_slave_context_t));
            }

            g_al_context.slave_count--;
            return;
        }
    }
}

/* ========================================================================== */
/* State Control Functions                                                   */
/* ========================================================================== */

al_status_t al_request_state(uint16_t slave_address,
                              al_state_t requested_state,
                              uint32_t timeout_ms)
{
    if (!g_al_context.initialized) {
        return AL_STATUS_NOT_INITIALIZED;
    }

    /* Get or allocate slave context */
    al_slave_context_t* slave = al_get_slave_context(slave_address);
    if (slave == NULL) {
        slave = al_alloc_slave_context(slave_address);
        if (slave == NULL) {
            return AL_STATUS_NO_MEMORY;
        }
    }

    /* Check if transition is valid */
    if (!al_state_is_transition_valid(slave->current_state, requested_state)) {
        return AL_STATUS_INVALID_STATE;
    }

    /* Write AL Control register */
    al_control_t control = {0};
    control.state = requested_state;
    control.ack = 0;
    control.request_id = 0;

    al_status_t status = al_write_control(slave_address, &control);
    if (status != AL_STATUS_SUCCESS) {
        return status;
    }

    /* Update slave context */
    slave->requested_state = requested_state;
    slave->state_transition_pending = true;
    slave->state_transition_start_time = al_get_time_ms();

    /* Wait for state transition with timeout */
    uint64_t start_time = al_get_time_ms();
    while (slave->state_transition_pending) {
        /* Check timeout */
        if (al_is_timeout(start_time, timeout_ms)) {
            slave->state_transition_pending = false;
            return AL_STATUS_TIMEOUT;
        }

        /* Read AL Status register */
        al_status_reg_t status_reg;
        status = al_read_status(slave_address, &status_reg);
        if (status != AL_STATUS_SUCCESS) {
            slave->state_transition_pending = false;
            return status;
        }

        /* Check if state changed */
        if (status_reg.state == requested_state) {
            slave->current_state = requested_state;
            slave->state_transition_pending = false;

            /* Trigger state change callback */
            al_trigger_state_change(slave_address, slave->current_state,
                                    requested_state, AL_STATUS_CODE_NO_ERROR);

            g_al_context.state_transitions++;
            return AL_STATUS_SUCCESS;
        }

        /* Check for error */
        if (status_reg.error) {
            al_status_code_t error_code;
            al_get_status_code(slave_address, &error_code);
            slave->status_code = error_code;
            slave->state_transition_pending = false;

            al_trigger_error(slave_address, error_code, "State transition failed");
            return AL_STATUS_ERROR;
        }

        /* Small delay to avoid busy waiting */
        // In real implementation, use proper sleep/yield
    }

    return AL_STATUS_SUCCESS;
}

al_status_t al_get_state(uint16_t slave_address, al_state_t* state)
{
    if (!g_al_context.initialized) {
        return AL_STATUS_NOT_INITIALIZED;
    }

    if (state == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Read AL Status register */
    al_status_reg_t status_reg;
    al_status_t status = al_read_status(slave_address, &status_reg);
    if (status != AL_STATUS_SUCCESS) {
        return status;
    }

    *state = (al_state_t)status_reg.state;

    /* Update slave context if exists */
    al_slave_context_t* slave = al_get_slave_context(slave_address);
    if (slave != NULL) {
        slave->current_state = *state;
    }

    return AL_STATUS_SUCCESS;
}

al_status_t al_get_status_code(uint16_t slave_address,
                                al_status_code_t* status_code)
{
    if (!g_al_context.initialized) {
        return AL_STATUS_NOT_INITIALIZED;
    }

    if (status_code == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Read AL Status Code register */
    uint16_t code;
    al_status_t status = al_read_reg16(slave_address, AL_STATUS_CODE_REG_ADDR, &code);
    if (status != AL_STATUS_SUCCESS) {
        return status;
    }

    *status_code = (al_status_code_t)code;

    /* Update slave context if exists */
    al_slave_context_t* slave = al_get_slave_context(slave_address);
    if (slave != NULL) {
        slave->status_code = *status_code;
    }

    return AL_STATUS_SUCCESS;
}

al_status_t al_read_control(uint16_t slave_address, al_control_t* control)
{
    if (!g_al_context.initialized) {
        return AL_STATUS_NOT_INITIALIZED;
    }

    if (control == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    uint16_t value;
    al_status_t status = al_read_reg16(slave_address, AL_CONTROL_REG_ADDR, &value);
    if (status != AL_STATUS_SUCCESS) {
        return status;
    }

    memcpy(control, &value, sizeof(uint16_t));
    return AL_STATUS_SUCCESS;
}

al_status_t al_write_control(uint16_t slave_address, const al_control_t* control)
{
    if (!g_al_context.initialized) {
        return AL_STATUS_NOT_INITIALIZED;
    }

    if (control == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    uint16_t value;
    memcpy(&value, control, sizeof(uint16_t));

    return al_write_reg16(slave_address, AL_CONTROL_REG_ADDR, value);
}

al_status_t al_read_status(uint16_t slave_address, al_status_reg_t* status)
{
    if (!g_al_context.initialized) {
        return AL_STATUS_NOT_INITIALIZED;
    }

    if (status == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    uint16_t value;
    al_status_t result = al_read_reg16(slave_address, AL_STATUS_REG_ADDR, &value);
    if (result != AL_STATUS_SUCCESS) {
        return result;
    }

    memcpy(status, &value, sizeof(uint16_t));
    return AL_STATUS_SUCCESS;
}

/* ========================================================================== */
/* String Functions                                                          */
/* ========================================================================== */

const char* al_get_state_name(al_state_t state)
{
    switch (state) {
        case AL_STATE_INIT:   return "Init";
        case AL_STATE_PREOP:  return "Pre-Operational";
        case AL_STATE_BOOT:   return "Bootstrap";
        case AL_STATE_SAFEOP: return "Safe-Operational";
        case AL_STATE_OP:     return "Operational";
        default:              return "Unknown";
    }
}

const char* al_get_status_code_string(al_status_code_t status_code)
{
    switch (status_code) {
        case AL_STATUS_CODE_NO_ERROR:
            return "No error";
        case AL_STATUS_CODE_UNSPECIFIED:
            return "Unspecified error";
        case AL_STATUS_CODE_NO_MEMORY:
            return "No memory";
        case AL_STATUS_CODE_INVALID_DEVICE_SETUP:
            return "Invalid device setup";
        case AL_STATUS_CODE_INVALID_MAILBOX_CONFIG:
            return "Invalid mailbox configuration";
        case AL_STATUS_CODE_INVALID_SM_CONFIG:
            return "Invalid sync manager configuration";
        case AL_STATUS_CODE_SLAVE_NEEDS_COLD_START:
            return "Slave needs cold start";
        case AL_STATUS_CODE_SLAVE_NEEDS_INIT:
            return "Slave needs init";
        case AL_STATUS_CODE_SLAVE_NEEDS_PREOP:
            return "Slave needs pre-operational";
        case AL_STATUS_CODE_SLAVE_NEEDS_SAFEOP:
            return "Slave needs safe-operational";
        default:
            return "Unknown error";
    }
}

/* ========================================================================== */
/* Callback Registration                                                     */
/* ========================================================================== */

al_status_t al_register_callbacks(al_state_change_cb_t state_change_cb,
                                   al_mailbox_receive_cb_t mailbox_receive_cb,
                                   al_error_cb_t error_cb)
{
    if (!g_al_context.initialized) {
        return AL_STATUS_NOT_INITIALIZED;
    }

    g_al_context.state_change_cb = state_change_cb;
    g_al_context.mailbox_receive_cb = mailbox_receive_cb;
    g_al_context.error_cb = error_cb;

    return AL_STATUS_SUCCESS;
}

al_status_t al_unregister_callbacks(void)
{
    if (!g_al_context.initialized) {
        return AL_STATUS_NOT_INITIALIZED;
    }

    g_al_context.state_change_cb = NULL;
    g_al_context.mailbox_receive_cb = NULL;
    g_al_context.error_cb = NULL;

    return AL_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Utility Functions                                                         */
/* ========================================================================== */

const char* al_get_version(void)
{
    return "1.0.0";
}

uint64_t al_get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

bool al_is_timeout(uint64_t start_time, uint32_t timeout_ms)
{
    uint64_t current_time = al_get_time_ms();
    return (current_time - start_time) >= timeout_ms;
}

void al_trigger_error(uint16_t slave_address, al_status_code_t error_code, const char* context)
{
    if (g_al_context.error_cb != NULL) {
        g_al_context.error_cb(slave_address, error_code, context);
    }
    g_al_context.errors++;
}

void al_trigger_state_change(uint16_t slave_address, al_state_t old_state,
                              al_state_t new_state, al_status_code_t status_code)
{
    if (g_al_context.state_change_cb != NULL) {
        al_control_ind_t ind = {
            .slave_address = slave_address,
            .old_state = old_state,
            .new_state = new_state,
            .status_code = status_code
        };
        g_al_context.state_change_cb(&ind);
    }
}

/* ========================================================================== */
/* Port-Specific Functions (Redundancy Support)                              */
/* ========================================================================== */

al_status_t al_request_state_port(uint16_t slave_address,
                                   al_state_t requested_state,
                                   uint8_t port,
                                   uint32_t timeout_ms)
{
    if (!g_al_context.initialized) {
        return AL_STATUS_NOT_INITIALIZED;
    }

    if (port >= hal_get_port_count()) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Get slave context */
    al_slave_context_t* slave = al_get_slave_context(slave_address);
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Check if transition is valid */
    if (!al_state_is_transition_valid(slave->current_state, requested_state)) {
        return AL_STATUS_INVALID_STATE;
    }

    /* Build FPWR frame for AL Control register (0x0120) */
    uint8_t frame_buffer[ECAT_MAX_FRAME_SIZE];
    ecat_frame_builder_t builder;

    uint8_t src_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer,
                                                  sizeof(frame_buffer),
                                                  src_mac, dst_mac);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Add FPWR datagram for AL Control register */
    uint32_t address = ecat_addr_configured(slave_address, 0x0120);
    uint16_t control_value = (uint16_t)requested_state;

    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_FPWR, 0,
                                              address, (uint8_t*)&control_value,
                                              2, false);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Finalize frame */
    uint16_t frame_length;
    status = ecat_frame_builder_finalize(&builder, &frame_length);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Allocate TX buffer */
    hal_frame_buffer_t* tx_buffer = NULL;
    hal_status_t hal_status = hal_alloc_tx_buffer(frame_length, &tx_buffer);
    if (hal_status != HAL_STATUS_SUCCESS || tx_buffer == NULL) {
        return AL_STATUS_ERROR;
    }

    /* Copy frame data */
    memcpy(tx_buffer->data, frame_buffer, frame_length);
    tx_buffer->length = frame_length;

    /* Send on specific port */
    hal_status = hal_send_frame_port(tx_buffer, port);
    hal_free_tx_buffer(tx_buffer);

    if (hal_status != HAL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Update slave context */
    slave->requested_state = requested_state;
    slave->state_transition_pending = true;
    slave->state_transition_start_time = al_get_time_ms();

    /* Wait for state transition with timeout */
    uint64_t start_time = al_get_time_ms();
    while (!al_is_timeout(start_time, timeout_ms)) {
        /* Read AL Status register to check state */
        al_state_t current_state;
        al_status_t read_status = al_get_state_port(slave_address, port, &current_state);

        if (read_status == AL_STATUS_SUCCESS && current_state == requested_state) {
            slave->current_state = current_state;
            slave->state_transition_pending = false;
            return AL_STATUS_SUCCESS;
        }

        /* Small delay to avoid busy-waiting */
        hal_sleep_ms(10);
    }

    return AL_STATUS_TIMEOUT;
}

al_status_t al_get_state_port(uint16_t slave_address,
                               uint8_t port,
                               al_state_t* state)
{
    if (!g_al_context.initialized) {
        return AL_STATUS_NOT_INITIALIZED;
    }

    if (state == NULL || port >= hal_get_port_count()) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Build FPRD frame for AL Status register (0x0130) */
    uint8_t frame_buffer[ECAT_MAX_FRAME_SIZE];
    ecat_frame_builder_t builder;

    uint8_t src_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer,
                                                  sizeof(frame_buffer),
                                                  src_mac, dst_mac);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Add FPRD datagram for AL Status register */
    uint32_t address = ecat_addr_configured(slave_address, 0x0130);
    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_FPRD, 0,
                                              address, NULL, 2, false);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Finalize frame */
    uint16_t frame_length;
    status = ecat_frame_builder_finalize(&builder, &frame_length);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Allocate TX buffer */
    hal_frame_buffer_t* tx_buffer = NULL;
    hal_status_t hal_status = hal_alloc_tx_buffer(frame_length, &tx_buffer);
    if (hal_status != HAL_STATUS_SUCCESS || tx_buffer == NULL) {
        return AL_STATUS_ERROR;
    }

    /* Copy frame data */
    memcpy(tx_buffer->data, frame_buffer, frame_length);
    tx_buffer->length = frame_length;

    /* Send on specific port */
    hal_status = hal_send_frame_port(tx_buffer, port);
    hal_free_tx_buffer(tx_buffer);

    if (hal_status != HAL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Wait for response on specific port */
    uint64_t start_time = hal_get_time_ms();
    uint32_t timeout_ms = 100;

    while (1) {
        /* Check timeout */
        if (hal_get_time_ms() - start_time >= timeout_ms) {
            return AL_STATUS_TIMEOUT;
        }

        /* Receive from specific port */
        hal_frame_buffer_t* rx_buffer = NULL;
        hal_status = hal_receive_frame_port(&rx_buffer, port);

        if (hal_status == HAL_STATUS_SUCCESS && rx_buffer != NULL) {
            /* Parse frame */
            ecat_frame_parser_t parser;
            status = ecat_frame_parser_init(&parser, rx_buffer->data, rx_buffer->length);

            if (status == DL_STATUS_SUCCESS) {
                status = ecat_frame_parser_validate(&parser);

                if (status == DL_STATUS_SUCCESS && ecat_frame_parser_has_more(&parser)) {
                    ecat_parsed_datagram_t response;
                    status = ecat_frame_parser_next_datagram(&parser, &response);

                    if (status == DL_STATUS_SUCCESS && response.length >= 2) {
                        /* Extract state from AL Status register */
                        uint16_t status_value = *(uint16_t*)response.data;
                        *state = (al_state_t)(status_value & 0x0F);
                        hal_free_rx_buffer(rx_buffer);
                        return AL_STATUS_SUCCESS;
                    }
                }
            }

            hal_free_rx_buffer(rx_buffer);
        }

        /* Small delay */
        hal_sleep_us(100);
    }

    return AL_STATUS_TIMEOUT;
}

al_status_t al_mailbox_send_port(const mbx_send_req_t* req, uint8_t port)
{
    if (!g_al_context.initialized) {
        return AL_STATUS_NOT_INITIALIZED;
    }

    if (req == NULL || port >= hal_get_port_count()) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Get slave context */
    al_slave_context_t* slave = al_get_slave_context(req->slave_address);
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Build mailbox header */
    uint8_t mbx_header[6];
    mbx_header[0] = (uint8_t)(req->length & 0xFF);
    mbx_header[1] = (uint8_t)((req->length >> 8) & 0xFF);
    mbx_header[2] = 0x00;  /* Station address (low) */
    mbx_header[3] = 0x00;  /* Station address (high) */
    mbx_header[4] = 0x00;  /* Channel & Priority */
    mbx_header[5] = (uint8_t)req->type;

    /* Allocate buffer for mailbox data (header + data) */
    uint16_t total_length = 6 + req->length;
    uint8_t* mbx_data = (uint8_t*)malloc(total_length);
    if (mbx_data == NULL) {
        return AL_STATUS_NO_MEMORY;
    }

    memcpy(mbx_data, mbx_header, 6);
    memcpy(mbx_data + 6, req->data, req->length);

    /* Build FPWR frame for mailbox write SM */
    uint8_t frame_buffer[ECAT_MAX_FRAME_SIZE];
    ecat_frame_builder_t builder;

    uint8_t src_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer,
                                                  sizeof(frame_buffer),
                                                  src_mac, dst_mac);
    if (status != DL_STATUS_SUCCESS) {
        free(mbx_data);
        return AL_STATUS_ERROR;
    }

    /* Add FPWR datagram for mailbox write */
    uint32_t address = ecat_addr_configured(req->slave_address, slave->mbx_write_offset);
    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_FPWR, 0,
                                              address, mbx_data, total_length, false);
    free(mbx_data);

    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Finalize frame */
    uint16_t frame_length;
    status = ecat_frame_builder_finalize(&builder, &frame_length);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Allocate TX buffer */
    hal_frame_buffer_t* tx_buffer = NULL;
    hal_status_t hal_status = hal_alloc_tx_buffer(frame_length, &tx_buffer);
    if (hal_status != HAL_STATUS_SUCCESS || tx_buffer == NULL) {
        return AL_STATUS_ERROR;
    }

    /* Copy frame data */
    memcpy(tx_buffer->data, frame_buffer, frame_length);
    tx_buffer->length = frame_length;

    /* Send on specific port */
    hal_status = hal_send_frame_port(tx_buffer, port);
    hal_free_tx_buffer(tx_buffer);

    if (hal_status != HAL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    g_al_context.mailbox_messages_sent++;
    return AL_STATUS_SUCCESS;
}

al_status_t al_mailbox_receive_port(uint16_t slave_address,
                                     uint8_t port,
                                     mailbox_type_t* type,
                                     uint8_t* data,
                                     uint16_t* length)
{
    if (!g_al_context.initialized) {
        return AL_STATUS_NOT_INITIALIZED;
    }

    if (type == NULL || data == NULL || length == NULL || port >= hal_get_port_count()) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Get slave context */
    al_slave_context_t* slave = al_get_slave_context(slave_address);
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Build FPRD frame for mailbox read SM */
    uint8_t frame_buffer[ECAT_MAX_FRAME_SIZE];
    ecat_frame_builder_t builder;

    uint8_t src_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    dl_status_t status = ecat_frame_builder_init(&builder, frame_buffer,
                                                  sizeof(frame_buffer),
                                                  src_mac, dst_mac);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Add FPRD datagram for mailbox read */
    uint32_t address = ecat_addr_configured(slave_address, slave->mbx_read_offset);
    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_FPRD, 0,
                                              address, NULL, slave->mbx_read_size, false);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Finalize frame */
    uint16_t frame_length;
    status = ecat_frame_builder_finalize(&builder, &frame_length);
    if (status != DL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Allocate TX buffer */
    hal_frame_buffer_t* tx_buffer = NULL;
    hal_status_t hal_status = hal_alloc_tx_buffer(frame_length, &tx_buffer);
    if (hal_status != HAL_STATUS_SUCCESS || tx_buffer == NULL) {
        return AL_STATUS_ERROR;
    }

    /* Copy frame data */
    memcpy(tx_buffer->data, frame_buffer, frame_length);
    tx_buffer->length = frame_length;

    /* Send on specific port */
    hal_status = hal_send_frame_port(tx_buffer, port);
    hal_free_tx_buffer(tx_buffer);

    if (hal_status != HAL_STATUS_SUCCESS) {
        return AL_STATUS_ERROR;
    }

    /* Wait for response on specific port */
    uint64_t start_time = hal_get_time_ms();
    uint32_t timeout_ms = 100;

    while (1) {
        /* Check timeout */
        if (hal_get_time_ms() - start_time >= timeout_ms) {
            return AL_STATUS_TIMEOUT;
        }

        /* Receive from specific port */
        hal_frame_buffer_t* rx_buffer = NULL;
        hal_status = hal_receive_frame_port(&rx_buffer, port);

        if (hal_status == HAL_STATUS_SUCCESS && rx_buffer != NULL) {
            /* Parse frame */
            ecat_frame_parser_t parser;
            status = ecat_frame_parser_init(&parser, rx_buffer->data, rx_buffer->length);

            if (status == DL_STATUS_SUCCESS) {
                status = ecat_frame_parser_validate(&parser);

                if (status == DL_STATUS_SUCCESS && ecat_frame_parser_has_more(&parser)) {
                    ecat_parsed_datagram_t response;
                    status = ecat_frame_parser_next_datagram(&parser, &response);

                    if (status == DL_STATUS_SUCCESS && response.length >= 6) {
                        /* Parse mailbox header */
                        uint16_t mbx_length = response.data[0] | (response.data[1] << 8);
                        *type = (mailbox_type_t)response.data[5];

                        /* Copy mailbox data */
                        uint16_t copy_length = (mbx_length < *length) ? mbx_length : *length;
                        memcpy(data, response.data + 6, copy_length);
                        *length = copy_length;

                        hal_free_rx_buffer(rx_buffer);
                        g_al_context.mailbox_messages_received++;
                        return AL_STATUS_SUCCESS;
                    }
                }
            }

            hal_free_rx_buffer(rx_buffer);
        }

        /* Small delay */
        hal_sleep_us(100);
    }

    return AL_STATUS_TIMEOUT;
}
