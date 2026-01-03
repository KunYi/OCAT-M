/**
 * @file dll_error.c
 * @brief EtherCAT Data Link Layer - Error Handling Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "ethercat/dll_errors.h"
#include <string.h>

/* ========================================================================== */
/* Private Variables                                                          */
/* ========================================================================== */

/** Last error code (thread-local if supported) */
#ifdef __STDC_NO_THREADS__
static dl_error_t last_error = DL_ERROR_NONE;
#else
#include <threads.h>
static thread_local dl_error_t last_error = DL_ERROR_NONE;
#endif

/** Error callback function pointer */
static dl_error_cb_t error_callback = NULL;

/* ========================================================================== */
/* Error String Table                                                         */
/* ========================================================================== */

static const char* error_strings[] = {
    [DL_ERROR_NONE] = "No error",
    [DL_ERROR_INIT_FAILED] = "Initialization failed",
    [DL_ERROR_INVALID_STATE] = "Invalid state for operation",
    [DL_ERROR_QUEUE_FULL] = "Queue is full",
    [DL_ERROR_QUEUE_EMPTY] = "Queue is empty",
    [DL_ERROR_INVALID_FRAME] = "Invalid frame format",
    [DL_ERROR_TX_TIMEOUT] = "Transmission timeout",
    [DL_ERROR_RX_TIMEOUT] = "Reception timeout",
    [DL_ERROR_HARDWARE] = "Hardware error",
    [DL_ERROR_NO_MEMORY] = "Memory allocation failed",
    [DL_ERROR_INVALID_PARAM] = "Invalid parameter"
};

#define ERROR_STRING_COUNT (sizeof(error_strings) / sizeof(error_strings[0]))

/* ========================================================================== */
/* Public Functions                                                           */
/* ========================================================================== */

dl_error_t dl_get_last_error(void)
{
    return last_error;
}

const char* dl_get_error_string(dl_error_t error)
{
    if (error < ERROR_STRING_COUNT && error_strings[error] != NULL) {
        return error_strings[error];
    }
    return "Unknown error";
}

dl_status_t dl_register_error_callback(dl_error_cb_t callback)
{
    error_callback = callback;
    return DL_STATUS_SUCCESS;
}

void dl_set_error(dl_error_t error, const char* context)
{
    last_error = error;

    /* Invoke error callback if registered */
    if (error_callback != NULL && error != DL_ERROR_NONE) {
        error_callback(error, context);
    }
}
