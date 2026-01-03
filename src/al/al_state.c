/**
 * @file al_state.c
 * @brief Application Layer State Machine Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "al_internal.h"
#include <string.h>

/* ========================================================================== */
/* State Transition Validation                                               */
/* ========================================================================== */

bool al_state_is_transition_valid(al_state_t current_state, al_state_t requested_state)
{
    /* Same state is always valid */
    if (current_state == requested_state) {
        return true;
    }

    switch (current_state) {
        case AL_STATE_INIT:
            /* Init can transition to Pre-Op */
            return (requested_state == AL_STATE_PREOP);

        case AL_STATE_PREOP:
            /* Pre-Op can transition to Init, Safe-Op, or Bootstrap */
            return (requested_state == AL_STATE_INIT ||
                    requested_state == AL_STATE_SAFEOP ||
                    requested_state == AL_STATE_BOOT);

        case AL_STATE_BOOT:
            /* Bootstrap can only transition to Init */
            return (requested_state == AL_STATE_INIT);

        case AL_STATE_SAFEOP:
            /* Safe-Op can transition to Pre-Op or Op */
            return (requested_state == AL_STATE_PREOP ||
                    requested_state == AL_STATE_OP);

        case AL_STATE_OP:
            /* Op can only transition to Safe-Op */
            return (requested_state == AL_STATE_SAFEOP);

        default:
            return false;
    }
}

/* ========================================================================== */
/* State Transition Execution                                                */
/* ========================================================================== */

al_status_t al_state_transition(al_slave_context_t* slave, al_state_t new_state)
{
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    al_state_t old_state = slave->current_state;

    /* Validate transition */
    if (!al_state_is_transition_valid(old_state, new_state)) {
        return AL_STATUS_INVALID_STATE;
    }

    /* Execute state-specific transition logic */
    al_status_t status = AL_STATUS_SUCCESS;

    switch (new_state) {
        case AL_STATE_INIT:
            status = al_state_enter_init(slave);
            break;

        case AL_STATE_PREOP:
            status = al_state_enter_preop(slave);
            break;

        case AL_STATE_BOOT:
            status = al_state_enter_boot(slave);
            break;

        case AL_STATE_SAFEOP:
            status = al_state_enter_safeop(slave);
            break;

        case AL_STATE_OP:
            status = al_state_enter_op(slave);
            break;

        default:
            return AL_STATUS_INVALID_STATE;
    }

    if (status == AL_STATUS_SUCCESS) {
        slave->current_state = new_state;
        slave->status_code = AL_STATUS_CODE_NO_ERROR;

        /* Trigger state change callback */
        al_trigger_state_change(slave->station_address, old_state, new_state,
                                AL_STATUS_CODE_NO_ERROR);
    }

    return status;
}

/* ========================================================================== */
/* State Entry Functions                                                     */
/* ========================================================================== */

al_status_t al_state_enter_init(al_slave_context_t* slave)
{
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* In Init state:
     * - No mailbox communication
     * - No process data communication
     * - Slave reads EEPROM configuration
     */

    /* Disable all sync managers */
    for (uint8_t i = 0; i < SM_MAX_COUNT; i++) {
        al_sm_disable(slave->station_address, i);
    }

    /* Reset mailbox state */
    slave->mbx_state = MBOX_STATE_IDLE;

    return AL_STATUS_SUCCESS;
}

al_status_t al_state_enter_preop(al_slave_context_t* slave)
{
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* In Pre-Op state:
     * - Mailbox communication enabled
     * - Process data communication disabled
     * - Configuration via mailbox (CoE)
     */

    /* Configure mailbox sync managers */
    al_status_t status;

    /* SM0: Mailbox Write (Master -> Slave) */
    if (slave->mbx_write_size > 0) {
        sm_config_t sm0_config = {
            .physical_start_address = slave->mbx_write_offset,
            .length = slave->mbx_write_size,
            .control = (SM_OP_MODE_1BUFFER << 0) | (1 << 2), /* 1-buffer, write */
            .status = 0,
            .enable = 1,
            .pdi_control = 0
        };

        status = al_sm_config(slave->station_address, 0, &sm0_config);
        if (status != AL_STATUS_SUCCESS) {
            return status;
        }

        status = al_sm_enable(slave->station_address, 0);
        if (status != AL_STATUS_SUCCESS) {
            return status;
        }
    }

    /* SM1: Mailbox Read (Slave -> Master) */
    if (slave->mbx_read_size > 0) {
        sm_config_t sm1_config = {
            .physical_start_address = slave->mbx_read_offset,
            .length = slave->mbx_read_size,
            .control = (SM_OP_MODE_1BUFFER << 0) | (0 << 2), /* 1-buffer, read */
            .status = 0,
            .enable = 1,
            .pdi_control = 0
        };

        status = al_sm_config(slave->station_address, 1, &sm1_config);
        if (status != AL_STATUS_SUCCESS) {
            return status;
        }

        status = al_sm_enable(slave->station_address, 1);
        if (status != AL_STATUS_SUCCESS) {
            return status;
        }
    }

    /* Reset mailbox state */
    slave->mbx_state = MBOX_STATE_IDLE;

    return AL_STATUS_SUCCESS;
}

al_status_t al_state_enter_boot(al_slave_context_t* slave)
{
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* In Bootstrap state:
     * - Only FoE mailbox protocol enabled
     * - Used for firmware updates
     */

    /* Configure mailbox sync managers (same as Pre-Op) */
    al_status_t status = al_state_enter_preop(slave);
    if (status != AL_STATUS_SUCCESS) {
        return status;
    }

    /* In bootstrap mode, only FoE is allowed */
    slave->supports_coe = false;
    slave->supports_foe = true;
    slave->supports_soe = false;
    slave->supports_voe = false;
    slave->supports_eoe = false;
    slave->supports_aoe = false;

    return AL_STATUS_SUCCESS;
}

al_status_t al_state_enter_safeop(al_slave_context_t* slave)
{
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* In Safe-Op state:
     * - Mailbox communication enabled
     * - Process data communication enabled (inputs only)
     * - Outputs are in safe state (typically zero)
     */

    /* Mailbox sync managers should already be configured from Pre-Op */

    /* Configure process data sync managers */
    /* SM2: Process Data Outputs (Master -> Slave) - disabled in Safe-Op */
    /* SM3: Process Data Inputs (Slave -> Master) - enabled in Safe-Op */

    /* Note: Actual SM configuration depends on slave's EEPROM configuration
     * This is a simplified implementation */

    return AL_STATUS_SUCCESS;
}

al_status_t al_state_enter_op(al_slave_context_t* slave)
{
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* In Op state:
     * - Full operation mode
     * - Mailbox and process data communication enabled
     * - Outputs are active
     */

    /* Enable process data outputs (SM2) */
    /* Note: Actual SM configuration depends on slave's EEPROM configuration
     * This is a simplified implementation */

    return AL_STATUS_SUCCESS;
}

/* ========================================================================== */
/* State Machine Processing                                                  */
/* ========================================================================== */

al_status_t al_state_process(al_slave_context_t* slave)
{
    if (slave == NULL) {
        return AL_STATUS_INVALID_PARAM;
    }

    /* Check if state transition is pending */
    if (slave->state_transition_pending) {
        /* Check for timeout */
        if (al_is_timeout(slave->state_transition_start_time,
                          AL_STATE_TRANSITION_TIMEOUT_MS)) {
            slave->state_transition_pending = false;
            al_trigger_error(slave->station_address, AL_STATUS_CODE_UNSPECIFIED,
                             "State transition timeout");
            return AL_STATUS_TIMEOUT;
        }

        /* Read AL Status register */
        al_status_reg_t status_reg;
        al_status_t status = al_read_status(slave->station_address, &status_reg);
        if (status != AL_STATUS_SUCCESS) {
            return status;
        }

        /* Check if state changed */
        if (status_reg.state == slave->requested_state) {
            slave->current_state = slave->requested_state;
            slave->state_transition_pending = false;

            /* Trigger state change callback */
            al_trigger_state_change(slave->station_address, slave->current_state,
                                    slave->requested_state, AL_STATUS_CODE_NO_ERROR);

            return AL_STATUS_SUCCESS;
        }

        /* Check for error */
        if (status_reg.error) {
            al_status_code_t error_code;
            al_get_status_code(slave->station_address, &error_code);
            slave->status_code = error_code;
            slave->state_transition_pending = false;

            al_trigger_error(slave->station_address, error_code,
                             "State transition failed");
            return AL_STATUS_ERROR;
        }
    }

    /* Process mailbox if in Pre-Op, Safe-Op, or Op state */
    if (slave->current_state == AL_STATE_PREOP ||
        slave->current_state == AL_STATE_SAFEOP ||
        slave->current_state == AL_STATE_OP ||
        slave->current_state == AL_STATE_BOOT) {
        al_mailbox_process(slave);
    }

    return AL_STATUS_SUCCESS;
}
