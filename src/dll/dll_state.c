/**
 * @file dll_state.c
 * @brief EtherCAT Data Link Layer - State Machine Implementation
 * @version 1.0.0
 * @date 2026-01-03
 *
 * State Machine Transitions:
 * UNINITIALIZED -> INITIALIZED (via dl_init)
 * INITIALIZED -> READY (via dl_start)
 * READY -> RUNNING (via first frame sent)
 * RUNNING -> READY (via dl_stop)
 * READY -> INITIALIZED (via dl_stop)
 * INITIALIZED -> UNINITIALIZED (via dl_shutdown)
 * ANY_STATE -> ERROR (via error condition)
 * ERROR -> INITIALIZED (via dl_reset)
 */

#include "ethercat/dll_state.h"
#include "ethercat/dll_errors.h"
#include <string.h>

/* ========================================================================== */
/* Private Variables                                                          */
/* ========================================================================== */

/** Current DLL state */
static dl_state_t current_state = DL_STATE_UNINITIALIZED;

/** State change callback */
static dl_state_change_cb_t state_change_callback = NULL;

/* ========================================================================== */
/* State Name Table                                                           */
/* ========================================================================== */

static const char* state_names[] = {
    [DL_STATE_UNINITIALIZED] = "UNINITIALIZED",
    [DL_STATE_INITIALIZED] = "INITIALIZED",
    [DL_STATE_READY] = "READY",
    [DL_STATE_RUNNING] = "RUNNING",
    [DL_STATE_ERROR] = "ERROR"
};

#define STATE_NAME_COUNT (sizeof(state_names) / sizeof(state_names[0]))

/* ========================================================================== */
/* State Transition Table                                                     */
/* ========================================================================== */

/**
 * State transition validity table
 * [from_state][to_state] = true if transition is valid
 */
static const bool transition_table[5][5] = {
    /* To:  UNINIT  INIT   READY  RUN    ERROR */
    /* UNINITIALIZED */ { true,  true,  false, false, true  },
    /* INITIALIZED   */ { true,  true,  true,  false, true  },
    /* READY         */ { false, true,  true,  true,  true  },
    /* RUNNING       */ { false, false, true,  true,  true  },
    /* ERROR         */ { false, true,  false, false, true  }
};

/* ========================================================================== */
/* Private Functions                                                          */
/* ========================================================================== */

/**
 * @brief Invoke state change callback
 *
 * @param old_state Previous state
 * @param new_state New state
 */
static void invoke_state_change_callback(dl_state_t old_state, dl_state_t new_state)
{
    if (state_change_callback != NULL) {
        state_change_callback(old_state, new_state);
    }
}

/* ========================================================================== */
/* Public Functions                                                           */
/* ========================================================================== */

void dl_state_init(void)
{
    current_state = DL_STATE_UNINITIALIZED;
    state_change_callback = NULL;
}

dl_state_t dl_state_get(void)
{
    return current_state;
}

dl_status_t dl_state_set(dl_state_t new_state)
{
    /* Validate state value */
    if (new_state >= STATE_NAME_COUNT) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "dl_state_set: invalid state");
        return DL_STATUS_INVALID_PARAM;
    }

    /* Check if transition is valid */
    if (!dl_state_is_transition_valid(current_state, new_state)) {
        dl_set_error(DL_ERROR_INVALID_STATE, "dl_state_set: invalid transition");
        return DL_STATUS_ERROR;
    }

    /* Perform state transition */
    dl_state_t old_state = current_state;
    current_state = new_state;

    /* Invoke callback if state actually changed */
    if (old_state != new_state) {
        invoke_state_change_callback(old_state, new_state);
    }

    return DL_STATUS_SUCCESS;
}

bool dl_state_is_transition_valid(dl_state_t from_state, dl_state_t to_state)
{
    /* Validate state values */
    if (from_state >= STATE_NAME_COUNT || to_state >= STATE_NAME_COUNT) {
        return false;
    }

    /* Check transition table */
    return transition_table[from_state][to_state];
}

dl_status_t dl_state_register_callback(dl_state_change_cb_t callback)
{
    state_change_callback = callback;
    return DL_STATUS_SUCCESS;
}

const char* dl_state_get_name(dl_state_t state)
{
    if (state < STATE_NAME_COUNT && state_names[state] != NULL) {
        return state_names[state];
    }
    return "UNKNOWN";
}
