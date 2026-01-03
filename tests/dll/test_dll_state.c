/**
 * @file test_dll_state.c
 * @brief Unit tests for DLL state machine
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "ethercat/dll_state.h"
#include "ethercat/dll_errors.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* State change callback test variables */
static bool callback_invoked = false;
static dl_state_t callback_old_state;
static dl_state_t callback_new_state;

/* ========================================================================== */
/* Test Helpers                                                               */
/* ========================================================================== */

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("  [PASS] %s\n", message); \
        } else { \
            tests_failed++; \
            printf("  [FAIL] %s\n", message); \
        } \
    } while(0)

#define TEST_START(name) \
    printf("\n=== Test: %s ===\n", name)

/* State change callback for testing */
static void test_state_change_callback(dl_state_t old_state, dl_state_t new_state)
{
    callback_invoked = true;
    callback_old_state = old_state;
    callback_new_state = new_state;
}

/* Reset callback test variables */
static void reset_callback_vars(void)
{
    callback_invoked = false;
    callback_old_state = DL_STATE_UNINITIALIZED;
    callback_new_state = DL_STATE_UNINITIALIZED;
}

/* ========================================================================== */
/* Test Cases                                                                 */
/* ========================================================================== */

void test_state_init(void)
{
    TEST_START("State Initialization");

    dl_state_init();
    dl_state_t state = dl_state_get();

    TEST_ASSERT(state == DL_STATE_UNINITIALIZED,
                "Initial state should be UNINITIALIZED");
}

void test_state_get_name(void)
{
    TEST_START("State Name Retrieval");

    const char* name;

    name = dl_state_get_name(DL_STATE_UNINITIALIZED);
    TEST_ASSERT(strcmp(name, "UNINITIALIZED") == 0,
                "UNINITIALIZED state name");

    name = dl_state_get_name(DL_STATE_INITIALIZED);
    TEST_ASSERT(strcmp(name, "INITIALIZED") == 0,
                "INITIALIZED state name");

    name = dl_state_get_name(DL_STATE_READY);
    TEST_ASSERT(strcmp(name, "READY") == 0,
                "READY state name");

    name = dl_state_get_name(DL_STATE_RUNNING);
    TEST_ASSERT(strcmp(name, "RUNNING") == 0,
                "RUNNING state name");

    name = dl_state_get_name(DL_STATE_ERROR);
    TEST_ASSERT(strcmp(name, "ERROR") == 0,
                "ERROR state name");

    name = dl_state_get_name(99);
    TEST_ASSERT(strcmp(name, "UNKNOWN") == 0,
                "Invalid state returns UNKNOWN");
}

void test_valid_transitions(void)
{
    TEST_START("Valid State Transitions");

    dl_state_init();
    dl_status_t status;

    /* UNINITIALIZED -> INITIALIZED */
    status = dl_state_set(DL_STATE_INITIALIZED);
    TEST_ASSERT(status == DL_STATUS_SUCCESS &&
                dl_state_get() == DL_STATE_INITIALIZED,
                "UNINITIALIZED -> INITIALIZED");

    /* INITIALIZED -> READY */
    status = dl_state_set(DL_STATE_READY);
    TEST_ASSERT(status == DL_STATUS_SUCCESS &&
                dl_state_get() == DL_STATE_READY,
                "INITIALIZED -> READY");

    /* READY -> RUNNING */
    status = dl_state_set(DL_STATE_RUNNING);
    TEST_ASSERT(status == DL_STATUS_SUCCESS &&
                dl_state_get() == DL_STATE_RUNNING,
                "READY -> RUNNING");

    /* RUNNING -> READY */
    status = dl_state_set(DL_STATE_READY);
    TEST_ASSERT(status == DL_STATUS_SUCCESS &&
                dl_state_get() == DL_STATE_READY,
                "RUNNING -> READY");

    /* READY -> INITIALIZED */
    status = dl_state_set(DL_STATE_INITIALIZED);
    TEST_ASSERT(status == DL_STATUS_SUCCESS &&
                dl_state_get() == DL_STATE_INITIALIZED,
                "READY -> INITIALIZED");

    /* INITIALIZED -> UNINITIALIZED */
    status = dl_state_set(DL_STATE_UNINITIALIZED);
    TEST_ASSERT(status == DL_STATUS_SUCCESS &&
                dl_state_get() == DL_STATE_UNINITIALIZED,
                "INITIALIZED -> UNINITIALIZED");
}

void test_invalid_transitions(void)
{
    TEST_START("Invalid State Transitions");

    dl_state_init();
    dl_status_t status;

    /* UNINITIALIZED -> READY (invalid) */
    status = dl_state_set(DL_STATE_READY);
    TEST_ASSERT(status != DL_STATUS_SUCCESS &&
                dl_state_get() == DL_STATE_UNINITIALIZED,
                "UNINITIALIZED -> READY should fail");

    /* Set to INITIALIZED for next test */
    dl_state_set(DL_STATE_INITIALIZED);

    /* INITIALIZED -> RUNNING (invalid) */
    status = dl_state_set(DL_STATE_RUNNING);
    TEST_ASSERT(status != DL_STATUS_SUCCESS &&
                dl_state_get() == DL_STATE_INITIALIZED,
                "INITIALIZED -> RUNNING should fail");
}

void test_error_state_transitions(void)
{
    TEST_START("Error State Transitions");

    dl_state_init();
    dl_status_t status;

    /* Set to RUNNING */
    dl_state_set(DL_STATE_INITIALIZED);
    dl_state_set(DL_STATE_READY);
    dl_state_set(DL_STATE_RUNNING);

    /* Any state -> ERROR */
    status = dl_state_set(DL_STATE_ERROR);
    TEST_ASSERT(status == DL_STATUS_SUCCESS &&
                dl_state_get() == DL_STATE_ERROR,
                "RUNNING -> ERROR");

    /* ERROR -> INITIALIZED (recovery) */
    status = dl_state_set(DL_STATE_INITIALIZED);
    TEST_ASSERT(status == DL_STATUS_SUCCESS &&
                dl_state_get() == DL_STATE_INITIALIZED,
                "ERROR -> INITIALIZED (recovery)");
}

void test_callback_registration(void)
{
    TEST_START("State Change Callback");

    dl_state_init();
    reset_callback_vars();

    /* Register callback */
    dl_status_t status = dl_state_register_callback(test_state_change_callback);
    TEST_ASSERT(status == DL_STATUS_SUCCESS,
                "Callback registration");

    /* Trigger state change */
    dl_state_set(DL_STATE_INITIALIZED);

    TEST_ASSERT(callback_invoked == true,
                "Callback was invoked");
    TEST_ASSERT(callback_old_state == DL_STATE_UNINITIALIZED,
                "Callback old state is correct");
    TEST_ASSERT(callback_new_state == DL_STATE_INITIALIZED,
                "Callback new state is correct");

    /* Unregister callback */
    reset_callback_vars();
    dl_state_register_callback(NULL);
    dl_state_set(DL_STATE_READY);

    TEST_ASSERT(callback_invoked == false,
                "Callback not invoked after unregistration");
}

void test_transition_validation(void)
{
    TEST_START("Transition Validation");

    bool valid;

    /* Valid transitions */
    valid = dl_state_is_transition_valid(DL_STATE_UNINITIALIZED, DL_STATE_INITIALIZED);
    TEST_ASSERT(valid == true,
                "UNINITIALIZED -> INITIALIZED is valid");

    valid = dl_state_is_transition_valid(DL_STATE_INITIALIZED, DL_STATE_READY);
    TEST_ASSERT(valid == true,
                "INITIALIZED -> READY is valid");

    valid = dl_state_is_transition_valid(DL_STATE_READY, DL_STATE_RUNNING);
    TEST_ASSERT(valid == true,
                "READY -> RUNNING is valid");

    /* Invalid transitions */
    valid = dl_state_is_transition_valid(DL_STATE_UNINITIALIZED, DL_STATE_READY);
    TEST_ASSERT(valid == false,
                "UNINITIALIZED -> READY is invalid");

    valid = dl_state_is_transition_valid(DL_STATE_INITIALIZED, DL_STATE_RUNNING);
    TEST_ASSERT(valid == false,
                "INITIALIZED -> RUNNING is invalid");

    valid = dl_state_is_transition_valid(DL_STATE_RUNNING, DL_STATE_INITIALIZED);
    TEST_ASSERT(valid == false,
                "RUNNING -> INITIALIZED is invalid");
}

/* ========================================================================== */
/* Main Test Runner                                                           */
/* ========================================================================== */

int main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("DLL State Machine Unit Tests\n");
    printf("========================================\n");

    /* Run all tests */
    test_state_init();
    test_state_get_name();
    test_valid_transitions();
    test_invalid_transitions();
    test_error_state_transitions();
    test_callback_registration();
    test_transition_validation();

    /* Print summary */
    printf("\n");
    printf("========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Total tests:  %d\n", tests_run);
    printf("Passed:       %d\n", tests_passed);
    printf("Failed:       %d\n", tests_failed);
    printf("========================================\n");

    if (tests_failed == 0) {
        printf("\nAll tests PASSED!\n\n");
        return 0;
    } else {
        printf("\nSome tests FAILED!\n\n");
        return 1;
    }
}
