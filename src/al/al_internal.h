/**
 * @file al_internal.h
 * @brief Application Layer Internal Definitions
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Internal structures and definitions for AL implementation.
 * This file is not part of the public API.
 */

#ifndef ETHERCAT_AL_INTERNAL_H
#define ETHERCAT_AL_INTERNAL_H

#include "ethercat/al.h"
#include "ethercat/dll.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Slave Context Structure                                                   */
/* ========================================================================== */

/**
 * @brief Slave device context
 */
typedef struct {
    uint16_t station_address;           /**< Configured station address */
    al_state_t current_state;           /**< Current AL state */
    al_state_t requested_state;         /**< Requested AL state */
    al_status_code_t status_code;       /**< Last status code */

    /* Mailbox configuration */
    uint16_t mbx_write_offset;          /**< Mailbox write SM offset */
    uint16_t mbx_write_size;            /**< Mailbox write SM size */
    uint16_t mbx_read_offset;           /**< Mailbox read SM offset */
    uint16_t mbx_read_size;             /**< Mailbox read SM size */
    mailbox_state_t mbx_state;          /**< Mailbox state */

    /* Sync Manager configuration */
    sm_config_t sm_config[SM_MAX_COUNT]; /**< SM configurations */

    /* State transition tracking */
    uint64_t state_transition_start_time; /**< State transition start time */
    bool state_transition_pending;      /**< State transition in progress */

    /* Mailbox buffers */
    uint8_t* mbx_send_buffer;           /**< Mailbox send buffer */
    uint8_t* mbx_recv_buffer;           /**< Mailbox receive buffer */
    uint16_t mbx_send_length;           /**< Mailbox send data length */
    uint16_t mbx_recv_length;           /**< Mailbox receive data length */

    /* Protocol support flags */
    bool supports_coe;                  /**< CoE support */
    bool supports_foe;                  /**< FoE support */
    bool supports_soe;                  /**< SoE support */
    bool supports_voe;                  /**< VoE support */
    bool supports_eoe;                  /**< EoE support */
    bool supports_aoe;                  /**< AoE support */
} al_slave_context_t;

/* ========================================================================== */
/* AL Context Structure                                                      */
/* ========================================================================== */

/**
 * @brief Application Layer context
 */
typedef struct {
    bool initialized;                   /**< Initialization flag */
    al_config_t config;                 /**< AL configuration */

    /* Callbacks */
    al_state_change_cb_t state_change_cb;
    al_mailbox_receive_cb_t mailbox_receive_cb;
    al_error_cb_t error_cb;

    /* Slave contexts */
    al_slave_context_t* slaves;         /**< Array of slave contexts */
    uint16_t slave_count;               /**< Number of slaves */

    /* Statistics */
    uint64_t state_transitions;         /**< Total state transitions */
    uint64_t mailbox_messages_sent;     /**< Total mailbox messages sent */
    uint64_t mailbox_messages_received; /**< Total mailbox messages received */
    uint64_t errors;                    /**< Total errors */
} al_context_t;

/* ========================================================================== */
/* Internal Function Declarations                                            */
/* ========================================================================== */

/**
 * @brief Get slave context by station address
 *
 * @param slave_address Slave station address
 * @return Pointer to slave context, NULL if not found
 */
al_slave_context_t* al_get_slave_context(uint16_t slave_address);

/**
 * @brief Allocate slave context
 *
 * @param slave_address Slave station address
 * @return Pointer to allocated slave context, NULL on error
 */
al_slave_context_t* al_alloc_slave_context(uint16_t slave_address);

/**
 * @brief Free slave context
 *
 * @param slave_address Slave station address
 */
void al_free_slave_context(uint16_t slave_address);

/* ========================================================================== */
/* State Machine Functions                                                   */
/* ========================================================================== */

/**
 * @brief Process state machine for a slave
 *
 * @param slave Pointer to slave context
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_state_process(al_slave_context_t* slave);

/**
 * @brief Check if state transition is valid
 *
 * @param current_state Current state
 * @param requested_state Requested state
 * @return true if transition is valid, false otherwise
 */
bool al_state_is_transition_valid(al_state_t current_state, al_state_t requested_state);

/**
 * @brief Execute state transition
 *
 * @param slave Pointer to slave context
 * @param new_state New state to transition to
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_state_transition(al_slave_context_t* slave, al_state_t new_state);

/**
 * @brief Enter Init state
 *
 * @param slave Pointer to slave context
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_state_enter_init(al_slave_context_t* slave);

/**
 * @brief Enter Pre-Operational state
 *
 * @param slave Pointer to slave context
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_state_enter_preop(al_slave_context_t* slave);

/**
 * @brief Enter Bootstrap state
 *
 * @param slave Pointer to slave context
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_state_enter_boot(al_slave_context_t* slave);

/**
 * @brief Enter Safe-Operational state
 *
 * @param slave Pointer to slave context
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_state_enter_safeop(al_slave_context_t* slave);

/**
 * @brief Enter Operational state
 *
 * @param slave Pointer to slave context
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_state_enter_op(al_slave_context_t* slave);

/* ========================================================================== */
/* Mailbox Functions                                                         */
/* ========================================================================== */

/**
 * @brief Initialize mailbox for a slave
 *
 * @param slave Pointer to slave context
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_mailbox_init(al_slave_context_t* slave);

/**
 * @brief Shutdown mailbox for a slave
 *
 * @param slave Pointer to slave context
 */
void al_mailbox_shutdown(al_slave_context_t* slave);

/**
 * @brief Process mailbox state machine
 *
 * @param slave Pointer to slave context
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_mailbox_process(al_slave_context_t* slave);

/**
 * @brief Write mailbox data to slave
 *
 * @param slave Pointer to slave context
 * @param data Data to write
 * @param length Data length
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_mailbox_write(al_slave_context_t* slave, const uint8_t* data, uint16_t length);

/**
 * @brief Read mailbox data from slave
 *
 * @param slave Pointer to slave context
 * @param data Buffer for read data
 * @param length Pointer to buffer length (in: max, out: actual)
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_mailbox_read(al_slave_context_t* slave, uint8_t* data, uint16_t* length);

/* ========================================================================== */
/* Register Access Functions                                                 */
/* ========================================================================== */

/**
 * @brief Read 8-bit register from slave
 *
 * @param slave_address Slave station address
 * @param offset Register offset
 * @param value Pointer to receive value
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_read_reg8(uint16_t slave_address, uint16_t offset, uint8_t* value);

/**
 * @brief Write 8-bit register to slave
 *
 * @param slave_address Slave station address
 * @param offset Register offset
 * @param value Value to write
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_write_reg8(uint16_t slave_address, uint16_t offset, uint8_t value);

/**
 * @brief Read 16-bit register from slave
 *
 * @param slave_address Slave station address
 * @param offset Register offset
 * @param value Pointer to receive value
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_read_reg16(uint16_t slave_address, uint16_t offset, uint16_t* value);

/**
 * @brief Write 16-bit register to slave
 *
 * @param slave_address Slave station address
 * @param offset Register offset
 * @param value Value to write
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_write_reg16(uint16_t slave_address, uint16_t offset, uint16_t value);

/**
 * @brief Read 32-bit register from slave
 *
 * @param slave_address Slave station address
 * @param offset Register offset
 * @param value Pointer to receive value
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_read_reg32(uint16_t slave_address, uint16_t offset, uint32_t* value);

/**
 * @brief Write 32-bit register to slave
 *
 * @param slave_address Slave station address
 * @param offset Register offset
 * @param value Value to write
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_write_reg32(uint16_t slave_address, uint16_t offset, uint32_t value);

/**
 * @brief Read block of data from slave
 *
 * @param slave_address Slave station address
 * @param offset Register offset
 * @param data Buffer for read data
 * @param length Number of bytes to read
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_read_block(uint16_t slave_address, uint16_t offset, uint8_t* data, uint16_t length);

/**
 * @brief Write block of data to slave
 *
 * @param slave_address Slave station address
 * @param offset Register offset
 * @param data Data to write
 * @param length Number of bytes to write
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_write_block(uint16_t slave_address, uint16_t offset, const uint8_t* data, uint16_t length);

/* ========================================================================== */
/* Utility Functions                                                         */
/* ========================================================================== */

/**
 * @brief Get current time in milliseconds
 *
 * @return Current time in milliseconds
 */
uint64_t al_get_time_ms(void);

/**
 * @brief Check if timeout has occurred
 *
 * @param start_time Start time in milliseconds
 * @param timeout_ms Timeout in milliseconds
 * @return true if timeout occurred, false otherwise
 */
bool al_is_timeout(uint64_t start_time, uint32_t timeout_ms);

/**
 * @brief Trigger error callback
 *
 * @param slave_address Slave station address
 * @param error_code Error code
 * @param context Error context string
 */
void al_trigger_error(uint16_t slave_address, al_status_code_t error_code, const char* context);

/**
 * @brief Trigger state change callback
 *
 * @param slave_address Slave station address
 * @param old_state Old state
 * @param new_state New state
 * @param status_code Status code
 */
void al_trigger_state_change(uint16_t slave_address, al_state_t old_state,
                              al_state_t new_state, al_status_code_t status_code);

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_AL_INTERNAL_H */
