/**
 * @file al.c
 * @brief Application Layer Core Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "al_internal.h"
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
