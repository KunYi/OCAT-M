/**
 * @file dll_errors.h
 * @brief EtherCAT Data Link Layer - Error Handling
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Based on ETG1000.3 - EtherCAT Data Link Layer Services
 *
 * This file contains error handling definitions and functions for the DLL layer.
 */

#ifndef ETHERCAT_DLL_ERRORS_H
#define ETHERCAT_DLL_ERRORS_H

#include "dll_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup DLL_Errors Data Link Layer Error Handling
 * @{
 */

/**
 * @brief Get last DLL error code
 *
 * Returns the last error code that occurred in the DLL layer.
 * This is thread-local if compiled with thread support.
 *
 * @return Last error code
 */
dl_error_t dl_get_last_error(void);

/**
 * @brief Get error description string
 *
 * Returns a human-readable description of the error code.
 *
 * @param error Error code
 * @return Pointer to error description string (static, do not free)
 */
const char* dl_get_error_string(dl_error_t error);

/**
 * @brief Register error callback
 *
 * Registers a callback function to be called when errors occur.
 * Only one callback can be registered at a time.
 *
 * @param callback Callback function pointer (NULL to unregister)
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dl_register_error_callback(dl_error_cb_t callback);

/**
 * @brief Set last error code (internal use)
 *
 * Sets the last error code and optionally invokes the error callback.
 * This function is for internal DLL use only.
 *
 * @param error Error code to set
 * @param context Context string describing where the error occurred
 */
void dl_set_error(dl_error_t error, const char* context);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_DLL_ERRORS_H */
