/**
 * @file dll_state.h
 * @brief EtherCAT Data Link Layer - State Machine Interface
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Based on ETG1000.3 - EtherCAT Data Link Layer Services
 *
 * This file contains the state machine interface for the DLL layer.
 */

#ifndef ETHERCAT_DLL_STATE_H
#define ETHERCAT_DLL_STATE_H

#include "dll_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup DLL_State Data Link Layer State Machine
 * @{
 */

/**
 * @brief State change callback function type
 *
 * Called when DLL state changes
 *
 * @param old_state Previous state
 * @param new_state New state
 */
typedef void (*dl_state_change_cb_t)(dl_state_t old_state, dl_state_t new_state);

/**
 * @brief Initialize state machine
 *
 * Initializes the state machine to UNINITIALIZED state.
 * This is called internally by the DLL.
 */
void dl_state_init(void);

/**
 * @brief Get current state
 *
 * Returns the current state of the DLL state machine.
 *
 * @return Current state
 */
dl_state_t dl_state_get(void);

/**
 * @brief Set new state
 *
 * Attempts to transition to a new state. Validates the transition
 * and invokes state change callback if registered.
 *
 * @param new_state Target state
 * @return DL_STATUS_SUCCESS if transition is valid, error code otherwise
 */
dl_status_t dl_state_set(dl_state_t new_state);

/**
 * @brief Validate state transition
 *
 * Checks if a state transition is valid according to the state machine.
 *
 * @param from_state Source state
 * @param to_state Target state
 * @return true if transition is valid, false otherwise
 */
bool dl_state_is_transition_valid(dl_state_t from_state, dl_state_t to_state);

/**
 * @brief Register state change callback
 *
 * Registers a callback to be invoked when state changes.
 *
 * @param callback Callback function pointer (NULL to unregister)
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dl_state_register_callback(dl_state_change_cb_t callback);

/**
 * @brief Get state name string
 *
 * Returns a human-readable name for the state.
 *
 * @param state State to get name for
 * @return Pointer to state name string (static, do not free)
 */
const char* dl_state_get_name(dl_state_t state);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_DLL_STATE_H */
