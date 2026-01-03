/**
 * @file al.h
 * @brief EtherCAT Application Layer - Public API
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Based on ETG1000.5 - EtherCAT Application Layer Services
 *
 * This file contains the public API for the Application Layer including
 * initialization, state control, mailbox communication, and sync manager
 * configuration.
 */

#ifndef ETHERCAT_AL_H
#define ETHERCAT_AL_H

#include "al_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup AL_API Application Layer API
 * @{
 */

/* ========================================================================== */
/* Initialization and Configuration                                          */
/* ========================================================================== */

/**
 * @brief Initialize Application Layer
 *
 * @param config Pointer to AL configuration
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_init(const al_config_t* config);

/**
 * @brief Shutdown Application Layer
 *
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_shutdown(void);

/**
 * @brief Check if Application Layer is initialized
 *
 * @return true if initialized, false otherwise
 */
bool al_is_initialized(void);

/* ========================================================================== */
/* State Control Functions                                                   */
/* ========================================================================== */

/**
 * @brief Request AL state change for a slave
 *
 * @param slave_address Slave station address
 * @param requested_state Requested AL state
 * @param timeout_ms Timeout in milliseconds
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_request_state(uint16_t slave_address,
                              al_state_t requested_state,
                              uint32_t timeout_ms);

/**
 * @brief Get current AL state of a slave
 *
 * @param slave_address Slave station address
 * @param state Pointer to receive current state
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_get_state(uint16_t slave_address, al_state_t* state);

/**
 * @brief Get AL status code of a slave
 *
 * @param slave_address Slave station address
 * @param status_code Pointer to receive status code
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_get_status_code(uint16_t slave_address,
                                al_status_code_t* status_code);

/**
 * @brief Read AL Control register
 *
 * @param slave_address Slave station address
 * @param control Pointer to receive control register value
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_read_control(uint16_t slave_address, al_control_t* control);

/**
 * @brief Write AL Control register
 *
 * @param slave_address Slave station address
 * @param control Pointer to control register value
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_write_control(uint16_t slave_address, const al_control_t* control);

/**
 * @brief Read AL Status register
 *
 * @param slave_address Slave station address
 * @param status Pointer to receive status register value
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_read_status(uint16_t slave_address, al_status_reg_t* status);

/**
 * @brief Get AL state name string
 *
 * @param state AL state
 * @return Pointer to state name string
 */
const char* al_get_state_name(al_state_t state);

/**
 * @brief Get AL status code description string
 *
 * @param status_code AL status code
 * @return Pointer to status code description string
 */
const char* al_get_status_code_string(al_status_code_t status_code);

/* ========================================================================== */
/* Mailbox Functions                                                         */
/* ========================================================================== */

/**
 * @brief Send mailbox message to slave
 *
 * @param req Pointer to mailbox send request
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_mailbox_send(const mbx_send_req_t* req);

/**
 * @brief Check if mailbox message is available
 *
 * @param slave_address Slave station address
 * @param available Pointer to receive availability flag
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_mailbox_check(uint16_t slave_address, bool* available);

/**
 * @brief Receive mailbox message from slave
 *
 * @param slave_address Slave station address
 * @param type Pointer to receive mailbox type
 * @param data Buffer for received data
 * @param length Pointer to buffer length (in: max, out: actual)
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_mailbox_receive(uint16_t slave_address,
                                mailbox_type_t* type,
                                uint8_t* data,
                                uint16_t* length);

/**
 * @brief Get mailbox protocol name string
 *
 * @param type Mailbox protocol type
 * @return Pointer to protocol name string
 */
const char* al_mailbox_get_protocol_name(mailbox_type_t type);

/**
 * @brief Check if mailbox protocol is supported by slave
 *
 * @param slave_address Slave station address
 * @param type Mailbox protocol type
 * @param supported Pointer to receive support flag
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_mailbox_check_protocol_support(uint16_t slave_address,
                                               mailbox_type_t type,
                                               bool* supported);

/* ========================================================================== */
/* Sync Manager Functions                                                    */
/* ========================================================================== */

/**
 * @brief Configure Sync Manager
 *
 * @param slave_address Slave station address
 * @param sm_index Sync Manager index (0-15)
 * @param config Pointer to SM configuration
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_sm_config(uint16_t slave_address,
                          uint8_t sm_index,
                          const sm_config_t* config);

/**
 * @brief Read Sync Manager configuration
 *
 * @param slave_address Slave station address
 * @param sm_index Sync Manager index (0-15)
 * @param config Pointer to receive SM configuration
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_sm_read_config(uint16_t slave_address,
                               uint8_t sm_index,
                               sm_config_t* config);

/**
 * @brief Enable Sync Manager
 *
 * @param slave_address Slave station address
 * @param sm_index Sync Manager index (0-15)
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_sm_enable(uint16_t slave_address, uint8_t sm_index);

/**
 * @brief Disable Sync Manager
 *
 * @param slave_address Slave station address
 * @param sm_index Sync Manager index (0-15)
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_sm_disable(uint16_t slave_address, uint8_t sm_index);

/* ========================================================================== */
/* Slave Information Interface (SII)                                         */
/* ========================================================================== */

/**
 * @brief Read SII (EEPROM) data
 *
 * @param slave_address Slave station address
 * @param offset EEPROM word offset
 * @param data Buffer for read data
 * @param length Number of words to read
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_sii_read(uint16_t slave_address,
                         uint16_t offset,
                         uint16_t* data,
                         uint16_t length);

/**
 * @brief Write SII (EEPROM) data
 *
 * @param slave_address Slave station address
 * @param offset EEPROM word offset
 * @param data Data to write
 * @param length Number of words to write
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_sii_write(uint16_t slave_address,
                          uint16_t offset,
                          const uint16_t* data,
                          uint16_t length);

/* ========================================================================== */
/* Callback Registration                                                     */
/* ========================================================================== */

/**
 * @brief Register AL callbacks
 *
 * @param state_change_cb State change callback (can be NULL)
 * @param mailbox_receive_cb Mailbox receive callback (can be NULL)
 * @param error_cb Error callback (can be NULL)
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_register_callbacks(al_state_change_cb_t state_change_cb,
                                   al_mailbox_receive_cb_t mailbox_receive_cb,
                                   al_error_cb_t error_cb);

/**
 * @brief Unregister all AL callbacks
 *
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_unregister_callbacks(void);

/* ========================================================================== */
/* Port-Specific Functions (Redundancy Support)                              */
/* ========================================================================== */

/**
 * @brief Request AL state change for a slave on specific port
 *
 * @param slave_address Slave station address
 * @param requested_state Requested AL state
 * @param port Port to use (0 = primary, 1 = secondary)
 * @param timeout_ms Timeout in milliseconds
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_request_state_port(uint16_t slave_address,
                                   al_state_t requested_state,
                                   uint8_t port,
                                   uint32_t timeout_ms);

/**
 * @brief Get current AL state of a slave via specific port
 *
 * @param slave_address Slave station address
 * @param port Port to use (0 = primary, 1 = secondary)
 * @param state Pointer to receive current state
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_get_state_port(uint16_t slave_address,
                               uint8_t port,
                               al_state_t* state);

/**
 * @brief Send mailbox message to slave via specific port
 *
 * @param req Pointer to mailbox send request
 * @param port Port to use (0 = primary, 1 = secondary)
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_mailbox_send_port(const mbx_send_req_t* req, uint8_t port);

/**
 * @brief Receive mailbox message from slave via specific port
 *
 * @param slave_address Slave station address
 * @param port Port to use (0 = primary, 1 = secondary)
 * @param type Pointer to receive mailbox type
 * @param data Buffer for received data
 * @param length Pointer to buffer length (in: max, out: actual)
 * @return AL_STATUS_SUCCESS on success, error code otherwise
 */
al_status_t al_mailbox_receive_port(uint16_t slave_address,
                                     uint8_t port,
                                     mailbox_type_t* type,
                                     uint8_t* data,
                                     uint16_t* length);

/* ========================================================================== */
/* Utility Functions                                                         */
/* ========================================================================== */

/**
 * @brief Get AL version string
 *
 * @return Pointer to version string
 */
const char* al_get_version(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_AL_H */
